//
// Copyright (c) 2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "io_vrmesh.h"

#include "vfh_log.h"

#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>
#include <GEO/GEO_AttributeHandle.h>
#include <SOP/SOP_Node.h>
#include <UT/UT_Assert.h>


using namespace VRayForHoudini;
using namespace VRayForHoudini::IO;


Vrmesh::Vrmesh(const Vrmesh &other)
{
	// TODO: If needed
}


GEO_IOTranslator* Vrmesh::duplicate() const
{
	return new Vrmesh(*this);
}


const char *Vrmesh::formatName() const
{
	return "V-Ray Proxy Format";
}


int Vrmesh::checkExtension(const char *name)
{
	UT_String sname(name);
	// NOTE: +1 to skip dot
	if (sname.fileExtension() && !strcmp(sname.fileExtension()+1, Vrmesh::extension)) {
		return true;
	}
	return false;
}


int Vrmesh::checkMagicNumber(unsigned magic)
{
	return 0;
}


GA_Detail::IOStatus Vrmesh::fileLoad(GEO_Detail *, UT_IStream &stream, bool ate_magic)
{
	Log::getLog().info("Vrmesh::fileLoad(%s)", stream.getFilename());

	return GA_Detail::IOStatus(true);
}


GA_Detail::IOStatus Vrmesh::fileSave(const GEO_Detail *, std::ostream &)
{
	return GA_Detail::IOStatus(true);
}
