/*
HUD Mask
Copyright (C) 2026 Milzstream <elliottquick@live.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "cutout-editor.hpp"
#include "hud-mask.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/graphics.h>
#include <util/platform.h>
#include <plugin-support.h>

#include <QCoreApplication>
#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QEvent>
#include <QLineF>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QPolygon>
#include <QPushButton>
#include <QRadialGradient>
#include <QSlider>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <utility>
#include <vector>

namespace {

struct CaptureJob {
	std::atomic<bool> cancelled{false};
	obs_weak_source_t *weak = nullptr;
	gs_texrender_t *texrender = nullptr;
	gs_stagesurf_t *stagesurf = nullptr;
	uint32_t cx = 0;
	uint32_t cy = 0;
	int stage = 0;
	bool loadExisting = false;
	std::function<void(QImage, bool)> onFrame;
	std::function<void()> onFail;
};

void capture_tick(void *param, float);

void capture_fail(CaptureJob *job)
{
	obs_log(LOG_WARNING, "cutout capture failed (stage %d size %ux%u)", job->stage, job->cx, job->cy);
	if (job->stagesurf) {
		gs_stagesurface_destroy(job->stagesurf);
		job->stagesurf = nullptr;
	}
	if (job->texrender) {
		gs_texrender_destroy(job->texrender);
		job->texrender = nullptr;
	}
	obs_remove_tick_callback(capture_tick, job);
	auto fail = job->onFail;
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[fail]() {
			if (fail)
				fail();
		},
		Qt::QueuedConnection);
}

void capture_tick(void *param, float)
{
	auto *job = static_cast<CaptureJob *>(param);
	if (job->cancelled.load())
		return;

	obs_enter_graphics();

	if (job->stage == 0) {
		obs_source_t *source = obs_weak_source_get_source(job->weak);
		if (!source) {
			capture_fail(job);
			obs_leave_graphics();
			return;
		}

		job->cx = obs_source_get_width(source);
		job->cy = obs_source_get_height(source);
		if (!job->cx || !job->cy) {
			obs_source_release(source);
			capture_fail(job);
			obs_leave_graphics();
			return;
		}

		const enum gs_color_space space = GS_CS_SRGB;
		const enum gs_color_format format = gs_get_format_from_space(space);
		job->texrender = gs_texrender_create(format, GS_ZS_NONE);
		job->stagesurf = gs_stagesurface_create(job->cx, job->cy, format);

		if (gs_texrender_begin_with_color_space(job->texrender, job->cx, job->cy, space)) {
			struct vec4 zero;
			vec4_zero(&zero);
			gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
			gs_ortho(0.0f, (float)job->cx, 0.0f, (float)job->cy, -100.0f, 100.0f);
			gs_blend_state_push();
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
			obs_source_inc_showing(source);
			obs_source_video_render(source);
			obs_source_dec_showing(source);
			gs_blend_state_pop();
			gs_texrender_end(job->texrender);
		}
		obs_source_release(source);
	} else if (job->stage == 1) {
		gs_texture_t *tex = job->texrender ? gs_texrender_get_texture(job->texrender) : nullptr;
		if (!tex || !job->stagesurf) {
			capture_fail(job);
			obs_leave_graphics();
			return;
		}
		gs_stage_texture(job->stagesurf, tex);
	} else if (job->stage == 2) {
		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		QImage image;
		if (job->stagesurf && gs_stagesurface_map(job->stagesurf, &data, &linesize)) {
			image = QImage((int)job->cx, (int)job->cy, QImage::Format_RGBX8888);
			for (uint32_t y = 0; y < job->cy; y++)
				memcpy(image.scanLine((int)y), data + y * linesize, (int)job->cx * 4);
			gs_stagesurface_unmap(job->stagesurf);
		}

		if (job->stagesurf) {
			gs_stagesurface_destroy(job->stagesurf);
			job->stagesurf = nullptr;
		}
		if (job->texrender) {
			gs_texrender_destroy(job->texrender);
			job->texrender = nullptr;
		}
		obs_remove_tick_callback(capture_tick, job);
		obs_leave_graphics();

		if (image.isNull()) {
			auto fail = job->onFail;
			QMetaObject::invokeMethod(
				QCoreApplication::instance(),
				[fail]() {
					if (fail)
						fail();
				},
				Qt::QueuedConnection);
			return;
		}

		const bool loadExisting = job->loadExisting;
		const QImage copy = image.copy();
		auto cb = job->onFrame;
		QMetaObject::invokeMethod(
			QCoreApplication::instance(),
			[cb, copy, loadExisting]() {
				if (cb)
					cb(copy, loadExisting);
			},
			Qt::QueuedConnection);
		return;
	}

	obs_leave_graphics();
	job->stage++;
}

class MaskCanvas : public QWidget {
public:
	QImage frame;
	QImage mask;
	int brush = 28;
	bool erase = false;

	explicit MaskCanvas(QWidget *parent = nullptr) : QWidget(parent)
	{
		setMouseTracking(true);
		setMinimumSize(640, 360);
		setCursor(Qt::CrossCursor);
	}

	void setFrame(const QImage &img)
	{
		frame = img.convertToFormat(QImage::Format_ARGB32);
		if (mask.size() != frame.size()) {
			mask = QImage(frame.size(), QImage::Format_Grayscale8);
			mask.fill(0);
		}
		update();
	}

	void loadExistingMask(const QImage &cropMask, int left, int top)
	{
		if (mask.isNull() || cropMask.isNull())
			return;
		QPainter p(&mask);
		p.drawImage(left, top, cropMask.convertToFormat(QImage::Format_Grayscale8));
		update();
	}

	void clearMask()
	{
		if (!mask.isNull())
			mask.fill(0);
		update();
	}

	bool snapToEdges()
	{
		if (frame.isNull() || mask.isNull())
			return false;

		const int w = mask.width();
		const int h = mask.height();
		if (w < 8 || h < 8)
			return false;

		std::vector<uint8_t> user(static_cast<size_t>(w) * h, 0);
		int painted = 0;
		for (int y = 0; y < h; y++) {
			const uint8_t *row = mask.constScanLine(y);
			for (int x = 0; x < w; x++) {
				if (row[x] >= 40) {
					user[static_cast<size_t>(y) * w + x] = 1;
					painted++;
				}
			}
		}
		if (painted < 40)
			return false;

		std::vector<uint8_t> lum(static_cast<size_t>(w) * h);
		QImage rgb = frame.convertToFormat(QImage::Format_ARGB32);
		for (int y = 0; y < h; y++) {
			const QRgb *row = reinterpret_cast<const QRgb *>(rgb.constScanLine(y));
			for (int x = 0; x < w; x++) {
				const QRgb p = row[x];
				lum[static_cast<size_t>(y) * w + x] =
					static_cast<uint8_t>((77 * qRed(p) + 150 * qGreen(p) + 29 * qBlue(p)) >> 8);
			}
		}

		std::vector<uint16_t> mag(static_cast<size_t>(w) * h, 0);
		int maxMag = 1;
		for (int y = 1; y < h - 1; y++) {
			for (int x = 1; x < w - 1; x++) {
				const int i = y * w + x;
				const int gx = -lum[i - w - 1] - 2 * lum[i - 1] - lum[i + w - 1] + lum[i - w + 1] +
					       2 * lum[i + 1] + lum[i + w + 1];
				const int gy = -lum[i - w - 1] - 2 * lum[i - w] - lum[i - w + 1] + lum[i + w - 1] +
					       2 * lum[i + w] + lum[i + w + 1];
				const int m = std::abs(gx) + std::abs(gy);
				mag[i] = static_cast<uint16_t>(m);
				maxMag = std::max(maxMag, m);
			}
		}

		std::vector<int> label(static_cast<size_t>(w) * h, 0);
		int nlab = 0;
		std::vector<int> stack;
		stack.reserve(1024);
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				const int start = y * w + x;
				if (!user[start] || label[start])
					continue;
				nlab++;
				stack.clear();
				stack.push_back(start);
				label[start] = nlab;
				while (!stack.empty()) {
					const int i = stack.back();
					stack.pop_back();
					const int px = i % w;
					const int py = i / w;
					const int nb[4] = {px > 0 ? i - 1 : -1, px + 1 < w ? i + 1 : -1,
							   py > 0 ? i - w : -1, py + 1 < h ? i + w : -1};
					for (int n : nb) {
						if (n < 0 || !user[n] || label[n])
							continue;
						label[n] = nlab;
						stack.push_back(n);
					}
				}
			}
		}

		QImage snapped(w, h, QImage::Format_Grayscale8);
		snapped.fill(0);
		QPainter fill(&snapped);
		fill.setRenderHint(QPainter::Antialiasing, true);
		fill.setPen(Qt::NoPen);
		fill.setBrush(Qt::white);

		bool any = false;
		for (int lab = 1; lab <= nlab; lab++) {
			double sx = 0, sy = 0;
			int count = 0;
			double rmax = 0;
			for (int i = 0; i < w * h; i++) {
				if (label[i] != lab)
					continue;
				const int x = i % w;
				const int y = i / w;
				sx += x;
				sy += y;
				count++;
			}
			if (count < 40)
				continue;
			sx /= count;
			sy /= count;
			for (int i = 0; i < w * h; i++) {
				if (label[i] != lab)
					continue;
				const double dx = (i % w) - sx;
				const double dy = (i / w) - sy;
				rmax = std::max(rmax, std::sqrt(dx * dx + dy * dy));
			}
			if (rmax < 6)
				continue;

			const int nrays = 256;
			QPolygon poly;
			poly.reserve(nrays);
			for (int r = 0; r < nrays; r++) {
				const double ang = (2.0 * 3.14159265358979323846 * r) / nrays;
				const double c = std::cos(ang);
				const double s = std::sin(ang);
				int bestT = static_cast<int>(rmax);
				int bestScore = -1;
				const int t0 = std::max(2, static_cast<int>(rmax * 0.28));
				const int t1 = static_cast<int>(rmax * 1.22);
				for (int t = t0; t <= t1; t++) {
					const int x = static_cast<int>(std::lround(sx + t * c));
					const int y = static_cast<int>(std::lround(sy + t * s));
					if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1)
						break;
					int score = mag[y * w + x];
					if (!user[y * w + x] && t > static_cast<int>(rmax))
						score /= 2;
					if (score > bestScore) {
						bestScore = score;
						bestT = t;
					}
				}
				if (bestScore < maxMag / 14)
					bestT = static_cast<int>(rmax);
				poly << QPoint(static_cast<int>(std::lround(sx + bestT * c)),
					       static_cast<int>(std::lround(sy + bestT * s)));
			}
			fill.drawPolygon(poly);
			any = true;
		}
		fill.end();

		if (!any)
			return false;

		std::vector<uint8_t> bin(static_cast<size_t>(w) * h, 0);
		for (int y = 0; y < h; y++) {
			const uint8_t *row = snapped.constScanLine(y);
			memcpy(bin.data() + static_cast<size_t>(y) * w, row, w);
		}

		const int feather = 6;
		std::vector<int> dist(static_cast<size_t>(w) * h, 9999);
		stack.clear();
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				const int i = y * w + x;
				if (bin[i] < 128)
					continue;
				bool border = x == 0 || y == 0 || x == w - 1 || y == h - 1;
				if (!border) {
					border = bin[i - 1] < 128 || bin[i + 1] < 128 || bin[i - w] < 128 ||
						 bin[i + w] < 128;
				}
				if (border) {
					dist[i] = 0;
					stack.push_back(i);
				}
			}
		}
		for (size_t qi = 0; qi < stack.size(); qi++) {
			const int i = stack[qi];
			const int nd = dist[i] + 1;
			if (nd > feather)
				continue;
			const int px = i % w;
			const int py = i / w;
			const int nb[4] = {px > 0 ? i - 1 : -1, px + 1 < w ? i + 1 : -1, py > 0 ? i - w : -1,
					   py + 1 < h ? i + w : -1};
			for (int n : nb) {
				if (n < 0 || bin[n] < 128 || dist[n] <= nd)
					continue;
				dist[n] = nd;
				stack.push_back(n);
			}
		}

		for (int y = 0; y < h; y++) {
			uint8_t *row = mask.scanLine(y);
			for (int x = 0; x < w; x++) {
				const int i = y * w + x;
				if (bin[i] < 128) {
					row[x] = 0;
					continue;
				}
				const int d = dist[i];
				if (d >= feather)
					row[x] = 255;
				else
					row[x] = static_cast<uint8_t>(d * 255 / feather);
			}
		}

		update();
		return true;
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.fillRect(rect(), QColor(20, 20, 20));
		if (frame.isNull())
			return;

		const QRect dest = fitted();
		p.drawImage(dest, frame);

		if (!mask.isNull()) {
			QImage overlay(mask.size(), QImage::Format_ARGB32);
			overlay.fill(Qt::transparent);
			for (int y = 0; y < mask.height(); y++) {
				const uint8_t *src = mask.constScanLine(y);
				QRgb *dst = reinterpret_cast<QRgb *>(overlay.scanLine(y));
				const uint8_t *up = y > 0 ? mask.constScanLine(y - 1) : nullptr;
				const uint8_t *dn = y + 1 < mask.height() ? mask.constScanLine(y + 1) : nullptr;
				for (int x = 0; x < mask.width(); x++) {
					if (!src[x])
						continue;
					const bool edge = x == 0 || x == mask.width() - 1 || !up || !dn ||
							  src[x - 1] < 20 || src[x + 1] < 20 || up[x] < 20 ||
							  dn[x] < 20;
					if (edge)
						dst[x] = qRgba(255, 255, 255, 230);
					else
						dst[x] = qRgba(255, 60, 180, src[x] * 90 / 255);
				}
			}
			p.setCompositionMode(QPainter::CompositionMode_SourceOver);
			p.drawImage(dest, overlay);
		}

		if (cursorOn_) {
			p.setPen(QPen(erase ? QColor(255, 80, 80) : QColor(255, 230, 80), 1));
			p.setBrush(Qt::NoBrush);
			const QPoint c = srcToWidget(cursorSrc_);
			p.drawEllipse(c, srcToWidgetLen(brush), srcToWidgetLen(brush));
		}
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton) {
			painting_ = true;
			lastSrc_ = widgetToSrc(e->pos());
			stamp(lastSrc_);
		}
	}

	void mouseMoveEvent(QMouseEvent *e) override
	{
		cursorOn_ = true;
		cursorSrc_ = widgetToSrc(e->pos());
		if (painting_ && (e->buttons() & Qt::LeftButton)) {
			const QPointF now = cursorSrc_;
			stroke(lastSrc_, now);
			lastSrc_ = now;
		}
		update();
	}

	void mouseReleaseEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton)
			painting_ = false;
	}

	void leaveEvent(QEvent *) override
	{
		cursorOn_ = false;
		update();
	}

	void wheelEvent(QWheelEvent *e) override
	{
		brush = std::clamp(brush + (e->angleDelta().y() > 0 ? 2 : -2), 4, 96);
		update();
	}

private:
	QRect fitted() const
	{
		if (frame.isNull())
			return {};
		const QSize avail = size();
		QSize img = frame.size();
		img.scale(avail, Qt::KeepAspectRatio);
		const int x = (avail.width() - img.width()) / 2;
		const int y = (avail.height() - img.height()) / 2;
		return {x, y, img.width(), img.height()};
	}

	QPointF widgetToSrc(const QPoint &p) const
	{
		const QRect d = fitted();
		if (d.width() <= 0 || d.height() <= 0 || frame.isNull())
			return {};
		const double x = (p.x() - d.x()) * (double)frame.width() / d.width();
		const double y = (p.y() - d.y()) * (double)frame.height() / d.height();
		return {x, y};
	}

	QPoint srcToWidget(const QPointF &s) const
	{
		const QRect d = fitted();
		if (frame.isNull() || frame.width() == 0)
			return {};
		const int x = d.x() + static_cast<int>(s.x() * d.width() / frame.width());
		const int y = d.y() + static_cast<int>(s.y() * d.height() / frame.height());
		return {x, y};
	}

	int srcToWidgetLen(int src) const
	{
		const QRect d = fitted();
		if (frame.isNull() || frame.width() == 0)
			return src;
		return std::max(1, src * d.width() / frame.width());
	}

	void stamp(const QPointF &src)
	{
		if (mask.isNull())
			return;
		QPainter p(&mask);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setPen(Qt::NoPen);
		if (erase) {
			p.setCompositionMode(QPainter::CompositionMode_Source);
			p.setBrush(Qt::black);
			p.drawEllipse(src, brush, brush);
		} else {
			QRadialGradient g(src, brush);
			g.setColorAt(0.0, Qt::white);
			g.setColorAt(0.55, Qt::white);
			g.setColorAt(1.0, QColor(255, 255, 255, 0));
			p.setBrush(g);
			p.drawEllipse(src, brush, brush);
		}
	}

	void stroke(const QPointF &from, const QPointF &to)
	{
		const QLineF line(from, to);
		const int steps = std::max(1, static_cast<int>(line.length() / std::max(1, brush / 3)));
		for (int i = 0; i <= steps; i++)
			stamp(line.pointAt(i / static_cast<double>(steps)));
	}

	bool painting_ = false;
	bool cursorOn_ = false;
	QPointF lastSrc_;
	QPointF cursorSrc_;
};

class CutoutDialog : public QDialog {
public:
	CutoutDialog(hud_mask *ctx, QWidget *parent) : QDialog(parent), ctx_(ctx)
	{
		setWindowTitle(QString::fromUtf8(obs_module_text("HUDMask.Editor.Title")));
		resize(1100, 720);

		canvas_ = new MaskCanvas(this);

		auto *highlight = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Highlight")), this);
		auto *erase = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Erase")), this);
		highlight->setCheckable(true);
		erase->setCheckable(true);
		highlight->setChecked(true);

		auto *brushLabel = new QLabel(this);
		auto *brush = new QSlider(Qt::Horizontal, this);
		brush->setRange(4, 96);
		brush->setValue(canvas_->brush);

		auto *snap = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Snap")), this);
		auto *refresh = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Refresh")), this);
		auto *clear = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Clear")), this);
		auto *ok = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Apply")), this);
		auto *cancel = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Cancel")), this);
		auto *status = new QLabel(QString::fromUtf8(obs_module_text("HUDMask.Editor.Hint")), this);
		status->setWordWrap(true);

		auto updateBrushLabel = [brushLabel, brush]() {
			brushLabel->setText(QString::fromUtf8(obs_module_text("HUDMask.Editor.Brush")) +
					    QString("  %1").arg(brush->value()));
		};
		updateBrushLabel();

		connect(highlight, &QPushButton::clicked, this, [this, highlight, erase]() {
			canvas_->erase = false;
			highlight->setChecked(true);
			erase->setChecked(false);
			canvas_->update();
		});
		connect(erase, &QPushButton::clicked, this, [this, highlight, erase]() {
			canvas_->erase = true;
			erase->setChecked(true);
			highlight->setChecked(false);
			canvas_->update();
		});
		connect(brush, &QSlider::valueChanged, this, [this, updateBrushLabel](int v) {
			canvas_->brush = v;
			updateBrushLabel();
			canvas_->update();
		});
		connect(snap, &QPushButton::clicked, this, [this, status]() {
			if (canvas_->snapToEdges())
				status->setText(QString::fromUtf8(obs_module_text("HUDMask.Editor.Snapped")));
			else
				status->setText(QString::fromUtf8(obs_module_text("HUDMask.Editor.SnapFailed")));
		});
		connect(refresh, &QPushButton::clicked, this, [this]() { startCapture(false); });
		connect(clear, &QPushButton::clicked, this, [this, status]() {
			canvas_->clearMask();
			status->setText(QString::fromUtf8(obs_module_text("HUDMask.Editor.Hint")));
		});
		connect(ok, &QPushButton::clicked, this, [this]() { applyAndClose(); });
		connect(cancel, &QPushButton::clicked, this, [this]() { reject(); });

		auto *tools = new QHBoxLayout();
		tools->addWidget(highlight);
		tools->addWidget(erase);
		tools->addWidget(brushLabel);
		tools->addWidget(brush, 1);
		tools->addWidget(snap);
		tools->addWidget(refresh);
		tools->addWidget(clear);
		tools->addStretch();
		tools->addWidget(ok);
		tools->addWidget(cancel);

		auto *root = new QVBoxLayout(this);
		root->addWidget(canvas_, 1);
		root->addWidget(status);
		root->addLayout(tools);

		startCapture(true);
	}

	~CutoutDialog() override { stopCapture(); }

private:
	void stopCapture()
	{
		if (!job_)
			return;
		job_->cancelled = true;
		obs_remove_tick_callback(capture_tick, job_);
		CaptureJob *j = job_;
		job_ = nullptr;
		obs_queue_task(
			OBS_TASK_GRAPHICS,
			[](void *p) {
				auto *job = static_cast<CaptureJob *>(p);
				if (job->stagesurf)
					gs_stagesurface_destroy(job->stagesurf);
				if (job->texrender)
					gs_texrender_destroy(job->texrender);
				if (job->weak)
					obs_weak_source_release(job->weak);
				delete job;
			},
			j, false);
	}

	void startCapture(bool loadExisting)
	{
		stopCapture();

		obs_source_t *target = hud_mask_get_target(ctx_);
		if (!target) {
			QMessageBox::warning(this, windowTitle(),
					     QString::fromUtf8(obs_module_text("HUDMask.Editor.NoSource")));
			return;
		}

		job_ = new CaptureJob();
		job_->weak = obs_source_get_weak_source(target);
		job_->loadExisting = loadExisting;
		QPointer<CutoutDialog> self(this);
		job_->onFrame = [self](QImage img, bool existing) {
			if (self)
				self->onFrame(std::move(img), existing);
		};
		job_->onFail = [self]() {
			if (!self)
				return;
			QMessageBox::warning(self, self->windowTitle(),
					     QString::fromUtf8(obs_module_text("HUDMask.Editor.CaptureFailed")));
		};
		obs_source_release(target);
		obs_add_tick_callback(capture_tick, job_);
	}

	void onFrame(QImage img, bool loadExisting)
	{
		if (img.isNull())
			return;
		const QSize old = canvas_->mask.size();
		canvas_->setFrame(img);
		if (loadExisting && !ctx_->mask_path.empty()) {
			QImage existing(QString::fromUtf8(ctx_->mask_path.c_str()));
			if (!existing.isNull())
				canvas_->loadExistingMask(existing, ctx_->crop_left, ctx_->crop_top);
		} else if (!loadExisting && old == img.size()) {
			/* keep mask */
		}
	}

	void applyAndClose()
	{
		if (canvas_->mask.isNull() || canvas_->frame.isNull()) {
			reject();
			return;
		}

		const QImage &m = canvas_->mask;
		int minx = m.width(), miny = m.height(), maxx = -1, maxy = -1;
		for (int y = 0; y < m.height(); y++) {
			const uint8_t *row = m.constScanLine(y);
			for (int x = 0; x < m.width(); x++) {
				if (row[x] < 20)
					continue;
				minx = std::min(minx, x);
				miny = std::min(miny, y);
				maxx = std::max(maxx, x);
				maxy = std::max(maxy, y);
			}
		}

		if (maxx < minx) {
			hud_mask_set_cutout(ctx_, "", 0, 0, 0, 0);
			accept();
			return;
		}

		const int pad = 2;
		minx = std::max(0, minx - pad);
		miny = std::max(0, miny - pad);
		maxx = std::min(m.width() - 1, maxx + pad);
		maxy = std::min(m.height() - 1, maxy + pad);

		const int left = minx;
		const int top = miny;
		const int right = m.width() - 1 - maxx;
		const int bottom = m.height() - 1 - maxy;

		QImage cropped = m.copy(minx, miny, maxx - minx + 1, maxy - miny + 1)
					 .convertToFormat(QImage::Format_Grayscale8);

		char *dir = obs_module_get_config_path(obs_current_module(), "masks");
		if (dir) {
			os_mkdirs(dir);
			bfree(dir);
		}

		const char *uuid = obs_source_get_uuid(ctx_->self);
		std::string rel = std::string("masks/") + (uuid ? uuid : "mask") + ".png";
		char *full = obs_module_get_config_path(obs_current_module(), rel.c_str());
		if (!full) {
			QMessageBox::warning(this, windowTitle(),
					     QString::fromUtf8(obs_module_text("HUDMask.Editor.SaveFailed")));
			return;
		}

		if (!cropped.save(QString::fromUtf8(full))) {
			bfree(full);
			QMessageBox::warning(this, windowTitle(),
					     QString::fromUtf8(obs_module_text("HUDMask.Editor.SaveFailed")));
			return;
		}

		hud_mask_set_cutout(ctx_, full, left, top, right, bottom);
		bfree(full);
		accept();
	}

	hud_mask *ctx_;
	MaskCanvas *canvas_;
	CaptureJob *job_ = nullptr;
};

} // namespace

bool hud_mask_open_editor(hud_mask *ctx)
{
	if (!ctx)
		return false;

	QWidget *parent = static_cast<QWidget *>(obs_frontend_get_main_window());
	CutoutDialog dialog(ctx, parent);
	dialog.exec();
	return true;
}
