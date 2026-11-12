/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "saved-frames.hpp"
#include "hud-mask.hpp"
#include "mask-process.hpp"
#include "presence.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace {

constexpr int k_thumb_w = 140;
constexpr int k_thumb_h = 96;

std::map<hud_mask *, class SavedFramesDialog *> g_dialogs;

QImage luma_image(const std::vector<uint8_t> &luma, int w, int h)
{
	if (w < 1 || h < 1 || luma.size() < static_cast<size_t>(w) * static_cast<size_t>(h))
		return {};
	QImage img(w, h, QImage::Format_Grayscale8);
	for (int y = 0; y < h; y++)
		memcpy(img.scanLine(y), luma.data() + static_cast<size_t>(y) * w, static_cast<size_t>(w));
	return img.convertToFormat(QImage::Format_RGB32);
}

QPixmap thumb_pixmap(const QImage &src, bool selected)
{
	QPixmap pm(k_thumb_w, k_thumb_h);
	pm.fill(Qt::black);
	QPainter p(&pm);
	if (!src.isNull()) {
		const QImage scaled =
			src.scaled(k_thumb_w - 6, k_thumb_h - 6, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		p.drawImage((k_thumb_w - scaled.width()) / 2, (k_thumb_h - scaled.height()) / 2, scaled);
	}
	if (selected) {
		p.setPen(QPen(Qt::white, 2));
		p.setBrush(Qt::NoBrush);
		p.drawRect(1, 1, k_thumb_w - 3, k_thumb_h - 3);
	}
	return pm;
}

class Preview : public QWidget {
public:
	explicit Preview(QWidget *parent) : QWidget(parent)
	{
		setMinimumSize(280, 220);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	}

	void set_image(const QImage &image)
	{
		image_ = image;
		update();
	}

	QSize sizeHint() const override { return QSize(280, 220); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.fillRect(rect(), Qt::black);
		if (image_.isNull() || width() < 2 || height() < 2)
			return;
		const QImage scaled = image_.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
		p.drawImage((width() - scaled.width()) / 2, (height() - scaled.height()) / 2, scaled);
	}

private:
	QImage image_;
};

class SnapTile : public QWidget {
public:
	SnapTile(int index, const QImage &image, const QString &caption, QWidget *parent)
		: QWidget(parent), index_(index), image_(image)
	{
		setFixedWidth(k_thumb_w);
		auto *col = new QVBoxLayout(this);
		col->setContentsMargins(0, 0, 0, 0);
		col->setSpacing(2);
		pic_ = new QLabel(this);
		pic_->setFixedSize(k_thumb_w, k_thumb_h);
		pic_->setAttribute(Qt::WA_TransparentForMouseEvents);
		pic_->setPixmap(thumb_pixmap(image_, false));
		auto *cap = new QLabel(caption, this);
		cap->setAlignment(Qt::AlignHCenter);
		cap->setAttribute(Qt::WA_TransparentForMouseEvents);
		col->addWidget(pic_);
		col->addWidget(cap);
	}

	int index() const { return index_; }

	void set_selected(bool on)
	{
		if (on == selected_)
			return;
		selected_ = on;
		pic_->setPixmap(thumb_pixmap(image_, on));
	}

	std::function<void(int)> on_click;

protected:
	void mousePressEvent(QMouseEvent *event) override
	{
		if (event->button() == Qt::LeftButton && on_click)
			on_click(index_);
		QWidget::mousePressEvent(event);
	}

private:
	int index_ = -1;
	bool selected_ = false;
	QImage image_;
	QLabel *pic_ = nullptr;
};

class SavedFramesDialog : public QDialog {
public:
	explicit SavedFramesDialog(hud_mask *ctx, QWidget *parent) : QDialog(parent), ctx_(ctx)
	{
		setWindowTitle(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SavedFrames.Title")));
		setWindowModality(Qt::NonModal);
		setAttribute(Qt::WA_QuitOnClose, false);
		resize(680, 420);
		setMinimumSize(520, 360);

		save_show_ = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SaveShow")), this);
		save_hide_ = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SaveHide")), this);
		delete_ = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SavedFrames.Delete")),
					   this);
		count_ = new QLabel(this);
		preview_ = new Preview(this);
		caption_ = new QLabel(this);
		caption_->setWordWrap(true);
		caption_->setMaximumHeight(48);

		auto *controls = new QVBoxLayout();
		controls->addWidget(save_show_);
		controls->addWidget(save_hide_);
		controls->addWidget(delete_);
		controls->addWidget(count_);
		controls->addStretch();

		auto *preview_col = new QVBoxLayout();
		preview_col->addWidget(preview_, 1);
		preview_col->addWidget(caption_);

		auto *top = new QHBoxLayout();
		top->addLayout(controls);
		top->addLayout(preview_col, 1);

		scroll_ = new QScrollArea(this);
		scroll_->setWidgetResizable(true);
		scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scroll_->setFixedHeight(128);
		strip_ = new QWidget();
		thumbs_ = new QHBoxLayout(strip_);
		thumbs_->setAlignment(Qt::AlignLeft);
		thumbs_->setContentsMargins(4, 4, 4, 4);
		scroll_->setWidget(strip_);

		auto *root = new QVBoxLayout(this);
		root->addLayout(top, 1);
		root->addWidget(scroll_);

		connect(save_show_, &QPushButton::clicked, this, [this] { queue(hud_mask_mark::show); });
		connect(save_hide_, &QPushButton::clicked, this, [this] { queue(hud_mask_mark::hide); });
		connect(delete_, &QPushButton::clicked, this, [this] { on_delete(); });

		timer_ = new QTimer(this);
		connect(timer_, &QTimer::timeout, this, [this] { tick(); });
		timer_->start(200);
		tick();
	}

	~SavedFramesDialog() override
	{
		detach();
		forget();
	}

	void detach()
	{
		if (timer_)
			timer_->stop();
		ctx_ = nullptr;
	}

	void forget()
	{
		for (auto it = g_dialogs.begin(); it != g_dialogs.end();) {
			if (it->second == this)
				it = g_dialogs.erase(it);
			else
				++it;
		}
	}

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QDialog::resizeEvent(event);
		if (preview_)
			preview_->update();
	}

private:
	void queue(hud_mask_mark mark)
	{
		if (!ctx_ || removing_)
			return;
		hud_mask_queue_mark(ctx_, mark);
		tick();
	}

	void on_delete()
	{
		if (!ctx_ || removing_ || sel_ < 0)
			return;
		removing_ = true;
		delete_->setEnabled(false);
		const int index = sel_;
		sel_ = -1;
		hud_mask_presence_drop_extra(ctx_, index);
		QTimer::singleShot(0, this, [this] { tick(); });
	}

	void select_snap(int index)
	{
		sel_ = (sel_ == index) ? -1 : index;
		mark_selection();
		delete_->setEnabled(sel_ >= 0);
		show_selected();
	}

	void mark_selection()
	{
		for (SnapTile *tile : tiles_)
			tile->set_selected(tile->index() == sel_);
	}

	void clear_tiles()
	{
		tiles_.clear();
		while (QLayoutItem *item = thumbs_->takeAt(0)) {
			delete item->widget();
			delete item;
		}
	}

	void add_tile(const hud_mask_still &still, int index)
	{
		const char *key = still.hide ? "HUDMask.AutoHide.SavedFrames.Hide"
					     : "HUDMask.AutoHide.SavedFrames.Show";
		auto *tile = new SnapTile(index, luma_image(still.luma, still.w, still.h),
					  QString::fromUtf8(obs_module_text(key)), strip_);
		tile->on_click = [this](int i) { select_snap(i); };
		tile->set_selected(index == sel_);
		tiles_.push_back(tile);
		thumbs_->addWidget(tile);
	}

	void rebuild()
	{
		clear_tiles();
		if (!ctx_)
			return;
		std::vector<hud_mask_still> extras;
		{
			std::lock_guard<std::mutex> lock(ctx_->data_mu);
			extras = ctx_->ref_extra;
		}
		for (int i = 0; i < static_cast<int>(extras.size()); i++)
			add_tile(extras[static_cast<size_t>(i)], i);
		thumbs_->addStretch();
	}

	void show_selected()
	{
		if (!ctx_ || !preview_ || !caption_)
			return;
		std::vector<uint8_t> luma;
		int w = 0;
		int h = 0;
		bool hide = false;
		bool ok = false;
		const bool live = sel_ < 0;
		{
			std::lock_guard<std::mutex> lock(ctx_->data_mu);
			if (live) {
				if (ctx_->live_w >= 1 && ctx_->live_h >= 1 && !ctx_->live_luma.empty()) {
					luma = ctx_->live_luma;
					w = ctx_->live_w;
					h = ctx_->live_h;
					ok = true;
				}
			} else if (sel_ < static_cast<int>(ctx_->ref_extra.size())) {
				const hud_mask_still &st = ctx_->ref_extra[static_cast<size_t>(sel_)];
				luma = st.luma;
				w = st.w;
				h = st.h;
				hide = st.hide;
				ok = true;
			}
		}
		const char *key = "HUDMask.AutoHide.SavedFrames.Waiting";
		if (ok && live)
			key = "HUDMask.AutoHide.SavedFrames.Live";
		else if (ok && hide)
			key = "HUDMask.AutoHide.SavedFrames.SavedHide";
		else if (ok)
			key = "HUDMask.AutoHide.SavedFrames.SavedShow";
		caption_->setText(QString::fromUtf8(obs_module_text(key)));
		preview_->set_image(ok ? luma_image(luma, w, h) : QImage());
	}

	void tick()
	{
		if (!ctx_)
			return;
		uint32_t gen = 0;
		uint32_t live_gen = 0;
		uint32_t label_gen = 0;
		int last_labeled = -1;
		bool can_save = false;
		int extra_n = 0;
		{
			std::lock_guard<std::mutex> lock(ctx_->data_mu);
			gen = ctx_->ref_gen;
			live_gen = ctx_->live_gen;
			label_gen = ctx_->label_gen;
			last_labeled = ctx_->last_labeled;
			can_save = ctx_->auto_hide && ctx_->ref_valid && !ctx_->mask_path.empty() &&
				   ctx_->mark_pending == hud_mask_mark::none;
			extra_n = static_cast<int>(ctx_->ref_extra.size());
		}
		count_->setText(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SavedFrames.Count"))
					.arg(extra_n)
					.arg(k_mask_presence_still_cap - 1));
		bool pick_new = false;
		if (!label_ready_) {
			label_ready_ = true;
			seen_label_ = label_gen;
		} else if (label_gen != seen_label_) {
			seen_label_ = label_gen;
			sel_ = last_labeled;
			pick_new = true;
		}
		if (sel_ >= extra_n)
			sel_ = -1;
		const bool gen_changed = !built_ || gen != seen_gen_ || removing_;
		if (gen_changed) {
			built_ = true;
			seen_gen_ = gen;
			removing_ = false;
			rebuild();
		} else if (pick_new) {
			mark_selection();
		}
		if (gen_changed || pick_new || (sel_ < 0 && live_gen != seen_live_))
			show_selected();
		seen_live_ = live_gen;
		delete_->setEnabled(sel_ >= 0);
		save_show_->setEnabled(can_save);
		save_hide_->setEnabled(can_save);
	}

	hud_mask *ctx_ = nullptr;
	QPushButton *save_show_ = nullptr;
	QPushButton *save_hide_ = nullptr;
	QPushButton *delete_ = nullptr;
	QLabel *count_ = nullptr;
	QLabel *caption_ = nullptr;
	Preview *preview_ = nullptr;
	QScrollArea *scroll_ = nullptr;
	QWidget *strip_ = nullptr;
	QHBoxLayout *thumbs_ = nullptr;
	QTimer *timer_ = nullptr;
	std::vector<SnapTile *> tiles_;
	int sel_ = -1;
	uint32_t seen_gen_ = 0;
	uint32_t seen_live_ = 0;
	uint32_t seen_label_ = 0;
	bool built_ = false;
	bool label_ready_ = false;
	bool removing_ = false;
};

} // namespace

void hud_mask_open_saved_frames(hud_mask *ctx)
{
	if (!ctx)
		return;
	auto it = g_dialogs.find(ctx);
	if (it != g_dialogs.end() && it->second) {
		it->second->raise();
		it->second->activateWindow();
		return;
	}
	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	auto *dlg = new SavedFramesDialog(ctx, parent);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	g_dialogs[ctx] = dlg;
	dlg->show();
}

void hud_mask_saved_frames_close(hud_mask *ctx)
{
	auto it = g_dialogs.find(ctx);
	if (it == g_dialogs.end())
		return;
	SavedFramesDialog *dlg = it->second;
	g_dialogs.erase(it);
	if (!dlg)
		return;
	dlg->detach();
	dlg->close();
}
