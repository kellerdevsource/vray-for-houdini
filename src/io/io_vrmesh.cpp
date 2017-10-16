//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "io_vrmesh.h"

#include "vfh_log.h"
#include "vfh_vrayproxyutils.h"
#include "uni.h"

#include <CH/CH_Manager.h>
#include <OP/OP_Context.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPacked.h>
#include <UT/UT_Assert.h>


using namespace VRayForHoudini;
using namespace VRayForHoudini::IO;


const char *const Vrmesh::extension = "vrmesh";


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
	const char *ext = sname.fileExtension();
	// NOTE: +1 to skip dot
	return (UTisstring(ext) && !SYSstrcasecmp(ext+1, Vrmesh::extension));
}


int Vrmesh::checkMagicNumber(unsigned magic)
{
	return 0;
}


GA_Detail::IOStatus Vrmesh::fileLoad(GEO_Detail *geo, UT_IStream &stream, bool /*ate_magic*/)
{
	const char *filepath = stream.getFilename();
	GU_Detail  *gdp = dynamic_cast<GU_Detail*>(geo);

	if (NOT(gdp)) {
		return GA_Detail::IOStatus(false);
	}

	if (   NOT(UTisstring(filepath))
		|| NOT(VUtils::uniPathOrFileExists(filepath)) )
	{
		gdp->addWarning(GU_WARNING_NO_METAOBJECTS, "Invalid filepath.");
		return GA_Detail::IOStatus(false);
	}

	Log::getLog().debug("Vrmesh::fileLoad(%s)", filepath);

	GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRayProxyRef");
	if (NOT(pack)) {
		gdp->addWarning(GU_WARNING_NO_METAOBJECTS, "Can't create packed primitive VRayProxyRef.");
		return GA_Detail::IOStatus(false);
	}

	// Set the location of the packed primitive's point.
	UT_Vector3 pivot(0, 0, 0);
	pack->setPivot(pivot);
	gdp->setPos3(pack->getPointOffset(0), pivot);

	pack->setViewportLOD(GEO_VIEWPORT_FULL);

	OP_Context context(CHgetEvalTime());
	UT_Options options;
	options.setOptionI("lod", LOD_PREVIEW)
			.setOptionF("frame", context.getFloatFrame())
			.setOptionS("file", filepath);

	pack->implementation()->update(options);

	return GA_Detail::IOStatus(true);
}


GA_Detail::IOStatus Vrmesh::fileSave(const GEO_Detail *, std::ostream &)
{
	// TODO: implement
	return GA_Detail::IOStatus(false);
}
