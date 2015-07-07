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

#ifdef CGR_HAS_VRAYSCENE

#include "sop_vrayscene.h"

#include <GEO/GEO_Point.h>
#include <GU/GU_PrimPoly.h>


using namespace VRayForHoudini;


VRayScenePreviewMan  SOP::VRayScene::m_previewMan;


void SOP::VRayScene::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "VRayScene";

	PreviewMeshSample::SetMaxPreviewFaces(2000);
}


OP_NodeFlags &SOP::VRayScene::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();

	const bool is_animated = evalInt("anim_type", 0, 0.0f) != 3;
	flags.setTimeDep(is_animated);

	return flags;
}


OP_ERROR SOP::VRayScene::cookMySop(OP_Context &context)
{
	PRINT_INFO("SOP::VRayScene::cookMySop()");

	const float t = context.getTime();

	UT_String path;
	evalString(path, "filepath", 0, t);
	if (path.equal("")) {
		return error();
	}

	const bool flipAxis = evalInt("flip_axis", 0, 0.0f);

	VrscenePreview *previewData = m_previewMan.getPreview(path.buffer());
	if (!previewData) {
		return error();
	}

	if (!previewData->meshes.size()) {
		previewData->loadData(0, 1);
	}

	const PreviewMeshList &meshesList = previewData->getPreviewData((int)t);

	if(error() < UT_ERROR_ABORT) {
		UT_Interrupt *boss = UTgetInterrupt();

		gdp->clearAndDestroy();

		if(boss->opStart("Building V-Ray Scene Preview Mesh")) {
			unsigned meshVertexOffset = 0;

			for (PreviewMeshList::const_iterator previewIt = meshesList.begin(); previewIt != meshesList.end(); ++previewIt) {
				const PreviewMeshSample *meshSample = *previewIt;

				// Points
				for (int v = 0; v < meshSample->numPreviewVerts; ++v) {
					VUtils::Vector vert = meshSample->tm * meshSample->previewVerts[v];
					if (flipAxis) {
						vert = gFlipMatrix * vert;
					}

#if UT_MAJOR_VERSION_INT < 14
					GEO_Point *point = gdp->appendPointElement();
					point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
					GA_Offset pointOffs = gdp->appendPoint();
					gdp->setPos3(pointOffs, UT_Vector4F(vert.x, vert.y, vert.z));
#endif
				}

				// Faces
				for (int f = 0; f < meshSample->numPreviewFaces; ++f) {
					const VUtils::FaceTopoData &face = meshSample->previewFaces[f];

					GU_PrimPoly *poly = GU_PrimPoly::build(gdp, 3, GU_POLY_CLOSED, 0);

					for (int c = 0; c < 3; ++c) {
						poly->setVertexPoint(c, meshVertexOffset+face.v[c]);
					}

					poly->reverse();
				}

				meshVertexOffset += meshSample->numPreviewVerts;
			}
		}

		boss->opEnd();
	}

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	return error();
}


OP::VRayNode::PluginResult SOP::VRayScene::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	UT_String path;
	evalString(path, "filepath", 0, 0.0f);
	if (NOT(path.isstring())) {
		PRINT_ERROR("VRayScene \"%s\": \"Filepath\" is not set!",
					getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = Attrs::PluginDesc::GetPluginName(this);

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("filepath", path.buffer()));

	exporter->setAttrsFromOpNode(pluginDesc, this);

	return OP::VRayNode::PluginResultSuccess;
}

#endif // CGR_HAS_VRAYSCENE
