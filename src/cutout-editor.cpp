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
#include "mask-process.hpp"
#include "presence.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/graphics.h>
#include <util/platform.h>
#include <plugin-support.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QPolygon>
#include <QPushButton>
#include <QRadialGradient>
#include <QSlider>
#include <QTimer>
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
	std::atomic<bool> uiPending{false};
	obs_weak_source_t *weak = nullptr;
	gs_texrender_t *texrender = nullptr;
	gs_stagesurf_t *stagesurf = nullptr;
	uint32_t cx = 0;
	uint32_t cy = 0;
	int stage = 0;
	int skip = 0;
	bool loadExisting = false;
	bool live = false;
	bool heldShowing = false;
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
	job->stage = 0;
	if (job->live)
		return;
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

	if (job->live && job->skip > 0) {
		job->skip--;
		return;
	}

	obs_enter_graphics();

	if (job->stage == 0) {
		obs_source_t *source = obs_weak_source_get_source(job->weak);
		if (!source) {
			capture_fail(job);
			obs_leave_graphics();
			return;
		}

		const uint32_t cx = obs_source_get_width(source);
		const uint32_t cy = obs_source_get_height(source);
		if (!cx || !cy) {
			obs_source_release(source);
			capture_fail(job);
			obs_leave_graphics();
			return;
		}

		if (job->texrender && (job->cx != cx || job->cy != cy)) {
			gs_texrender_destroy(job->texrender);
			job->texrender = nullptr;
			if (job->stagesurf) {
				gs_stagesurface_destroy(job->stagesurf);
				job->stagesurf = nullptr;
			}
		}
		job->cx = cx;
		job->cy = cy;

		const enum gs_color_space space = GS_CS_SRGB;
		const enum gs_color_format format = gs_get_format_from_space(space);
		if (!job->texrender)
			job->texrender = gs_texrender_create(format, GS_ZS_NONE);
		if (!job->stagesurf)
			job->stagesurf = gs_stagesurface_create(job->cx, job->cy, format);
		gs_texrender_reset(job->texrender);

		if (gs_texrender_begin_with_color_space(job->texrender, job->cx, job->cy, space)) {
			struct vec4 zero;
			vec4_zero(&zero);
			gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
			gs_ortho(0.0f, (float)job->cx, 0.0f, (float)job->cy, -100.0f, 100.0f);
			gs_blend_state_push();
			gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
			if (job->live) {
				if (!job->heldShowing) {
					obs_source_inc_showing(source);
					job->heldShowing = true;
				}
			} else {
				obs_source_inc_showing(source);
			}
			obs_source_video_render(source);
			if (!job->live)
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

		if (!job->live) {
			if (job->stagesurf) {
				gs_stagesurface_destroy(job->stagesurf);
				job->stagesurf = nullptr;
			}
			if (job->texrender) {
				gs_texrender_destroy(job->texrender);
				job->texrender = nullptr;
			}
			obs_remove_tick_callback(capture_tick, job);
		}
		obs_leave_graphics();

		if (image.isNull()) {
			if (job->live) {
				job->stage = 0;
				return;
			}
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
		job->loadExisting = false;
		if (!job->live || !job->uiPending.exchange(true)) {
			const QImage copy = image.copy();
			auto cb = job->onFrame;
			QMetaObject::invokeMethod(
				QCoreApplication::instance(),
				[cb, copy, loadExisting]() {
					if (cb)
						cb(copy, loadExisting);
				},
				Qt::QueuedConnection);
		}

		if (job->live) {
			job->stage = 0;
			job->skip = 2;
		}
		return;
	}

	obs_leave_graphics();
	job->stage++;
}

enum class BrushShape { Circle, Square };
enum class EditorTool { Paint, Line, Fill, Magic };

QCursor make_bucket_cursor()
{
	QPixmap pm(24, 24);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(QPen(Qt::white, 1.4));
	p.setBrush(QColor(80, 220, 255));
	QPolygon body;
	body << QPoint(5, 9) << QPoint(17, 9) << QPoint(15, 19) << QPoint(7, 19);
	p.drawPolygon(body);
	p.setBrush(Qt::NoBrush);
	p.drawArc(11, 3, 9, 9, 20 * 16, 200 * 16);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(255, 60, 180));
	p.drawEllipse(15, 18, 5, 5);
	return QCursor(pm, 3, 20);
}

