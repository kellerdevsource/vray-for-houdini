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

#include "sop_vrayproxy.h"

#include <GEO/GEO_Point.h>
#include <GU/GU_PrimPoly.h>

#include <bmpbuffer.h> // For bmpCheckAssetPath


using namespace VRayForHoudini;


void SOP::VRayProxy::setPluginType()
{
	pluginType = "GEOMETRY";
	pluginID   = "GeomMeshFile";
}


OP_NodeFlags &SOP::VRayProxy::flags()
{
	OP_NodeFlags &flags = SOP_Node::flags();

	const bool is_animated = evalInt("anim_type", 0, 0.0f) != 3;
	flags.setTimeDep(is_animated);

	return flags;
}


OP_ERROR SOP::VRayProxy::cookMySop(OP_Context &context)
{
	PRINT_INFO("SOP::VRayProxy::cookMySop()");

	const float t = context.getTime();

	UT_String path;
	evalString(path, "file", 0, t);
	if (path.equal("")) {
		return error();
	}

	const bool flipAxis = evalInt("flip_axis", 0, 0.0f);
	const float scale   = evalFloat("scale", 0, 0.0f);

	gdp->clearAndDestroy();

	VUtils::CharString filepath = path.buffer();
	if (VUtils::bmpCheckAssetPath(filepath, NULL, NULL, false)) {
		VUtils::MeshFile *proxy = VUtils::newDefaultMeshFile(filepath.ptr());

		int res = proxy->init(filepath.ptr());
		if (res && proxy->getNumVoxels()) {
			if(error() < UT_ERROR_ABORT) {
				UT_Interrupt *boss = UTgetInterrupt();

				if(boss->opStart("Building V-Ray Scene Preview Mesh")) {
					VUtils::MeshVoxel *previewVoxel = proxy->getVoxel(proxy->getNumVoxels() - 1);

					VUtils::VertGeomData *verts = previewVoxel->getVertGeomData();
					VUtils::FaceTopoData *faces = previewVoxel->getFaceTopoData();

					const int numPreviewFaces = previewVoxel->getNumFaces();
					const int numPreviewVerts = numPreviewFaces * 3;

					// Points
					for (int v = 0; v < numPreviewVerts; ++v) {
						VUtils::Vector vert = verts[v];
						if (flipAxis) {
							vert = gFlipMatrix * vert;
						}

						VUtils::Matrix tm;
						tm.f[0].set(scale, 0.0f, 0.0f);
						tm.f[1].set(0.0f, scale, 0.0f);
						tm.f[2].set(0.0f, 0.0f, scale);

						vert = tm * vert;

#if UT_MAJOR_VERSION_INT < 14
						GEO_Point *point = gdp->appendPointElement();
						point->setPos(UT_Vector4F(vert.x, vert.y, vert.z));
#else
						GA_Offset pointOffs = gdp->appendPoint();
						gdp->setPos3(pointOffs, UT_Vector4F(vert.x, vert.y, vert.z));
#endif
					}

					// Faces
					for (int f = 0; f < numPreviewFaces; ++f) {
						const VUtils::FaceTopoData &face = faces[f];

						GU_PrimPoly *poly = GU_PrimPoly::build(gdp, 3, GU_POLY_CLOSED, 0);

						for (int c = 0; c < 3; ++c) {
							poly->setVertexPoint(c, face.v[c]);
						}

						poly->reverse();
					}

					proxy->releaseVoxel(previewVoxel);
				}

				boss->opEnd();
			}
		}
	}

#if UT_MAJOR_VERSION_INT < 14
	gdp->notifyCache(GU_CACHE_ALL);
#endif

	return error();
}


OP::VRayNode::PluginResult SOP::VRayProxy::asPluginDesc(Attrs::PluginDesc &pluginDesc, VRayExporter *exporter, OP_Node *parent)
{
	UT_String path;
	evalString(path, "file", 0, 0.0f);
	if (NOT(path.isstring())) {
		PRINT_ERROR("VRayProxy \"%s\": \"File\" is not set!",
					getName().buffer());
		return OP::VRayNode::PluginResultError;
	}

	pluginDesc.pluginID   = pluginID.c_str();
	pluginDesc.pluginName = Attrs::PluginDesc::GetPluginName(this);

	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("file", path.buffer()));
	pluginDesc.pluginAttrs.push_back(Attrs::PluginAttr("flip_axis", evalInt("flip_axis", 0, 0.0f)));

	exporter->setAttrsFromOpNode(pluginDesc, this);

	return OP::VRayNode::PluginResultSuccess;
}
