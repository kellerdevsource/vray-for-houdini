//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_INCLUDES_H
#define VRAY_FOR_HOUDINI_INCLUDES_H

#include <UT/UT_Version.h>
#include <UT/UT_Interrupt.h>

#define HDK_16_0_633 UT_MAJOR_VERSION_INT >= 16 && UT_MINOR_VERSION_INT >= 0
#define HDK_16_5 UT_MAJOR_VERSION_INT >= 16 && UT_MINOR_VERSION_INT >= 5

#if HDK_16_5
#define GET_SET_ARG_SEP ,
#define GET_SET_ARG_PRIM_FWD prim

#define GET_SET_ARG_PRIM_SINGLE const GU_PrimPacked *prim
#define GET_SET_ARG_PRIM GET_SET_ARG_PRIM_SINGLE,

#define SET_ARG_PRIM_SINGLE GU_PrimPacked *prim
#define SET_ARG_PRIM SET_ARG_PRIM_SINGLE GET_SET_ARG_SEP

#define GET_PRIM_SINGLE const_cast<GU_PrimPacked*>(getPrim())
#define GET_PRIM GET_PRIM_SINGLE GET_SET_ARG_SEP
#else
#define GET_SET_ARG_SEP
#define GET_SET_ARG_PRIM_FWD

#define GET_SET_ARG_PRIM_SINGLE
#define GET_SET_ARG_PRIM

#define SET_ARG_PRIM_SINGLE
#define SET_ARG_PRIM

#define GET_PRIM
#endif

#endif // VRAY_FOR_HOUDINI_INCLUDES_H
