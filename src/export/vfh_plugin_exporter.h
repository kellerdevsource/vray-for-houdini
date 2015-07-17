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

#ifndef VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
#define VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H

#include "vfh_defines.h"
#include "vfh_vray.h"

#include <OP/OP_Node.h>
#include <OP/OP_Network.h>


namespace VRayForHoudini {
namespace Attrs {

struct PluginAttr {
	enum AttrType {
		AttrTypeUnknown = 0,
		AttrTypeIgnore,
		AttrTypeInt,
		AttrTypeFloat,
		AttrTypeVector,
		AttrTypeColor,
		AttrTypeAColor,
		AttrTypeTransform,
		AttrTypeString,
		AttrTypePlugin,
		AttrTypeListInt,
		AttrTypeListFloat,
		AttrTypeListVector,
		AttrTypeListColor,
		AttrTypeListTransform,
		AttrTypeListString,
		AttrTypeListPlugin,
		AttrTypeListValue,
		AttrTypeRawListInt,
		AttrTypeRawListFloat,
		AttrTypeRawListVector,
		AttrTypeRawListColor,
	};

	PluginAttr() {
		paramName.clear();
		paramType = PluginAttr::AttrTypeUnknown;
	}

	PluginAttr(const std::string &attrName, const AttrType attrType) {
		paramName = attrName;
		paramType = attrType;
	}

	PluginAttr(const std::string &attrName, const std::string &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeString;
		paramValue.valString = attrValue;
	}

	PluginAttr(const std::string &attrName, const char *attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeString;
		paramValue.valString = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::Transform attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeTransform;
		paramValue.valTransform = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::Plugin attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypePlugin;
		paramValue.valPlugin = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::Plugin attrValue, const std::string &output) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypePlugin;
		paramValue.valPlugin = attrValue;
		paramValue.valPluginOutput = output;
	}

	PluginAttr(const std::string &attrName, const AttrType &attrType, const float &a, const float &b, const float &c, const float &d=0.0f) {
		paramName = attrName;
		paramType = attrType;
		paramValue.valVector[0] = a;
		paramValue.valVector[1] = b;
		paramValue.valVector[2] = c;
		paramValue.valVector[3] = d;
	}

	PluginAttr(const std::string &attrName, const float &a, const float &b, const float &c, const float &d=1.0f) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeAColor;
		paramValue.valVector[0] = a;
		paramValue.valVector[1] = b;
		paramValue.valVector[2] = c;
		paramValue.valVector[3] = d;
	}

	PluginAttr(const std::string &attrName, const VRay::Vector &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeVector;
		paramValue.valVector[0] = attrValue.x;
		paramValue.valVector[1] = attrValue.y;
		paramValue.valVector[2] = attrValue.z;
		paramValue.valVector[3] = 1.0f;
	}

	PluginAttr(const std::string &attrName, const int &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const bool &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeInt;
		paramValue.valInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const float &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeFloat;
		paramValue.valFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const fpreal &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeFloat;
		paramValue.valFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::IntList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListInt;
		paramValue.valListInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::FloatList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListFloat;
		paramValue.valListFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::VectorList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListVector;
		paramValue.valListVector = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::ColorList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListColor;
		paramValue.valListColor = attrValue;
	}

	PluginAttr(const std::string &attrName, const VRay::ValueList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeListValue;
		paramValue.valListValue = attrValue;
	}

	PluginAttr(const std::string &attrName, const VUtils::IntRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListInt;
		paramValue.valRawListInt = attrValue;
	}

	PluginAttr(const std::string &attrName, const VUtils::FloatRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListFloat;
		paramValue.valRawListFloat = attrValue;
	}

	PluginAttr(const std::string &attrName, const VUtils::VectorRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListVector;
		paramValue.valRawListVector = attrValue;
	}

	PluginAttr(const std::string &attrName, const VUtils::ColorRefList &attrValue) {
		paramName = attrName;
		paramType = PluginAttr::AttrTypeRawListColor;
		paramValue.valRawListColor = attrValue;
	}

	const char *typeStr() const {
		switch (paramType) {
			case AttrTypeInt: return "Int";
			case AttrTypeFloat: return "Float";
			case AttrTypeVector: return "Vector";
			case AttrTypeColor: return "Color";
			case AttrTypeAColor: return "AColor";
			case AttrTypeTransform: return "Transform";
			case AttrTypeString: return "String";
			case AttrTypePlugin: return "Plugin";
			case AttrTypeListInt: return "ListInt";
			case AttrTypeListFloat: return "ListFloat";
			case AttrTypeListVector: return "ListVector";
			case AttrTypeListColor: return "ListColor";
			case AttrTypeListTransform: return "ListTransform";
			case AttrTypeListString: return "ListString";
			case AttrTypeListPlugin: return "ListPlugin";
			case AttrTypeListValue: return "ListValue";
			case AttrTypeRawListInt: return "RawListInt";
			case AttrTypeRawListFloat: return "RawListFloat";
			case AttrTypeRawListVector: return "RawListVector";
			case AttrTypeRawListColor: return "RawListColor";
			default:
				break;
		}
		return "AttrTypeUnknown";
	}