class MaskCanvas : public QWidget {
public:
	QImage frame;
	QImage mask;
	int brush = 28;
	bool erase = false;
	BrushShape shape = BrushShape::Circle;
	EditorTool tool = EditorTool::Paint;

	explicit MaskCanvas(QWidget *parent = nullptr) : QWidget(parent)
	{
		setMouseTracking(true);
		setFocusPolicy(Qt::StrongFocus);
		setMinimumSize(640, 360);
		setCursor(Qt::CrossCursor);
		dwell_ = new QTimer(this);
		dwell_->setSingleShot(true);
		dwell_->setInterval(300);
		connect(dwell_, &QTimer::timeout, this, [this]() {
			if (!lineDragging_)
				return;
			lineStraight_ = true;
			lineFreehand_.clear();
			update();
		});
	}

	void setTool(EditorTool t)
	{
		cancelLineDrag();
		magicPath_.clear();
		magicDragging_ = false;
		tool = t;
		updateCursor();
		if (toolChanged)
			toolChanged();
		update();
	}

	double zoom() const { return zoom_; }
	int zoomPercent() const { return static_cast<int>(std::lround(zoom_ * 100)); }
	void setZoomPercent(int percent)
	{
		zoom_ = std::clamp(percent / 100.0, 1.0, 16.0);
		if (viewChanged)
			viewChanged();
		update();
	}
	void resetView()
	{
		zoom_ = 1.0;
		if (!frame.isNull())
			viewCenter_ = QPointF(frame.width() / 2.0, frame.height() / 2.0);
		if (viewChanged)
			viewChanged();
		update();
	}
	void setSpaceDown(bool down)
	{
		spaceDown_ = down;
		if (!panning_)
			updateCursor();
	}

	std::function<void()> viewChanged;
	std::function<void()> brushChanged;
	std::function<void()> undoChanged;
	std::function<void()> toolChanged;

