//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_PCH_H
#define VRAY_FOR_HOUDINI_PCH_H

#include <chrono>
#include <cstdio>
#include <exception>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <stdarg.h>
#include <stdlib.h>

#ifdef _MSC_VER
#  include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/preprocessor.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/unordered_map.hpp>

// Houdini SDK
#include <CH/CH_Channel.h>
#include <CH/CH_LocalVariable.h>
#include <CH/CH_Manager.h>
#include <CMD/CMD_Args.h>
#include <CMD/CMD_Manager.h>
#include <DOP/DOP_Engine.h>
#include <DOP/DOP_Node.h>
#include <DOP/DOP_Parent.h>
#include <EXPR/EXPR_Lock.h>
#include <FS/FS_FileSystem.h>
#include <FS/FS_Info.h>
#include <FS/UT_DSO.h>
#include <GA/GA_AttributeFilter.h>
#include <GA/GA_PageHandle.h>
#include <GA/GA_SaveMap.h>
#include <GA/GA_Types.h>
#include <GEO/GEO_Detail.h>
#include <GEO/GEO_IOTranslator.h>
#include <GEO/GEO_Point.h>
#include <GEO/GEO_Primitive.h>
#include <GEO/GEO_PrimPoly.h>
#include <GT/GT_GEOAttributeFilter.h>
#include <GT/GT_GEODetail.h>
#include <GT/GT_GEOPrimCollect.h>
#include <GT/GT_GEOPrimPacked.h>
#include <GT/GT_PrimInstance.h>
#include <GU/GU_Detail.h>
#include <GU/GU_DetailHandle.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedFactory.h>
#include <GU/GU_PackedGeometry.h>
#include <GU/GU_PackedImpl.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PrimPolySoup.h>
#include <GU/GU_PrimVDB.h>
#include <GU/GU_PrimVolume.h>
#include <HOM/HOM_BaseKeyframe.h>
#include <HOM/HOM_EnumModules.h>
#include <HOM/HOM_Module.h>
#include <HOM/HOM_playbar.h>
#include <HOM/HOM_Ramp.h>
#include <HOM/HOM_Vector2.h>
#include <OBJ/OBJ_Camera.h>
#include <OBJ/OBJ_Geometry.h>
#include <OBJ/OBJ_Light.h>
#include <OBJ/OBJ_Node.h>
#include <OBJ/OBJ_SubNet.h>
#include <OP/OP_AutoLockInputs.h>
#include <OP/OP_Bundle.h>
#include <OP/OP_BundleList.h>
#include <OP/OP_Context.h>
#include <OP/OP_Director.h>
#include <OP/OP_Input.h>
#include <OP/OP_Network.h>
#include <OP/OP_Node.h>
#include <OP/OP_NodeInfoParms.h>
#include <OP/OP_OperatorTable.h>
#include <OP/OP_Options.h>
#include <OP/OP_SaveFlags.h>
#include <OP/OP_SpareParms.h>
#include <OpenEXR/ImathLimits.h>
#include <OpenEXR/ImathMath.h>
#include <POP/POP_Node.h>
#include <POPNET/POPNET_Node.h>
#include <PRM/DS_Stream.h>
#include <PRM/PRM_ChoiceList.h>
#include <PRM/PRM_Include.h>
#include <PRM/PRM_Name.h>
#include <PRM/PRM_Parm.h>
#include <PRM/PRM_ParmMicroNode.h>
#include <PRM/PRM_ParmOwner.h>
#include <PRM/PRM_RampUtils.h>
#include <PRM/PRM_Range.h>
#include <PRM/PRM_ScriptPage.h>
#include <PRM/PRM_ScriptParm.h>
#include <PRM/PRM_Shared.h>
#include <PRM/PRM_SpareData.h>
#include <PRM/PRM_Template.h>
#include <ROP/ROP_Error.h>
#include <ROP/ROP_Node.h>
#include <ROP/ROP_Templates.h>
#include <SHOP/SHOP_GeoOverride.h>
#include <SHOP/SHOP_Node.h>
#include <SHOP/SHOP_Operator.h>
#include <SHOP/SHOP_Util.h>
#include <SIM/SIM_Geometry.h>
#include <SIM/SIM_GeometryCopy.h>
#include <SIM/SIM_Position.h>
#include <SOP/SOP_Node.h>
#include <SYS/SYS_Math.h>
#include <SYS/SYS_Types.h>
#include <UT/UT_Assert.h>
#include <UT/UT_BoundingBox.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Exit.h>
#include <UT/UT_Interrupt.h>
#include <UT/UT_IOTable.h>
#include <UT/UT_IStream.h>
#include <UT/UT_MemoryCounter.h>
#include <UT/UT_NTStreamUtil.h>
#include <UT/UT_Options.h>
#include <UT/UT_StackTrace.h>
#include <UT/UT_String.h>
#include <VOP/VOP_ExportedParmsManager.h>
#include <VOP/VOP_LanguageContextTypeList.h>
#include <VOP/VOP_Node.h>
#include <VOP/VOP_Operator.h>
#include <VOP/VOP_OperatorInfo.h>
#include <VOP/VOP_ParmGenerator.h>
#include <VOPNET/VOPNET_Node.h>

#if UT_MAJOR_VERSION_INT >= 16
#include <RE/RE_Window.h>
#else
#include <RE/RE_QtWindow.h>
#endif

// V-Ray Application SDK
#include <vraysdk.hpp>

// Qt
#include <QtCore>
#include <QtGui>
#if UT_MAJOR_VERSION_INT >= 16
#  include <QtWidgets>
#endif

#endif // VRAY_FOR_HOUDINI_PCH_H