	struct PluginAttrValue {
		int                 valInt;
		float               valFloat;
		float               valVector[4];
		std::string         valString;
		VRay::Plugin        valPlugin;
		std::string         valPluginOutput;
		VRay::Transform     valTransform;

		VRay::IntList       valListInt;
		VRay::FloatList     valListFloat;
		VRay::VectorList    valListVector;
		VRay::ValueList     valListValue;
		VRay::ColorList     valListColor;

		VUtils::ColorRefList  valRawListColor;
		VUtils::VectorRefList valRawListVector;
		VUtils::IntRefList    valRawListInt;
		VUtils::FloatRefList  valRawListFloat;
	} paramValue;

	std::string             paramName;
	AttrType                paramType;

};
typedef std::vector<PluginAttr> PluginAttrs;


struct PluginDesc {
	PluginDesc() {}

	PluginDesc(OP_Node *op_node, const std::string &pluginID):
		pluginID(pluginID),
		pluginName(PluginDesc::GetPluginName(op_node))
	{}

	PluginDesc(OP_Node *op_node, const std::string &pluginID, const std::string &namePrefix):
		pluginID(pluginID),
		pluginName(namePrefix)
	{
		pluginName.append(PluginDesc::GetPluginName(op_node));
	}

	PluginDesc(const std::string &pluginName, const std::string &pluginID):
		pluginName(pluginName),
		pluginID(pluginID)
	{}

	void addAttribute(const PluginAttr &attr) {
		PluginAttr *_attr = get(attr.paramName);
		if (_attr) {
			*_attr = attr;
		}
		else {
			pluginAttrs.push_back(attr);
		}
	}

	std::string             pluginID;
	std::string             pluginName;
	PluginAttrs             pluginAttrs;

	bool contains(const std::string &paramName) const {
		if (get(paramName)) {
			return true;
		}
		return false;
	}

	const PluginAttr *get(const std::string &paramName) const {
		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}

	PluginAttr *get(const std::string &paramName) {
		for (auto &pIt : pluginAttrs) {
			PluginAttr &p = pIt;
			if (paramName == p.paramName) {
				return &p;
			}
		}
		return nullptr;
	}

	void showAttributes() const {
		PRINT_INFO("Plugin \"%s.%s\" parameters:",
				   pluginID.c_str(), pluginName.c_str())
		for (const auto &pIt : pluginAttrs) {
			const PluginAttr &p = pIt;
			PRINT_INFO("  %s [%s]",
					   p.paramName.c_str(), p.typeStr());
		}
	}

	static std::string GetPluginName(OP_Node *op_node, const std::string &prefix="", const std::string &suffix="") {
		std::string pluginName = prefix + op_node->getName().buffer();
		pluginName.append("|");
		pluginName.append(op_node->getParentNetwork()->getName().buffer());
		pluginName.append(suffix);
		return pluginName;
	}
};

} // namespace Attrs


typedef boost::function<void (void)>                                                          CbVoid;

typedef boost::function<void (VRay::VRayRenderer&)>                                           CbOnRendererClose;
typedef boost::function<void (VRay::VRayRenderer&)>                                           CbOnImageReady;

typedef boost::function<void (VRay::VRayRenderer&, VRay::VRayImage*)>                         CbOnRTImageUpdated;

typedef boost::function<void (VRay::VRayRenderer&, int, int, int, int, const char*)>          CbOnBucketInit;
typedef boost::function<void (VRay::VRayRenderer&, int, int, int, int, const char*)>          CbOnBucketFailed;
typedef boost::function<void (VRay::VRayRenderer&, int, int, const char*, VRay::VRayImage*)>  CbOnBucketReady;

typedef boost::function<void (VRay::VRayRenderer&, const char*, int)>                         CbOnDumpMessage;
typedef boost::function<void (VRay::VRayRenderer&, const char*, int, int)>                    CbOnProgress;

template <typename CbT>
struct CbBase
{
	typedef std::vector<CbVoid>  CbVoidArray;
	typedef std::vector<CbT>     CbTypeArray;

	void add(CbT cb)    { m_cbTyped.push_back(cb); }
	void add(CbVoid cb) { m_cbVoid.push_back(cb);  }

	void clear() {
		m_cbVoid.clear();
		m_cbTyped.clear();
	}

	CbVoidArray m_cbVoid;
	CbTypeArray m_cbTyped;
};


typedef CbBase<CbOnRendererClose>   CbSetOnRendererClose;
typedef CbBase<CbOnImageReady>      CbSetOnImageReady;
typedef CbBase<CbOnRTImageUpdated>  CbSetOnRTImageUpdated;
typedef CbBase<CbOnBucketInit>      CbSetOnBucketInit;
typedef CbBase<CbOnBucketFailed>    CbSetOnBucketFailed;
typedef CbBase<CbOnBucketReady>     CbSetOnBucketReady;
typedef CbBase<CbOnDumpMessage>     CbSetOnDumpMessage;
typedef CbBase<CbOnProgress>        CbSetOnProgress;


