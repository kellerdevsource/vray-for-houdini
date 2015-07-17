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

#include "vfh_plugin_exporter.h"

#include <QtCore/QString>
#include <boost/bind.hpp>


#define PRINT_CALLBACK_CALLS  0


using namespace VRayForHoudini;
using namespace VRayForHoudini::Attrs;


VRay::VRayInit *VRayPluginRenderer::g_vrayInit = nullptr;


static void OnDumpMessage(VRay::VRayRenderer &renderer, const char *msg, int level, void *userData)
{
	QString message(msg);
	message = message.simplified();

	if (level <= VRay::MessageError) {
		PRINT_ERROR("V-Ray: %s", message.toAscii().constData());
	}
	else if (level > VRay::MessageError && level <= VRay::MessageWarning) {
		PRINT_WARN("V-Ray: %s", message.toAscii().constData());
	}
	else if (level > VRay::MessageWarning && level <= VRay::MessageInfo) {
		PRINT_INFO("V-Ray: %s", message.toAscii().constData());
	}

	CbSetOnDumpMessage *callbacks = reinterpret_cast<CbSetOnDumpMessage*>(userData);
	for (CbSetOnDumpMessage::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, msg, level);
	}
	for (CbSetOnDumpMessage::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnProgress(VRay::VRayRenderer &renderer, const char *msg, int elementNumber, int elementsCount, void *userData)
{
	QString message(msg);
	message = message.simplified();

	const float percentage = 100.0 * elementNumber / elementsCount;

	PRINT_INFO("V-Ray: %s %.1f%%",
			   message.toAscii().constData(), percentage);

	CbSetOnProgress *callbacks = reinterpret_cast<CbSetOnProgress*>(userData);
	for (CbSetOnProgress::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, msg, elementNumber, elementsCount);
	}
	for (CbSetOnProgress::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnRendererClose(VRay::VRayRenderer &renderer, void *userData)
{
#if PRINT_CALLBACK_CALLS
	PRINT_WARN("VRayPluginRenderer::OnRendererClose()");
#endif
	CbSetOnRendererClose *callbacks = reinterpret_cast<CbSetOnRendererClose*>(userData);
	for (CbSetOnRendererClose::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer);
	}
	for (CbSetOnRendererClose::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnImageReady(VRay::VRayRenderer &renderer, void *userData)
{
#if PRINT_CALLBACK_CALLS
	PRINT_WARN("VRayPluginRenderer::OnImageReady()");
#endif
	CbSetOnImageReady *callbacks = reinterpret_cast<CbSetOnImageReady*>(userData);
	for (CbSetOnImageReady::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer);
	}
	for (CbSetOnImageReady::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnRTImageUpdated(VRay::VRayRenderer &renderer, VRay::VRayImage *img , void *userData)
{
#if PRINT_CALLBACK_CALLS
	PRINT_WARN("VRayPluginRenderer::OnRTImageUpdated()");
#endif
	CbSetOnRTImageUpdated *callbacks = reinterpret_cast<CbSetOnRTImageUpdated*>(userData);
	for (CbSetOnRTImageUpdated::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, img);
	}
	for (CbSetOnRTImageUpdated::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnBucketInit(VRay::VRayRenderer &renderer, int x, int y, int w, int h, const char *host, void *userData)
{
#if PRINT_CALLBACK_CALLS
	PRINT_WARN("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketInit *callbacks = reinterpret_cast<CbSetOnBucketInit*>(userData);
	for (CbSetOnBucketInit::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, x, y, w, h, host);
	}
	for (CbSetOnBucketInit::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnBucketFailed(VRay::VRayRenderer &renderer, int x, int y, int w, int h, const char *host, void *userData)
{
#if PRINT_CALLBACK_CALLS
	PRINT_WARN("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketFailed *callbacks = reinterpret_cast<CbSetOnBucketFailed*>(userData);
	for (CbSetOnBucketFailed::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, x, y, w, h, host);
	}
	for (CbSetOnBucketFailed::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


static void OnBucketReady(VRay::VRayRenderer &renderer, int x, int y, const char *host, VRay::VRayImage *img, void *userData)
{
#if PRINT_CALLBACK_CALLS
	PRINT_WARN("VRayPluginRenderer::OnBucketReady()");
#endif
	CbSetOnBucketReady *callbacks = reinterpret_cast<CbSetOnBucketReady*>(userData);
	for (CbSetOnBucketReady::CbTypeArray::const_iterator cbIt = callbacks->m_cbTyped.begin(); cbIt != callbacks->m_cbTyped.end(); ++cbIt) {
		(*cbIt)(renderer, x, y, host, img);
	}
	for (CbSetOnBucketReady::CbVoidArray::const_iterator cbIt = callbacks->m_cbVoid.begin(); cbIt != callbacks->m_cbVoid.end(); ++cbIt) {
		(*cbIt)();
	}
}


VRayPluginRenderer::VRayPluginRenderer():
	m_vray(nullptr)
{}


VRayPluginRenderer::~VRayPluginRenderer()
{
	PRINT_WARN("~VRayPluginRenderer()");

	freeMem();
}


void VRayPluginRenderer::init(int reInit)
{
	if (NOT(VRayPluginRenderer::g_vrayInit)) {
		return;
	}

	if (reInit) {
		freeMem();
	}

	if (NOT(m_vray)) {
		try {
			VRay::RendererOptions options;
			options.keepRTRunning = true;
			options.showFrameBuffer = false;

			// TODO: Distributed rendering settings
			options.noDR = true;

			m_vray = new VRay::VRayRenderer(options);
		}
		catch (...) {
			m_vray = nullptr;
		}
	}
}


void VRayPluginRenderer::freeMem()
{
	FreePtr(m_vray);
	m_pluginUsage.clear();
}


void VRayPluginRenderer::setImageSize(const int w, const int h)
{
	if (NOT(m_vray)) {
		return;
	}

	m_vray->setImageSize(w, h);
}


void VRayPluginRenderer::setMode(int mode)
{
	if (NOT(m_vray)) {
		return;
	}

	m_vray->setRenderMode((VRay::RendererOptions::RenderMode)mode);
	m_vray->setAutoCommit(true);

	m_vray->setOnDumpMessage(OnDumpMessage,       (void*)&m_callbacks.m_cbOnDumpMessage);
	m_vray->setOnProgress(OnProgress,             (void*)&m_callbacks.m_cbOnProgress);

	m_vray->setOnRendererClose(OnRendererClose,   (void*)&m_callbacks.m_cbOnRendererClose);
	m_vray->setOnImageReady(OnImageReady,         (void*)&m_callbacks.m_cbOnImageReady);

	m_vray->setOnRTImageUpdated(OnRTImageUpdated, (void*)&m_callbacks.m_cbOnRTImageUpdated);

	m_vray->setOnBucketInit(OnBucketInit,         (void*)&m_callbacks.m_cbOnBucketInit);
	m_vray->setOnBucketFailed(OnBucketFailed,     (void*)&m_callbacks.m_cbOnBucketFailed);
	m_vray->setOnBucketReady(OnBucketReady,       (void*)&m_callbacks.m_cbOnBucketReady);

	// Stop before new export
	m_vray->stop();
}


VRay::Plugin VRayPluginRenderer::exportPlugin(const Attrs::PluginDesc &pluginDesc)
{
#define CGR_DEBUG_APPSDK_VALUES  0
	if (NOT(m_vray)) {
		return VRay::Plugin();
	}

	if (pluginDesc.pluginID.empty()) {
		// NOTE: Could be done intentionally to skip plugin creation
		PRINT_WARN("[%s] PluginDesc.pluginID is not set!",
				   pluginDesc.pluginName.c_str());
		return VRay::Plugin();
	}

	VRay::Plugin plug = newPlugin(pluginDesc);
	if (NOT(plug)) {
		PRINT_ERROR("Failed to create plugin: %s [%s]",
					pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str());
	}
	else {
		for (const auto &pIt : pluginDesc.pluginAttrs) {
			const PluginAttr &p = pIt;
#if CGR_DEBUG_APPSDK_VALUES
			PRINT_INFO("Setting plugin parameter: \"%s\" %s.%s",
					   pluginDesc.pluginName.c_str(), pluginDesc.pluginID.c_str(), p.paramName.c_str());
#endif
			if (p.paramType == PluginAttr::AttrTypeIgnore) {
				continue;
			}

			if (p.paramType == PluginAttr::AttrTypeInt) {
				plug.setValue(p.paramName, p.paramValue.valInt);
			}
			else if (p.paramType == PluginAttr::AttrTypeFloat) {
				plug.setValue(p.paramName, p.paramValue.valFloat);
			}
			else if (p.paramType == PluginAttr::AttrTypeColor) {
				plug.setValue(p.paramName, VRay::Color(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2]));
			}
			else if (p.paramType == PluginAttr::AttrTypeVector) {
				plug.setValue(p.paramName, VRay::Vector(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2]));
			}
			else if (p.paramType == PluginAttr::AttrTypeAColor) {
				plug.setValue(p.paramName, VRay::AColor(p.paramValue.valVector[0], p.paramValue.valVector[1], p.paramValue.valVector[2], p.paramValue.valVector[3]));
			}
			else if (p.paramType == PluginAttr::AttrTypePlugin) {
				if (p.paramValue.valPluginOutput.empty()) {
					plug.setValue(p.paramName, p.paramValue.valPlugin);
				}
				else {
					plug.setValue(p.paramName, p.paramValue.valPlugin, p.paramValue.valPluginOutput);
				}
			}
			else if (p.paramType == PluginAttr::AttrTypeTransform) {
				plug.setValue(p.paramName, p.paramValue.valTransform);
			}
			else if (p.paramType == PluginAttr::AttrTypeString) {
				plug.setValue(p.paramName, p.paramValue.valString);
#if CGR_DEBUG_APPSDK_VALUES
				PRINT_INFO("AttrTypeString:  %s [%s] = %s",
						   p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
			}
			else if (p.paramType == PluginAttr::AttrTypeListPlugin) {
				plug.setValue(p.paramName, VRay::Value(p.paramValue.valListValue));
			}
			else if (p.paramType == PluginAttr::AttrTypeListInt) {
				plug.setValue(p.paramName, VRay::Value(p.paramValue.valListInt));
#if CGR_DEBUG_APPSDK_VALUES
				PRINT_INFO("AttrTypeListInt:  %s [%s] = %s",
						   p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
			}
			else if (p.paramType == PluginAttr::AttrTypeListFloat) {
				plug.setValue(p.paramName, VRay::Value(p.paramValue.valListFloat));
#if CGR_DEBUG_APPSDK_VALUES
				PRINT_INFO("AttrTypeListFloat:  %s [%s] = %s",
						   p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
			}
			else if (p.paramType == PluginAttr::AttrTypeListVector) {
				plug.setValue(p.paramName, VRay::Value(p.paramValue.valListVector));
			}
			else if (p.paramType == PluginAttr::AttrTypeListColor) {
				plug.setValue(p.paramName, VRay::Value(p.paramValue.valListColor));
			}
			else if (p.paramType == PluginAttr::AttrTypeListValue) {
				plug.setValue(p.paramName, VRay::Value(p.paramValue.valListValue));
#if CGR_DEBUG_APPSDK_VALUES
				PRINT_INFO("AttrTypeListValue:  %s [%s] = %s",
						   p.paramName.c_str(), plug.getType().c_str(), plug.getValueAsString(p.paramName).c_str());
#endif
			}
			else if (p.paramType == PluginAttr::AttrTypeRawListInt) {
				plug.setValue(p.paramName,
							  (void*)&p.paramValue.valRawListInt[0],
							  p.paramValue.valRawListInt.size() * sizeof(int));
			}
			else if (p.paramType == PluginAttr::AttrTypeRawListFloat) {
				plug.setValue(p.paramName,
							  (void*)&p.paramValue.valRawListFloat[0],
							  p.paramValue.valRawListFloat.size() * sizeof(float));
			}
			else if (p.paramType == PluginAttr::AttrTypeRawListVector) {
				plug.setValue(p.paramName,
							  (void*)&p.paramValue.valRawListVector[0],
							  p.paramValue.valRawListVector.size() * sizeof(VUtils::Vector));
			}
			else if (p.paramType == PluginAttr::AttrTypeRawListColor) {
				plug.setValue(p.paramName,
							  (void*)&p.paramValue.valRawListColor[0],
							  p.paramValue.valRawListColor.size() * sizeof(VUtils::Color));
			}
		}
	}

	return plug;
}


VRay::Plugin VRayPluginRenderer::newPlugin(const Attrs::PluginDesc &pluginDesc)
{
	VRay::Plugin plug = m_vray->newPlugin(pluginDesc.pluginName, pluginDesc.pluginID);

	if (NOT(pluginDesc.pluginName.empty())) {
		m_pluginUsage[pluginDesc.pluginName.c_str()] = PluginUsed(plug);
	}

	return plug;
}


void VRayPluginRenderer::resetObjects()
{
	for (VRayPluginRenderer::PluginUsage::iterator pIt = m_pluginUsage.begin(); pIt != m_pluginUsage.end(); ++pIt) {
		pIt.data().used = false;
	}
}


void VRayPluginRenderer::syncObjects()
{
	if (!m_vray) {
		return;
	}

	typedef VUtils::HashSet<const char*> RemoveKeys;
	RemoveKeys removeKeys;

	for (VRayPluginRenderer::PluginUsage::iterator pIt = m_pluginUsage.begin(); pIt != m_pluginUsage.end(); ++pIt) {
		if (NOT(pIt.data().used)) {
			removeKeys.insert(pIt.key());

			bool res = m_vray->removePlugin(pIt.data().plugin);

			PRINT_WARN("Removing: %s [%i]",
					   pIt.data().plugin.getName().c_str(), res);

			VRay::Error err = m_vray->getLastError();
			if (err != VRay::SUCCESS) {
				PRINT_ERROR("Error removing plugin: %s",
							err.toString().c_str());
			}
		}
	}

	for (RemoveKeys::iterator kIt = removeKeys.begin(); kIt != removeKeys.end(); ++kIt) {
		m_pluginUsage.erase(kIt.key());
	}
}


int VRayPluginRenderer::exportScene(const std::string &filepath)
{
	PRINT_INFO("Starting export to \"%s\"...",
			   filepath.c_str());

	int res = m_vray->exportScene(filepath.c_str());
	if (res) {
		PRINT_ERROR("Error exporting scene!");
	}

	VRay::Error err = m_vray->getLastError();
	if (err != VRay::SUCCESS) {
		PRINT_ERROR("Error: %s",
					err.toString().c_str());
	}

	return 0;
}


int VRayPluginRenderer::startRender(int locked)
{
	PRINT_INFO("Starting render for frame %.3f...",
			   m_vray->getCurrentTime());

	m_vray->start();

	if (locked) {
		m_vray->waitForImageReady();
	}

	return 0;
}


int VRayPluginRenderer::startSequence(int start, int end, int step, int locked)
{
	PRINT_INFO("Starting sequence render (%i-%i,%i)...",
			   start, end, step);

	VRay::SubSequenceDesc seq;
	seq.start = start;
	seq.end   = end;
	seq.step  = step;

	m_vray->renderSequence(&seq, 1);
	// m_vray->renderSequence();
	if (locked) {
		m_vray->waitForSequenceDone();
	}

	return 0;
}


void VRayPluginRenderer::stopRender()
{
	m_vray->stop();
}


int VRayPluginRenderer::isRtRunning()
{
	bool is_rt_running = false;
	if (m_vray) {
		const VRay::RendererOptions &options = m_vray->getOptions();
		if (options.renderMode >= VRay::RendererOptions::RENDER_MODE_RT_CPU) {
			if (true) {
				is_rt_running = true;
			}
		}
	}
	return is_rt_running;
}


void VRayPluginRenderer::VRayInit()
{
	PRINT_INFO("VRayPluginRenderer::VRayInit()");

	if (NOT(VRayPluginRenderer::g_vrayInit)) {
		try {
			bool init_vfb = true;
#ifdef __APPLE__
			init_vfb = false;
#endif
			VRayPluginRenderer::g_vrayInit = new VRay::VRayInit(init_vfb);
		}
		catch (...) {
			VRayPluginRenderer::g_vrayInit = nullptr;
		}
	}
}


void VRayPluginRenderer::VRayDone()
{
	FreePtr(VRayPluginRenderer::g_vrayInit);
}


void VRayPluginRenderer::resetCallbacks()
{
	m_callbacks.clear();
}


void VRayForHoudini::VRayPluginRenderer::setAnimation(bool on)
{
	if (m_vray) {
		m_vray->useAnimatedValues(on);
	}
}


void VRayForHoudini::VRayPluginRenderer::setFrame(fpreal frame)
{
	if (m_vray) {
		m_vray->setCurrentTime(frame);
	}
}


int VRayPluginRenderer::clearFrames(fpreal toTime)
{
	if (m_vray) {
		m_vray->clearAllPropertyValuesUpToTime(toTime);
	}
}
