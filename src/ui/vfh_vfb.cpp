//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_vfb.h"

#include <RE/RE_QtWindow.h>


#define PRINT_UI_CALLS  0


using namespace VRayForHoudini;
using namespace VRayForHoudini::UI;


VfbWidget::VfbWidget(QWidget *parent, Qt::WindowFlags f):
	QWidget(parent, f),
	m_vfb(nullptr)
{}


VfbWidget::~VfbWidget()
{
	FreePtr(m_vfb);
}


void VfbWidget::paintEvent(QPaintEvent *e)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VfbDraw::paintEvent");
#endif

	if (m_vfb && m_csect.tryEnter()) {
		QPainter painter(this);

		const int width  = m_vfb->getWidth();
		const int height = m_vfb->getHeight();

		const VRay::AColor *pixels = m_vfb->getPixelData();
		if (pixels) {
			QImage img(width, height, QImage::Format_ARGB32);

			for (int w = 0; w < width; ++w) {
				for (int h = 0; h < height; ++h) {
					const VRay::AColor &pixel = pixels[width * h + w];

					float r = pixel.color.r;
					float g = pixel.color.g;
					float b = pixel.color.b;
					float a = pixel.alpha;

					if (m_options.clamp) {
						r = VUtils::Min(r, 1.0f);
						g = VUtils::Min(g, 1.0f);
						b = VUtils::Min(b, 1.0f);
					}
					if (m_options.forceAlpha) {
						a = 1;
					}

					img.setPixel(w, h, qRgba(r * 255.0f, g * 255.0f, b * 255.0f, a * 255.0f));
				}
			}

			painter.fillRect(0, 0, width, height, Qt::SolidPattern);
			painter.drawImage(0, 0, img);
		}

		if (m_buckets.size()) {
			QPen line_pen(QColor(0, 145, 208));
			line_pen.setWidth(1);
			line_pen.setStyle(Qt::DashLine);

			painter.setPen(line_pen);

			Q_FOREACH (const VfbBucket &bucket, m_buckets) {
				if (bucket.w && bucket.h) {
					painter.fillRect(bucket.x, bucket.y, bucket.w, bucket.h, Qt::SolidPattern);

					//
					// 0----1
					// |    |
					// |    |
					// 3----2
					//
					const int x0 = bucket.x;
					const int y0 = bucket.y;
					const int x1 = x0 + bucket.w;
					const int y1 = y0;
					const int x2 = x1;
					const int y2 = y1 + bucket.h;
					const int x3 = x0;
					const int y3 = y2;

					painter.drawLine(x0, y0, x1, y1);
					painter.drawLine(x1, y1, x2, y2);
					painter.drawLine(x2, y2, x3, y3);
					painter.drawLine(x3, y3, x0, y0);
				}
			}
		}

		if (!m_status.isEmpty()) {
			const int progress_height = 18;

			QRect status_bar(QPoint(0, height - progress_height), QSize(width, progress_height));
			painter.fillRect(status_bar, QBrush(QColor(0, 145, 208, 100)));

			QRect status_text(QPoint(5, height - progress_height + 3), QSize(width, progress_height));
			QPen text_pen(QColor(240, 240, 240));
			painter.setPen(text_pen);
			painter.drawText(status_text, m_status);
		}

		m_csect.leave();
	}
}


VfbWindow::VfbWindow(QWidget *parent, Qt::WindowFlags f):
	QMainWindow(parent, f),
	m_widget(this)
{
	setWindowFlags(windowFlags() | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint);
	setCentralWidget(&m_widget);
}


VfbWindow::~VfbWindow()
{}


void VfbWindow::keyPressEvent(QKeyEvent *e)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VfbWidget::keyPressEvent");
#endif
	if (e->key() == Qt::Key_Escape) {
		if (m_abortCb) {
			m_abortCb();

			// Cleanup tmp UI data; shouldn't be done here...
			m_widget.m_buckets.clear();
			m_widget.m_status.clear();
			m_widget.update();
		}
	}
}


VFB::VFB():
	m_window(nullptr)
{}


VFB::~VFB()
{
	free();
}