struct CbCollection {
	void clear() {
		m_cbOnRendererClose.clear();
		m_cbOnImageReady.clear();
		m_cbOnRTImageUpdated.clear();
		m_cbOnBucketInit.clear();
		m_cbOnBucketFailed.clear();
		m_cbOnBucketReady.clear();
		m_cbOnDumpMessage.clear();
		m_cbOnProgress.clear();
	}

	CbSetOnRendererClose   m_cbOnRendererClose;
	CbSetOnImageReady      m_cbOnImageReady;
	CbSetOnRTImageUpdated  m_cbOnRTImageUpdated;
	CbSetOnBucketInit      m_cbOnBucketInit;
	CbSetOnBucketFailed    m_cbOnBucketFailed;
	CbSetOnBucketReady     m_cbOnBucketReady;
	CbSetOnDumpMessage     m_cbOnDumpMessage;
	CbSetOnProgress        m_cbOnProgress;
};


class VRayPluginRenderer {
	struct PluginUsed {
		PluginUsed() {}
		PluginUsed(const VRay::Plugin &p):
			plugin(p),
			used(true)
		{}

		VRay::Plugin  plugin;
		int           used;
	};

	typedef VUtils::HashMap<PluginUsed> PluginUsage;

public:
	VRayPluginRenderer();
	~VRayPluginRenderer();

	void                          init(int reInit=false);
	void                          freeMem();
	void                          setImageSize(const int w, const int h);
	void                          setMode(int mode);

	VRay::Plugin                  exportPlugin(const Attrs::PluginDesc &pluginDesc);
	void                          resetObjects();
	void                          syncObjects();

	void                          setAnimation(bool on);
	void                          setFrame(fpreal frame);
	int                           clearFrames(fpreal toTime);

	int                           exportScene(const std::string &filepath);
	int                           startRender(int locked=false);
	int                           startSequence(int start, int end, int step, int locked=false);

	void                          stopRender();

	int                           isRtRunning();

	void                          showVFB(const bool show=true) { m_vray->showFrameBuffer(show, true); }

private:
	VRay::Plugin                  newPlugin(const Attrs::PluginDesc &pluginDesc);

public:
	VRay::VRayRenderer           *m_vray;
	PluginUsage                   m_pluginUsage;

	static VRay::VRayInit        *g_vrayInit;

	static void                   VRayInit();
	static void                   VRayDone();

public:
	void                          resetCallbacks();

	void                          addCbOnRendererClose(CbOnRendererClose cb)   { m_callbacks.m_cbOnRendererClose.add(cb); }
	void                          addCbOnRendererClose(CbVoid cb)              { m_callbacks.m_cbOnRendererClose.add(cb); }
	void                          addCbOnImageReady(CbOnImageReady cb)         { m_callbacks.m_cbOnImageReady.add(cb); }
	void                          addCbOnImageReady(CbVoid cb)                 { m_callbacks.m_cbOnImageReady.add(cb); }
	void                          addCbOnRTImageUpdated(CbOnRTImageUpdated cb) { m_callbacks.m_cbOnRTImageUpdated.add(cb); }
	void                          addCbOnRTImageUpdated(CbVoid cb)             { m_callbacks.m_cbOnRTImageUpdated.add(cb); }
	void                          addCbOnBucketInit(CbOnBucketInit cb)         { m_callbacks.m_cbOnBucketInit.add(cb); }
	void                          addCbOnBucketInit(CbVoid cb)                 { m_callbacks.m_cbOnBucketInit.add(cb); }
	void                          addCbOnBucketReady(CbOnBucketReady cb)       { m_callbacks.m_cbOnBucketReady.add(cb); }
	void                          addCbOnBucketReady(CbVoid cb)                { m_callbacks.m_cbOnBucketReady.add(cb); }
	void                          addCbOnBucketFailed(CbOnBucketFailed cb)     { m_callbacks.m_cbOnBucketFailed.add(cb); }
	void                          addCbOnBucketFailed(CbVoid cb)               { m_callbacks.m_cbOnBucketFailed.add(cb); }
	void                          addCbOnDumpMessage(CbOnDumpMessage cb)       { m_callbacks.m_cbOnDumpMessage.add(cb); }
	void                          addCbOnDumpMessage(CbVoid cb)                { m_callbacks.m_cbOnDumpMessage.add(cb); }
	void                          addCbOnProgress(CbOnProgress cb)             { m_callbacks.m_cbOnProgress.add(cb); }
	void                          addCbOnProgress(CbVoid cb)                   { m_callbacks.m_cbOnProgress.add(cb); }

	CbCollection                  m_callbacks;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_PLUGIN_EXPORTER_H