	void setFrame(const QImage &img)
	{
		const bool first = frame.isNull();
		const QSize prev = frame.size();
		frame = img.convertToFormat(QImage::Format_ARGB32);
		if (mask.size() != frame.size()) {
			mask = QImage(frame.size(), QImage::Format_Grayscale8);
			mask.fill(0);
		}
		if (first || prev != frame.size()) {
			zoom_ = 1.0;
			viewCenter_ = QPointF(frame.width() / 2.0, frame.height() / 2.0);
			if (viewChanged)
				viewChanged();
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
		if (mask.isNull())
			return;
		pushUndo();
		mask.fill(0);
		update();
	}

	bool canUndo() const { return hasUndo_; }

	void undo()
	{
		if (!hasUndo_ || undoMask_.isNull())
			return;
		mask = undoMask_;
		hasUndo_ = false;
		undoMask_ = QImage();
		if (undoChanged)
			undoChanged();
		update();
	}

	void pushUndo()
	{
		if (mask.isNull())
			return;
		undoMask_ = mask.copy();
		hasUndo_ = true;
		if (undoChanged)
			undoChanged();
	}

	std::vector<uint8_t> frameLuma() const
	{
		const int w = frame.width();
		const int h = frame.height();
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
		return lum;
	}

	bool snapEdges()
	{
		if (frame.isNull() || mask.isNull())
			return false;
		const int w = mask.width();
		const int h = mask.height();
		std::vector<uint8_t> user(static_cast<size_t>(w) * h);
		for (int y = 0; y < h; y++)
			memcpy(user.data() + static_cast<size_t>(y) * w, mask.constScanLine(y), static_cast<size_t>(w));
		std::vector<uint8_t> out;
		const int search = std::max(4, std::min(8, brush / 4));
		if (!mask_snap_edges(user, frameLuma(), w, h, search, out))
			return false;
		pushUndo();
		for (int y = 0; y < h; y++)
			memcpy(mask.scanLine(y), out.data() + static_cast<size_t>(y) * w, static_cast<size_t>(w));
		update();
		return true;
	}

	void fillAt(const QPointF &src)
	{
		if (mask.isNull())
			return;
		const int w = mask.width();
		const int h = mask.height();
		int x = static_cast<int>(std::lround(src.x()));
		int y = static_cast<int>(std::lround(src.y()));
		if (x < 0 || y < 0 || x >= w || y >= h)
			return;

		if (!erase && closedPoly_.size() >= 4 &&
		    closedPoly_.containsPoint(src, Qt::OddEvenFill)) {
			pushUndo();
			QPainter fp(&mask);
			fp.setRenderHint(QPainter::Antialiasing, false);
			fp.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			fp.setBrush(Qt::white);
			fp.drawPolygon(closedPoly_);
			closedPoly_.clear();
			update();
			return;
		}

		std::vector<uint8_t> gray(static_cast<size_t>(w) * h);
		for (int yy = 0; yy < h; yy++)
			memcpy(gray.data() + static_cast<size_t>(yy) * w, mask.constScanLine(yy),
			       static_cast<size_t>(w));
		const bool ok = erase ? mask_flood_erase(gray, w, h, x, y)
				      : mask_flood_fill(gray, w, h, x, y, true);
		if (!ok)
			return;
		pushUndo();
		for (int yy = 0; yy < h; yy++)
			memcpy(mask.scanLine(yy), gray.data() + static_cast<size_t>(yy) * w, static_cast<size_t>(w));
		update();
	}

	void closePolyline()
	{
		if (linePts_.size() < 3)
			return;
		pushUndo();
		strokeOnMask(linePts_.back(), linePts_.front());
		closedPoly_ = linePts_;
		closedPoly_ << linePts_.front();
		linePts_.clear();
		update();
	}

	void cancelLineDrag()
	{
		if (dwell_)
			dwell_->stop();
		lineDragging_ = false;
		lineStraight_ = false;
		lineFreehand_.clear();
		update();
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

		p.setRenderHint(QPainter::Antialiasing, true);
		auto drawPreviewLine = [&](const QPoint &a, const QPoint &b, int srcWidth) {
			const int wgt = std::max(2, srcToWidgetLen(std::max(2, srcWidth)));
			p.setPen(QPen(QColor(255, 60, 180), wgt + 2, Qt::SolidLine, Qt::RoundCap));
			p.drawLine(a, b);
			p.setPen(QPen(QColor(255, 255, 255), wgt, Qt::SolidLine, Qt::RoundCap));
			p.drawLine(a, b);
		};
		if (tool == EditorTool::Line && lineDragging_ && !linePts_.empty()) {
			p.setBrush(Qt::NoBrush);
			QPen cyan(QColor(80, 220, 255), 2);
			cyan.setCapStyle(Qt::RoundCap);
			p.setPen(cyan);
			if (lineStraight_) {
				p.drawLine(srcToWidget(linePts_.back()), srcToWidget(cursorSrc_));
			} else if (lineFreehand_.size() >= 2) {
				for (int i = 1; i < lineFreehand_.size(); i++)
					p.drawLine(srcToWidget(lineFreehand_[i - 1]), srcToWidget(lineFreehand_[i]));
			}
		}
		if (tool == EditorTool::Magic && magicPath_.size() >= 2) {
			p.setBrush(Qt::NoBrush);
			QPen cyan(QColor(80, 220, 255), 2);
			cyan.setCapStyle(Qt::RoundCap);
			p.setPen(cyan);
			for (int i = 1; i < magicPath_.size(); i++)
				p.drawLine(srcToWidget(magicPath_[i - 1]), srcToWidget(magicPath_[i]));
		}

		if (cursorOn_ && tool == EditorTool::Paint) {
			p.setPen(QPen(erase ? QColor(255, 80, 80) : QColor(255, 230, 80), 1));
			p.setBrush(Qt::NoBrush);
			const QPoint c = srcToWidget(cursorSrc_);
			const int r = srcToWidgetLen(brush);
			if (shape == BrushShape::Square)
				p.drawRect(c.x() - r, c.y() - r, r * 2, r * 2);
			else
				p.drawEllipse(c, r, r);
		}
	}

	void mousePressEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && spaceDown_)) {
			panning_ = true;
			lastPanWidget_ = e->pos();
			setCursor(Qt::ClosedHandCursor);
			return;
		}
		if (e->button() == Qt::LeftButton)
			handlePress(widgetToSrc(e->pos()));
	}

	void mouseMoveEvent(QMouseEvent *e) override
	{
		cursorOn_ = true;
		cursorSrc_ = widgetToSrc(e->pos());
		if (panning_) {
			const double scale = fitScale() * zoom_;
			if (scale > 1e-6) {
				const QPoint d = e->pos() - lastPanWidget_;
				viewCenter_ -= QPointF(d.x() / scale, d.y() / scale);
				lastPanWidget_ = e->pos();
				if (viewChanged)
					viewChanged();
			}
			update();
			return;
		}
		if (e->buttons() & Qt::LeftButton)
			handleMove(cursorSrc_);
		update();
	}

	void mouseReleaseEvent(QMouseEvent *e) override
	{
		if (e->button() == Qt::LeftButton)
			handleRelease(widgetToSrc(e->pos()));
		if (e->button() == Qt::MiddleButton || e->button() == Qt::LeftButton)
			panning_ = false;
		updateCursor();
	}

	void leaveEvent(QEvent *) override
	{
		cursorOn_ = false;
		update();
	}

	void keyPressEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
			setSpaceDown(true);
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_0 && e->modifiers() & Qt::ControlModifier) {
			resetView();
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_Z && e->modifiers() & Qt::ControlModifier) {
			undo();
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_Escape) {
			cancelLineDrag();
			magicPath_.clear();
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
			closePolyline();
			e->accept();
			return;
		}
		QWidget::keyPressEvent(e);
	}

	void keyReleaseEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
			setSpaceDown(false);
			e->accept();
			return;
		}
		QWidget::keyReleaseEvent(e);
	}

	void wheelEvent(QWheelEvent *e) override
	{
		if (e->modifiers() & Qt::ControlModifier) {
			if (frame.isNull())
				return;
			const QPoint pos = e->position().toPoint();
			const QPointF src = widgetToSrc(pos);
			const double factor = e->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
			zoom_ = std::clamp(zoom_ * factor, 1.0, 16.0);
			const double scale = fitScale() * zoom_;
			if (scale > 1e-6) {
				viewCenter_.setX(src.x() - (pos.x() - width() / 2.0) / scale);
				viewCenter_.setY(src.y() - (pos.y() - height() / 2.0) / scale);
			}
			if (viewChanged)
				viewChanged();
			e->accept();
			update();
			return;
		}
		brush = std::clamp(brush + (e->angleDelta().y() > 0 ? 2 : -2), 4, 96);
		if (brushChanged)
			brushChanged();
		update();
	}