void VFB::init()
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::init");
#endif
	if (!m_window) {
		m_window = new VfbWindow(RE_QtWindow::mainQtWindow());
		m_window->setWindowTitle("V-Ray Frame Buffer");
	}

	m_window->m_widget.m_buckets.clear();
	m_window->m_widget.m_status.clear();
}

void VFB::free()
{
	FreePtr(m_window);
}


int VFB::isInitialized()
{
	return !!(m_window);
}


void VFB::show()
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::show");
#endif
	m_window->show();
	m_window->raise();
}


void VFB::hide()
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::hide");
#endif
	if (m_window) {
		m_window->hide();
	}
}


void VFB::resize(int w, int h)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::resize");
#endif
	m_window->resize(w, h);
	m_window->m_widget.resize(w, h);

	FreePtr(m_window->m_widget.m_vfb);

	m_window->m_widget.m_vfb = VRay::VRayImage::create(w, h);
}


void VFB::set_abort_callback(const AbortCb &cb)
{
	m_window->set_abort_callback(cb);
}


void VFB::on_rt_image_updated(VRay::VRayRenderer&, VRay::VRayImage *img)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::on_rt_image_updated");
#endif
	if (m_csect.tryEnter()) {
		m_window->m_widget.m_vfb->draw(img, 0, 0);
		m_csect.leave();
	}

	m_window->m_widget.update();
}


void VFB::on_image_ready(VRay::VRayRenderer &renderer)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::on_image_ready");
#endif
	if (m_csect.tryEnter()) {
		m_window->m_widget.m_vfb->draw(renderer.getImage(), 0, 0);
		m_window->m_widget.m_status.clear();
		m_csect.leave();
	}

	m_window->m_widget.update();
}


void VFB::on_bucket_init(VRay::VRayRenderer&, int x, int y, int w, int h, const char *host)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::on_bucket_init");
#endif
	if (m_csect.tryEnter()) {
		VRay::LocalVRayImage rect(m_window->m_widget.m_vfb->crop(x, y, w, h));
		rect->addColor(VRay::Color(0.0f, 0.0f, 0.0f));

		m_window->m_widget.m_buckets.append(VfbBucket(x, y, w, h));
		m_window->m_widget.m_vfb->draw(rect, x, y);
		m_csect.leave();
	}

	m_window->m_widget.update();
}


void VFB::on_bucket_failed(VRay::VRayRenderer&, int x, int y, int w, int h, const char *host)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::on_bucket_failed");
#endif
	if (m_csect.tryEnter()) {
		VRay::LocalVRayImage rect(m_window->m_widget.m_vfb->crop(x, y, w, h));
		rect->addColor(VRay::Color(0.0f, 0.0f, 0.0f));

		m_window->m_widget.m_buckets.removeAll(VfbBucket(x, y, w, h));
		m_window->m_widget.m_vfb->draw(rect, x, y);
		m_csect.leave();
	}

	m_window->m_widget.update();
}


void VFB::on_bucket_ready(VRay::VRayRenderer&, int x, int y, const char *host, VRay::VRayImage *img)
{
#if PRINT_UI_CALLS
	Log::getLog().info("VFB::on_bucket_ready");
#endif
	if (m_csect.tryEnter()) {
		m_window->m_widget.m_buckets.removeAll(VfbBucket(x, y));
		m_window->m_widget.m_vfb->draw(img, x, y);
		m_csect.leave();
	}

	m_window->m_widget.update();
}


void VFB::on_dump_message(VRay::VRayRenderer&, const char *msg, int level)
{
	QString message;
	message.sprintf("%s", msg);

	if (m_csect.tryEnter()) {
		m_window->m_widget.m_status = message.simplified();
		m_csect.leave();
	}

	m_window->m_widget.update();
}


void VFB::on_progress(VRay::VRayRenderer&, const char *msg, int elementNumber, int elementsCount)
{
	const float percentage = 100.0 * elementNumber / elementsCount;

	QString message;
	message.sprintf("%s %.0f%%", msg, percentage);

	if (m_csect.tryEnter()) {
		m_window->m_widget.m_status = message.simplified();
		m_csect.leave();
	}

	m_window->m_widget.update();
}
