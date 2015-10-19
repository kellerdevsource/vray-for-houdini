//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "vfh_exporter.h"

#include <OBJ/OBJ_Node.h>

#include <DOP/DOP_Node.h>
#include <DOP/DOP_Parent.h>
#include <DOP/DOP_Engine.h>

#include <SIM/SIM_Position.h>
#include <SIM/SIM_Geometry.h>
#include <SIM/SIM_GeometryCopy.h>

#include <POP/POP_Node.h>
#include <POPNET/POPNET_Node.h>


using namespace VRayForHoudini;


VRay::Plugin VRayExporter::exportParticles(OBJ_Node *dop_network)
{
	POPNET_Node *popnet_node = dop_network->castToPOPNETNode();
	if (popnet_node) {
		PRINT_WARN("POPNET_Node");
	}

	POP_Node *pop_node = dop_network->castToPOPNode();
	if (pop_node) {
		PRINT_WARN("POP_Node");
	}

	const DOP_Parent *dop_parent = dop_network->castToDOPParent();
	if (dop_parent) {
		const DOP_Engine &engine = dop_parent->getEngine();
		int totalDopObjects = engine.getNumSimulationObjects();

		PRINT_WARN("getNumSimulationObjects = %i",
				   totalDopObjects);

		for (int i = 0; i < totalDopObjects; i++) {
			const SIM_Object *currObject = engine.getSimulationObject(i);
#if 0
			const SIM_Position *pos = currObject->getPosition();
			while (pos) {
				UT_Vector3 t;
				pos->getPosition(t);
				PRINT_INFO("Pos: %.3f", t.x());
				pos++;
			}
#endif
			const SIM_Data *sim_data = currObject->getConstNamedSubData("Geometry");
			// sim_data->getPOPNode();
			if (sim_data) {
				PRINT_WARN("SIM_Object = %s; SIM_Object.getNumSubData = %i; SIM_Data.getNumSubData = %i",
						   currObject->getName().buffer(), currObject->getNumSubData(), sim_data->getNumSubData());
			}

			UT_String posPath(SIM_POSITION_DATANAME);
			const SIM_Geometry *geometry = static_cast<const SIM_Geometry*>(currObject->getConstNamedSubData(SIM_GEOMETRY_DATANAME));
			if (geometry) {
				geometry->getPositionPath(posPath);
				PRINT_WARN("HAS GEO: %s",
						   posPath.buffer());

				const SIM_Data *posData = currObject->getConstNamedSubData(posPath.buffer());
				if (NOT(posData)) {
					posData = currObject->getConstNamedSubData("Position");
				}
				if (posData) {
					PRINT_WARN("HAS SOME POS...");
				}

				GU_ConstDetailHandle gdh = geometry->getGeometry();
				const GU_Detail *gdp = gdh.readLock();

				GA_ROAttributeRef ref_sourceuv(gdp->findPointAttribute("sourceuv"));
				GA_ROHandleV2 sourceuv(ref_sourceuv.getAttribute());

				GA_ROAttributeRef ref_instancepath(gdp->findPointAttribute("instancepath"));
				GA_ROHandleS instancepath(ref_instancepath.getAttribute());
#if 0
				for (GA_Iterator it(gdp->getPointRange()); !it.atEnd(); it.advance()) {
					if (sourceuv.isValid()) {
						UT_Vector2F uv(sourceuv.get(it.getOffset()));
						printf("%i sourceuv: (%f, %f)\n", it.getIndex(), uv.x(), uv.y());
					}
					if (instancepath.isValid()) {
						printf("%i object: %s\n", it.getIndex(), instancepath.get(*it));
					}
				}
#endif
				gdh.unlock(gdp);
			}

			const SIM_Position *pos = static_cast<const SIM_Position*>(currObject->getConstNamedSubData(posPath.buffer()));
			if (pos) {
				PRINT_WARN("HAS POSITION");
			}
		}
	}

	return VRay::Plugin();
}
