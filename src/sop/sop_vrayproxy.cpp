//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_prm_templates.h"
#include "sop_vrayproxy.h"
#include "gu_vrayproxyref.h"

#include <GU/GU_PrimPacked.h>

using namespace VRayForHoudini;

PRM_Template* SOP::VRayProxy::getPrmTemplate()
{
	PRM_Template *prmTemplate = Parm::getPrmTemplate("GeomMeshFile");
	while (prmTemplate && prmTemplate->getType() != PRM_LIST_TERMINATOR) {
		if (vutils_strcmp(prmTemplate->getToken(), "reload") == 0) {
			prmTemplate->setCallback(cbClearCache);
			break;
		}
		prmTemplate++;
	}
	return prmTemplate;
}

int SOP::VRayProxy::cbClearCache(void *data, int /*index*/, fpreal t, const PRM_Template* /*tplate*/)
{
	OP_Node *node = reinterpret_cast<OP_Node*>(data);

	UT_String filepath;
	node->evalString(filepath, "file", 0, t);

	if (filepath.isstring()) {
		ClearVRayProxyCache(filepath);
	}

	return 0;
}

SOP::VRayProxy::VRayProxy(OP_Network *parent, const char *name, OP_Operator *entry)
	: NodeBase(parent, name, entry)
{
	// This indicates that this SOP manually manages its data IDs,
	// so that Houdini can identify what attributes may have changed,
	// e.g. to reduce work for the viewport, or other SOPs that
	// check whether data IDs have changed.
	// By default, (i.e. if this msg weren't here), all data IDs
	// would be bumped after the SOP cook, to indicate that
	// everything might have changed.
	// If some data IDs don't get bumped properly, the viewport
	// may not update, or SOPs that check data IDs
	// may not cook correctly, so be *very* careful!
	mySopFlags.setManagesDataIDs(true);
}

void SOP::VRayProxy::setPluginType()
{
	pluginType = VRayPluginType::GEOMETRY;
	pluginID   = "GeomMeshFile";
}

void SOP::VRayProxy::setTimeDependent()
{
	const VUtils::MeshFileAnimType::Enum animType =
		static_cast<VUtils::MeshFileAnimType::Enum>(evalInt("anim_type", 0, 0.0));

	previewMeshAnimated = animType != VUtils::MeshFileAnimType::Still;

	flags().setTimeDep(previewMeshAnimated);
}

void SOP::VRayProxy::updatePrimitive(const OP_Context &context)
{
	vassert(m_primPacked);

	const fpreal t = context.getTime();

	// Set the options on the primitive
	OP_Options primOptions;
	for (int i = 0; i < getParmList()->getEntries(); ++i) {
		const PRM_Parm &prm = getParm(i);
		primOptions.setOptionFromTemplate(this, prm, *prm.getTemplatePtr(), t);
	}

	UT_String viewportlod;
	evalString(viewportlod, "viewportlod", 0, 0.0);
	m_primPacked->setViewportLOD(GEOviewportLOD(viewportlod));
	primOptions.setOptionS("viewportlod", viewportlod);

	UT_String objectPath;
	evalString(objectPath, "object_path", 0, 0.0);
	primOptions.setOptionS("object_path", objectPath);

	primOptions.setOptionI("lod", evalInt("loadtype", 0, 0.0));
	primOptions.setOptionF("current_frame", previewMeshAnimated ? context.getFloatFrame() : 0.0f);

	if (m_primOptions != primOptions) {
		m_primOptions = primOptions;

		GU_PackedImpl *primImpl = m_primPacked->implementation();
		if (primImpl) {
#if HDK_16_5
			primImpl->update(m_primPacked, m_primOptions);
#else
			primImpl->update(m_primOptions);
#endif
		}
	}
}

OP_ERROR SOP::VRayProxy::cookMySop(OP_Context &context)
{
	Log::getLog().debug("SOP::VRayProxy::cookMySop()");

	if (!m_primPacked) {
		m_primPacked = GU_PrimPacked::build(*gdp, "VRayProxyRef");

		// Set the location of the packed primitive point.
		const UT_Vector3 pivot(0.0, 0.0, 0.0);
		m_primPacked->setPivot(pivot);

		gdp->setPos3(m_primPacked->getPointOffset(0), pivot);
	}

	vassert(m_primPacked);

	setTimeDependent();
	updatePrimitive(context);

	return error();
}