private:
	double fitScale() const
	{
		if (frame.isNull() || width() <= 0 || height() <= 0)
			return 1.0;
		return std::min(width() / static_cast<double>(frame.width()),
				height() / static_cast<double>(frame.height()));
	}

	QRect fitted() const
	{
		if (frame.isNull())
			return {};
		const double scale = fitScale() * zoom_;
		const int dw = std::max(1, static_cast<int>(std::lround(frame.width() * scale)));
		const int dh = std::max(1, static_cast<int>(std::lround(frame.height() * scale)));
		const int x = static_cast<int>(std::lround(width() / 2.0 - viewCenter_.x() * scale));
		const int y = static_cast<int>(std::lround(height() / 2.0 - viewCenter_.y() * scale));
		return {x, y, dw, dh};
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

	void updateCursor()
	{
		if (spaceDown_)
			setCursor(panning_ ? Qt::ClosedHandCursor : Qt::OpenHandCursor);
		else if (tool == EditorTool::Fill)
			setCursor(fillCursor_);
		else
			setCursor(Qt::CrossCursor);
	}

	void stamp(const QPointF &src)
	{
		if (mask.isNull())
			return;
		QPainter p(&mask);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setPen(Qt::NoPen);
		if (erase)
			p.setCompositionMode(QPainter::CompositionMode_Source);

		const QRectF box(src.x() - brush, src.y() - brush, brush * 2.0, brush * 2.0);
		if (erase) {
			p.setBrush(Qt::black);
			if (shape == BrushShape::Square)
				p.drawRect(box);
			else
				p.drawEllipse(src, brush, brush);
			return;
		}
		if (shape == BrushShape::Square) {
			p.setBrush(Qt::white);
			p.drawRect(box);
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

	void strokeOnMask(const QPointF &a, const QPointF &b)
	{
		if (mask.isNull())
			return;
		QPainter p(&mask);
		p.setRenderHint(QPainter::Antialiasing, true);
		if (erase)
			p.setCompositionMode(QPainter::CompositionMode_Source);
		QPen pen(erase ? Qt::black : Qt::white, 2.0);
		pen.setCapStyle(Qt::RoundCap);
		pen.setJoinStyle(Qt::RoundJoin);
		p.setPen(pen);
		p.drawLine(a, b);
	}

	void handlePress(const QPointF &src)
	{
		if (tool == EditorTool::Fill) {
			fillAt(src);
			return;
		}
		if (tool == EditorTool::Magic) {
			magicPath_.clear();
			magicPath_ << src;
			magicDragging_ = true;
			return;
		}
		if (tool == EditorTool::Line) {
			if (linePts_.size() >= 3) {
				const QPointF first = linePts_.front();
				const double d = QLineF(src, first).length();
				if (d <= std::max(8.0, brush * 0.8)) {
					closePolyline();
					return;
				}
			}
			if (linePts_.empty())
				linePts_ << src;
			lineDragging_ = true;
			lineStraight_ = false;
			lineFreehand_.clear();
			lineFreehand_ << linePts_.back() << src;
			if (dwell_)
				dwell_->start();
			return;
		}
		painting_ = true;
		lastSrc_ = src;
		stamp(src);
	}

	void handleMove(const QPointF &src)
	{
		if (tool == EditorTool::Magic && magicDragging_) {
			if (magicPath_.isEmpty() || QLineF(magicPath_.back(), src).length() >= 2.0)
				magicPath_ << src;
			return;
		}
		if (tool == EditorTool::Line && lineDragging_) {
			if (lineStraight_) {
				cursorSrc_ = src;
			} else {
				if (lineFreehand_.isEmpty() || QLineF(lineFreehand_.back(), src).length() >= 1.5)
					lineFreehand_ << src;
				if (dwell_)
					dwell_->start();
			}
			return;
		}
		if (painting_) {
			stroke(lastSrc_, src);
			lastSrc_ = src;
		}
	}

	void handleRelease(const QPointF &src)
	{
		if (tool == EditorTool::Magic && magicDragging_) {
			magicDragging_ = false;
			if (QLineF(magicPath_.back(), magicPath_.front()).length() > 3)
				magicPath_ << magicPath_.front();
			applyMagic();
			magicPath_.clear();
			return;
		}
		if (tool == EditorTool::Line && lineDragging_) {
			if (dwell_)
				dwell_->stop();
			pushUndo();
			const QPointF from = linePts_.back();
			if (lineStraight_) {
				strokeOnMask(from, src);
				linePts_ << src;
			} else {
				for (int i = 1; i < lineFreehand_.size(); i++)
					strokeOnMask(lineFreehand_[i - 1], lineFreehand_[i]);
				if (!lineFreehand_.isEmpty())
					linePts_ << lineFreehand_.back();
			}
			lineDragging_ = false;
			lineStraight_ = false;
			lineFreehand_.clear();
			if (linePts_.size() >= 3) {
				const double d = QLineF(linePts_.back(), linePts_.front()).length();
				if (d <= std::max(8.0, brush * 0.8))
					closePolyline();
			}
			return;
		}
		painting_ = false;
	}

	void applyMagic()
	{
		if (frame.isNull() || mask.isNull() || magicPath_.size() < 8)
			return;
		std::vector<MaskPoint> loop;
		loop.reserve(static_cast<size_t>(magicPath_.size()));
		for (const QPointF &pt : magicPath_)
			loop.push_back({static_cast<float>(pt.x()), static_cast<float>(pt.y())});
		std::vector<MaskPoint> snapped;
		if (!mask_magic_shrinkwrap(loop, frameLuma(), mask.width(), mask.height(), 18, snapped))
			return;
		QPolygonF poly;
		poly.reserve(static_cast<int>(snapped.size()));
		for (const MaskPoint &pt : snapped)
			poly << QPointF(pt.x, pt.y);
		pushUndo();
		QPainter p(&mask);
		p.setRenderHint(QPainter::Antialiasing, true);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		p.drawPolygon(poly);
		update();
	}

	bool painting_ = false;
	bool panning_ = false;
	bool spaceDown_ = false;
	bool cursorOn_ = false;
	double zoom_ = 1.0;
	QPointF viewCenter_;
	QPoint lastPanWidget_;
	QPointF lastSrc_;
	QPointF cursorSrc_;
	QImage undoMask_;
	bool hasUndo_ = false;
	QTimer *dwell_ = nullptr;
	bool lineDragging_ = false;
	bool lineStraight_ = false;
	QPolygonF linePts_;
	QPolygonF lineFreehand_;
	bool magicDragging_ = false;
	QPolygonF magicPath_;
	QPolygonF closedPoly_;
	QCursor fillCursor_ = make_bucket_cursor();
};

class CutoutDialog : public QDialog {
public:
	CutoutDialog(hud_mask *ctx, QWidget *parent) : QDialog(parent), ctx_(ctx)
	{
		setWindowTitle(QString::fromUtf8(obs_module_text("HUDMask.Editor.Title")));
		resize(1100, 720);

		canvas_ = new MaskCanvas(this);

		auto *maskBrush = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.MaskBrush")), this);
		auto *eraseBrush = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.EraseBrush")), this);
		maskBrush->setCheckable(true);
		eraseBrush->setCheckable(true);
		maskBrush->setChecked(true);

		auto *shape = new QComboBox(this);
		shape->addItem(QString::fromUtf8(obs_module_text("HUDMask.Editor.BrushCircle")), 0);
		shape->addItem(QString::fromUtf8(obs_module_text("HUDMask.Editor.BrushSquare")), 1);
		shape->addItem(QString::fromUtf8(obs_module_text("HUDMask.Editor.Line")), 2);
		shape->addItem(QString::fromUtf8(obs_module_text("HUDMask.Editor.Fill")), 3);
		shape->addItem(QString::fromUtf8(obs_module_text("HUDMask.Editor.Magic")), 4);

		auto *brushLabel = new QLabel(this);
		auto *brush = new QSlider(Qt::Horizontal, this);
		brush->setRange(4, 96);
		brush->setValue(canvas_->brush);

		auto *zoomLabel = new QLabel(this);
		auto *zoom = new QSlider(Qt::Horizontal, this);
		zoom->setRange(100, 1600);
		zoom->setSingleStep(5);
		zoom->setPageStep(25);
		zoom->setValue(100);

		auto *snap = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Snap")), this);
		auto *undo = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Undo")), this);
		undo->setEnabled(false);
		auto *refresh = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Refresh")), this);
		auto *clear = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Clear")), this);
		auto *ok = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Apply")), this);
		auto *cancel = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Cancel")), this);

		pauseBtn_ = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Pause")), this);
		playBtn_ = new QPushButton(QString::fromUtf8(obs_module_text("HUDMask.Editor.Play")), this);
		pauseBtn_->setCheckable(true);
		playBtn_->setCheckable(true);
		pauseBtn_->setChecked(true);

		auto *hintPan = new QLabel(QString::fromUtf8(obs_module_text("HUDMask.Editor.HintPan")), this);
		auto *hintZoom = new QLabel(QString::fromUtf8(obs_module_text("HUDMask.Editor.HintZoom")), this);
		auto *hintBrush = new QLabel(QString::fromUtf8(obs_module_text("HUDMask.Editor.HintBrush")), this);
		auto *hintTool = new QLabel(this);

		canvas_->viewChanged = [this, zoomLabel, zoom]() {
			const int pct = canvas_->zoomPercent();
			zoomLabel->setText(QString::fromUtf8(obs_module_text("HUDMask.Editor.Zoom")).arg(pct));
			zoom->blockSignals(true);
			zoom->setValue(pct);
			zoom->blockSignals(false);
		};
		canvas_->viewChanged();

		auto updateBrushLabel = [brushLabel, brush]() {
			brushLabel->setText(QString::fromUtf8(obs_module_text("HUDMask.Editor.Brush")) +
					    QString("  %1").arg(brush->value()));
		};
		updateBrushLabel();
		canvas_->brushChanged = [this, brush, updateBrushLabel]() {
			brush->blockSignals(true);
			brush->setValue(canvas_->brush);
			brush->blockSignals(false);
			updateBrushLabel();
		};
		canvas_->undoChanged = [this, undo]() { undo->setEnabled(canvas_->canUndo()); };

		auto applyMode = [this, maskBrush, eraseBrush, shape, brush, brushLabel, hintTool]() {
			maskBrush->setChecked(!canvas_->erase);
			eraseBrush->setChecked(canvas_->erase);
			const EditorTool t = canvas_->tool;
			const char *hint = "HUDMask.Editor.HintBrush";
			if (t == EditorTool::Line)
				hint = "HUDMask.Editor.HintLine";
			else if (t == EditorTool::Fill)
				hint = "HUDMask.Editor.HintFill";
			else if (t == EditorTool::Magic)
				hint = "HUDMask.Editor.HintMagic";
			hintTool->setText(QString::fromUtf8(obs_module_text(hint)));
			const bool sizeOn = t == EditorTool::Paint;
			brush->setEnabled(sizeOn);
			brushLabel->setEnabled(sizeOn);
		};
		applyMode();

		connect(maskBrush, &QPushButton::clicked, this, [this, applyMode]() {
			canvas_->erase = false;
			applyMode();
			canvas_->update();
		});
		connect(eraseBrush, &QPushButton::clicked, this, [this, applyMode]() {
			canvas_->erase = true;
			applyMode();
			canvas_->update();
		});
		connect(shape, &QComboBox::currentIndexChanged, this, [this, shape, applyMode](int) {
			const int id = shape->currentData().toInt();
			if (id == 0) {
				canvas_->setTool(EditorTool::Paint);
				canvas_->shape = BrushShape::Circle;
			} else if (id == 1) {
				canvas_->setTool(EditorTool::Paint);
				canvas_->shape = BrushShape::Square;
			} else if (id == 2) {
				canvas_->setTool(EditorTool::Line);
			} else if (id == 3) {
				canvas_->setTool(EditorTool::Fill);
			} else {
				canvas_->setTool(EditorTool::Magic);
			}
			applyMode();
		});
		connect(brush, &QSlider::valueChanged, this, [this, updateBrushLabel](int v) {
			canvas_->brush = v;
			updateBrushLabel();
			canvas_->update();
		});
		connect(zoom, &QSlider::valueChanged, this, [this](int v) { canvas_->setZoomPercent(v); });
		connect(snap, &QPushButton::clicked, this, [this]() { canvas_->snapEdges(); });
		connect(undo, &QPushButton::clicked, this, [this]() { canvas_->undo(); });
		connect(refresh, &QPushButton::clicked, this, [this]() {
			setLive(false);
			startCapture(false, false);
		});
		connect(clear, &QPushButton::clicked, this, [this]() { canvas_->clearMask(); });
		connect(ok, &QPushButton::clicked, this, [this]() { applyAndClose(); });
		connect(cancel, &QPushButton::clicked, this, [this]() { reject(); });
		connect(pauseBtn_, &QPushButton::clicked, this, [this]() { setLive(false); });
		connect(playBtn_, &QPushButton::clicked, this, [this]() { setLive(true); });

		auto *hints = new QVBoxLayout();
		hints->setContentsMargins(0, 0, 0, 0);
		hints->setSpacing(2);
		hints->addWidget(hintPan);
		hints->addWidget(hintZoom);
		hints->addWidget(hintBrush);
		hints->addWidget(hintTool);

		auto *transport = new QHBoxLayout();
		transport->addStretch();
		transport->addWidget(pauseBtn_);
		transport->addWidget(playBtn_);

		auto *footer = new QHBoxLayout();
		footer->addLayout(hints, 1);
		footer->addLayout(transport);

		auto *tools = new QHBoxLayout();
		tools->addWidget(maskBrush);
		tools->addWidget(eraseBrush);
		tools->addWidget(shape);
		tools->addWidget(brushLabel);
		tools->addWidget(brush, 1);
		tools->addWidget(zoomLabel);
		tools->addWidget(zoom, 1);
		tools->addWidget(snap);
		tools->addWidget(undo);
		tools->addWidget(refresh);
		tools->addWidget(clear);
		tools->addStretch();
		tools->addWidget(ok);
		tools->addWidget(cancel);

		auto *root = new QVBoxLayout(this);
		root->addWidget(canvas_, 1);
		root->addLayout(footer);
		root->addLayout(tools);

		startCapture(true, false);
		canvas_->setFocus();
	}

	~CutoutDialog() override { stopCapture(); }

protected:
	void keyPressEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
			canvas_->setSpaceDown(true);
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_0 && e->modifiers() & Qt::ControlModifier) {
			canvas_->resetView();
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_Z && e->modifiers() & Qt::ControlModifier) {
			canvas_->undo();
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_Escape) {
			canvas_->cancelLineDrag();
			e->accept();
			return;
		}
		if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
			canvas_->closePolyline();
			e->accept();
			return;
		}
		QDialog::keyPressEvent(e);
	}

	void keyReleaseEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
			canvas_->setSpaceDown(false);
			e->accept();
			return;
		}
		QDialog::keyReleaseEvent(e);
	}

