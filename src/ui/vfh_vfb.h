//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// Andrei Izrantcev <andrei.izrantcev@chaosgroup.com>
//
// All rights reserved. These coded instructions, statements and
// computer programs contain unpublished information proprietary to
// Chaos Software Ltd, which is protected by the appropriate copyright
// laws and may not be disclosed to third parties or copied or
// duplicated, in whole or in part, without prior written consent of
// Chaos Software Ltd.
//

#ifndef VRAY_FOR_HOUDINI_VFB_H
#define VRAY_FOR_HOUDINI_VFB_H

#include "vfh_defines.h"
#include "vfh_vray.h"

#include <QtGui/QtGui>
#include <boost/function.hpp>


namespace VRayForHoudini {
namespace UI {

typedef boost::function<void (void)> AbortCb;
typedef VUtils::CriticalSection      VfbMutex;

struct VfbOptions {
	VfbOptions():
		clamp(true),
		forceAlpha(true)
	{}

	int clamp;
	int forceAlpha;
};


struct VfbBucket {
	VfbBucket():
		x(0),
		y(0),
		w(0),
		h(0)
	{}

	VfbBucket(int x, int y, int w, int h):
		x(x),
		y(y),
		w(w),
		h(h)
	{}

	VfbBucket(int x, int y):
		x(x),
		y(y),
		w(0),
		h(0)
	{}

	int      x;
	int      y;
	int      w;
	int      h;

	QString  host;

	bool operator==(const VfbBucket& other) {
		return ((x == other.x) && (y == other.y));
	}
};

typedef QList<VfbBucket> VfbBuckets;


class VfbWidget:
		public QWidget
{
public:
	VfbWidget(QWidget* parent=0, Qt::WindowFlags f=0);
	virtual ~VfbWidget();

protected:
	void paintEvent(QPaintEvent* e);

public:
	VRay::VRayImage *m_vfb;
	VfbBuckets       m_buckets;
	VfbOptions       m_options;
	QString          m_status;

private:
	QPixmap          m_pixmap;
	VfbMutex         m_csect;

};


class VfbWindow:
		public QMainWindow
{
public:
	VfbWindow(QWidget* parent=0, Qt::WindowFlags f=0);
	virtual ~VfbWindow();

protected:
	virtual void  keyPressEvent(QKeyEvent*) VRAY_OVERRIDE;

public:
	void          set_abort_callback(const AbortCb &cb) { m_abortCb = cb; }

	VfbWidget     m_widget;

private:
	AbortCb       m_abortCb;

};


class VFB
{

public:
	VFB();
	~VFB();

	void       init();
	void       free();

	void       show();
	void       hide();
	void       resize(int w, int h);

	void       on_dump_message(VRay::VRayRenderer &renderer, const char *msg, int level);
	void       on_progress(VRay::VRayRenderer &renderer, const char *msg, int elementNumber, int elementsCount);

	void       on_image_ready(VRay::VRayRenderer &renderer);

	void       on_rt_image_updated(VRay::VRayRenderer &renderer, VRay::VRayImage *img);

	void       on_bucket_init(VRay::VRayRenderer&, int x, int y, int w, int h, const char *host);
	void       on_bucket_failed(VRay::VRayRenderer&, int x, int y, int w, int h, const char *host);
	void       on_bucket_ready(VRay::VRayRenderer&, int x, int y, const char *host, VRay::VRayImage *img);

	void       set_abort_callback(const AbortCb &cb);

private:
	VfbWindow *m_window;
	AbortCb    m_abortCb;
	VfbMutex   m_csect;

};

} // namespace UI
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VFB_H
