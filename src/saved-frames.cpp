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
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <cstring>
#include <map>
#include <mutex>
#include <vector>

namespace {

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

QPixmap framed(const QImage &src, const QString &badge)
{
	QPixmap pm(96, 64);
	pm.fill(Qt::black);
	QPainter p(&pm);
	if (!src.isNull()) {
		const QImage scaled = src.scaled(96, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		p.drawImage((96 - scaled.width()) / 2, (64 - scaled.height()) / 2, scaled);
	}
	QFont font = p.font();
	font.setPixelSize(10);
	font.setBold(true);
	p.setFont(font);
	const int bw = p.fontMetrics().horizontalAdvance(badge) + 8;
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0, 0, 0, 170));
	p.drawRect(0, 48, bw, 16);
	p.setPen(Qt::white);
	p.drawText(4, 60, badge);
	return pm;
}

class FrameTile : public QWidget {
public:
	FrameTile(const QImage &image, const QString &badge, bool can_remove, QWidget *parent) : QWidget(parent)
	{
		setFixedSize(104, 72);
		auto *pic = new QLabel(this);
		pic->setPixmap(framed(image, badge));
		pic->setGeometry(4, 4, 96, 64);
		if (!can_remove)
			return;
		remove_ = new QPushButton(QString(QChar(0x00D7)), this);
		remove_->setFixedSize(16, 16);
		remove_->move(84, 2);
		remove_->setToolTip(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SavedFrames.Remove")));
		remove_->raise();
	}

	QPushButton *remove_ = nullptr;
};

class SavedFramesDialog : public QDialog {
public:
	explicit SavedFramesDialog(hud_mask *ctx, QWidget *parent) : QDialog(parent), ctx_(ctx)
	{
		setWindowTitle(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SavedFrames.Title")));
		setWindowModality(Qt::NonModal);
		setAttribute(Qt::WA_QuitOnClose, false);
		resize(460, 168);

		save_show_ = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SaveShow")), this);
		save_hide_ = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SaveHide")), this);
		count_ = new QLabel(this);

		auto *row = new QHBoxLayout();
		row->addWidget(save_show_);
		row->addWidget(save_hide_);
		row->addStretch();
		row->addWidget(count_);

		scroll_ = new QScrollArea(this);
		scroll_->setWidgetResizable(true);
		scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		scroll_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		scroll_->setFixedHeight(92);
		strip_ = new QWidget();
		thumbs_ = new QHBoxLayout(strip_);
		thumbs_->setAlignment(Qt::AlignLeft);
		thumbs_->setContentsMargins(4, 4, 4, 4);
		scroll_->setWidget(strip_);

		auto *root = new QVBoxLayout(this);
		root->addLayout(row);
		root->addWidget(scroll_);

		connect(save_show_, &QPushButton::clicked, this, [this] { queue(hud_mask_mark::show); });
		connect(save_hide_, &QPushButton::clicked, this, [this] { queue(hud_mask_mark::hide); });

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

private:
	void queue(hud_mask_mark mark)
	{
		if (!ctx_ || removing_)
			return;
		hud_mask_queue_mark(ctx_, mark);
		tick();
	}

	void on_remove(int index)
	{
		if (!ctx_ || removing_)
			return;
		removing_ = true;
		hud_mask_presence_drop_extra(ctx_, index);
		QTimer::singleShot(0, this, [this] { tick(); });
	}

	void clear_tiles()
	{
		while (QLayoutItem *item = thumbs_->takeAt(0)) {
			delete item->widget();
			delete item;
		}
	}

	void rebuild()
	{
		clear_tiles();
		if (!ctx_)
			return;
		hud_mask_still apply;
		std::vector<hud_mask_still> extras;
		bool valid = false;
		{
			std::lock_guard<std::mutex> lock(ctx_->data_mu);
			valid = ctx_->ref_valid && !ctx_->ref_luma.empty();
			if (valid) {
				apply.w = ctx_->ref_w;
				apply.h = ctx_->ref_h;
				apply.luma = ctx_->ref_luma;
				extras = ctx_->ref_extra;
			}
		}
		if (!valid)
			return;
		add_tile(apply, QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SavedFrames.Apply")), -1);
		for (int i = 0; i < static_cast<int>(extras.size()); i++) {
			const char *key = extras[static_cast<size_t>(i)].hide ? "HUDMask.AutoHide.SavedFrames.Hide"
									       : "HUDMask.AutoHide.SavedFrames.Show";
			add_tile(extras[static_cast<size_t>(i)], QString::fromUtf8(obs_module_text(key)), i);
		}
		thumbs_->addStretch();
	}

	void add_tile(const hud_mask_still &still, const QString &badge, int extra_index)
	{
		auto *tile = new FrameTile(luma_image(still.luma, still.w, still.h), badge, extra_index >= 0, strip_);
		if (tile->remove_) {
			const int index = extra_index;
			connect(tile->remove_, &QPushButton::clicked, this, [this, index] { on_remove(index); });
		}
		thumbs_->addWidget(tile);
	}

	void tick()
	{
		if (!ctx_)
			return;
		uint32_t gen = 0;
		bool can_save = false;
		int count = 0;
		{
			std::lock_guard<std::mutex> lock(ctx_->data_mu);
			gen = ctx_->ref_gen;
			can_save = ctx_->auto_hide && ctx_->ref_valid && !ctx_->mask_path.empty() &&
				   ctx_->mark_pending == hud_mask_mark::none;
			if (ctx_->ref_valid)
				count = 1 + static_cast<int>(ctx_->ref_extra.size());
		}
		count_->setText(QString::fromUtf8(obs_module_text("HUDMask.AutoHide.SavedFrames.Count"))
					.arg(count)
					.arg(k_mask_presence_still_cap));
		if (!built_ || gen != seen_gen_ || removing_) {
			built_ = true;
			seen_gen_ = gen;
			removing_ = false;
			rebuild();
		}
		save_show_->setEnabled(can_save);
		save_hide_->setEnabled(can_save);
	}

	hud_mask *ctx_ = nullptr;
	QPushButton *save_show_ = nullptr;
	QPushButton *save_hide_ = nullptr;
	QLabel *count_ = nullptr;
	QScrollArea *scroll_ = nullptr;
	QWidget *strip_ = nullptr;
	QHBoxLayout *thumbs_ = nullptr;
	QTimer *timer_ = nullptr;
	uint32_t seen_gen_ = 0;
	bool built_ = false;
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