private:
	void setLive(bool live)
	{
		live_ = live;
		if (pauseBtn_)
			pauseBtn_->setChecked(!live);
		if (playBtn_)
			playBtn_->setChecked(live);
		if (live)
			startCapture(false, true);
		else
			stopCapture();
	}

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
				if (job->heldShowing && job->weak) {
					obs_source_t *source = obs_weak_source_get_source(job->weak);
					if (source) {
						obs_source_dec_showing(source);
						obs_source_release(source);
					}
				}
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

	void startCapture(bool loadExisting, bool live)
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
		job_->loadExisting = loadExisting && !live;
		job_->live = live;
		QPointer<CutoutDialog> self(this);
		job_->onFrame = [self](QImage img, bool existing) {
			if (self)
				self->onFrame(std::move(img), existing);
		};
		job_->onFail = [self, live]() {
			if (!self || live)
				return;
			QMessageBox::warning(self, self->windowTitle(),
					     QString::fromUtf8(obs_module_text("HUDMask.Editor.CaptureFailed")));
		};
		obs_source_release(target);
		obs_add_tick_callback(capture_tick, job_);
	}

	void onFrame(QImage img, bool loadExisting)
	{
		if (job_)
			job_->uiPending.store(false);
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
		std::vector<uint8_t> gray(static_cast<size_t>(m.width()) * m.height());
		for (int y = 0; y < m.height(); y++)
			memcpy(gray.data() + static_cast<size_t>(y) * m.width(), m.constScanLine(y),
			       static_cast<size_t>(m.width()));
		const MaskCrop crop = mask_crop_from_opaque(gray, m.width(), m.height(), 20, 32);
		if (crop.empty) {
			hud_mask_set_cutout(ctx_, "", 0, 0, 0, 0);
			accept();
			return;
		}

		const int left = crop.left;
		const int top = crop.top;
		const int right = crop.right;
		const int bottom = crop.bottom;

		QImage cropped = m.copy(crop.min_x, crop.min_y, crop.max_x - crop.min_x + 1, crop.max_y - crop.min_y + 1)
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

		const int cw = crop.max_x - crop.min_x + 1;
		const int ch = crop.max_y - crop.min_y + 1;
		std::vector<uint8_t> luma(static_cast<size_t>(cw) * ch);
		QImage frame_crop = canvas_->frame.copy(crop.min_x, crop.min_y, cw, ch)
					    .convertToFormat(QImage::Format_ARGB32);
		for (int y = 0; y < ch; y++) {
			const QRgb *row = reinterpret_cast<const QRgb *>(frame_crop.constScanLine(y));
			for (int x = 0; x < cw; x++) {
				const QRgb p = row[x];
				luma[static_cast<size_t>(y) * cw + x] =
					static_cast<uint8_t>((77 * qRed(p) + 150 * qGreen(p) + 29 * qBlue(p)) >> 8);
			}
		}
		std::vector<uint8_t> crop_mask(static_cast<size_t>(cw) * ch);
		for (int y = 0; y < ch; y++)
			memcpy(crop_mask.data() + static_cast<size_t>(y) * cw, cropped.constScanLine(y),
			       static_cast<size_t>(cw));
		ctx_->mask_path = full;
		hud_mask_presence_save_ref(ctx_, luma.data(), crop_mask.data(), cw, ch);

		hud_mask_set_cutout(ctx_, full, left, top, right, bottom);
		bfree(full);
		accept();
	}

	hud_mask *ctx_;
	MaskCanvas *canvas_;
	CaptureJob *job_ = nullptr;
	QPushButton *pauseBtn_ = nullptr;
	QPushButton *playBtn_ = nullptr;
	bool live_ = false;
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
