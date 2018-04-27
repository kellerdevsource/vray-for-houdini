//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//
// Our "patched" version of UT/UT_DSOVersion.h
//

#ifndef __VFH_DSO_VERSION_H__
#define __VFH_DSO_VERSION_H__

#include <SYS/SYS_Types.h>
#include <SYS/SYS_Version.h>
#include <SYS/SYS_Visibility.h>

#define UT_DSO_VERSION	SYS_VERSION_RELEASE
#define UT_DSOVERSION_EXPORT extern "C" SYS_VISIBILITY_EXPORT

UT_DSOVERSION_EXPORT void HoudiniDSOVersion(const char **dsoVersion)
{
	*dsoVersion = UT_DSO_VERSION;
}

UT_DSOVERSION_EXPORT void HoudiniGetTagInfo(const char **tagInfo)
{
	*tagInfo = UT_DSO_TAGINFO;
}

#if defined(__GNUC__)
UT_DSOVERSION_EXPORT unsigned HoudiniCompilerVersion()
{
	return (__GNUC__ * 100 + __GNUC_MINOR__);
}
#elif defined(_MSC_VER)
UT_DSOVERSION_EXPORT unsigned HoudiniCompilerVersion()
{
	// NOTE: We're using MSVS 2017, but it's not yet
	// officially compatible with Houdini.
	return 1900;
}
#endif

#endif // __VFH_DSO_VERSION_H__
