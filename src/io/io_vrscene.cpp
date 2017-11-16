//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#include "io_vrscene.h"

#include "vfh_log.h"
#include "vfh_includes.h"

#include <CH/CH_Manager.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPacked.h>

using namespace VRayForHoudini;
using namespace IO;

const char *const Vrscene::fileExtension = "vrscene";

GEO_IOTranslator* Vrscene::duplicate() const
{
	return new Vrscene(*this);
}

const char *Vrscene::formatName() const
{
	return "V-Ray Standalone Scene Format";
}

int Vrscene::checkExtension(const char *name)
{
	UT_String sname(name);
	const char *ext = sname.fileExtension();
	return UTisstring(ext) && !SYSstrcasecmp(ext+1, fileExtension);
}

int Vrscene::checkMagicNumber(unsigned)
{
	return 0;
}

GA_Detail::IOStatus Vrscene::fileSave(const GEO_Detail*, std::ostream&)
{
	return GA_Detail::IOStatus(false);
}

GA_Detail::IOStatus Vrscene::fileLoad(GEO_Detail *geo, UT_IStream &stream, bool)
{
	GU_Detail *gdp = static_cast<GU_Detail*>(geo);
	if (!gdp) {
		gdp->addWarning(GU_WARNING_NO_METAOBJECTS, "Invalid detail!");
		return GA_Detail::IOStatus(false);
	}

	const char *filepath = stream.getFilename();

	if (!UTisstring(filepath) ||
		!VUtils::uniPathOrFileExists(filepath))
	{
		gdp->addWarning(GU_WARNING_NO_METAOBJECTS, "Invalid filepath!");
		return GA_Detail::IOStatus(false);
	}

	Log::getLog().debug("Vrscene::fileLoad(%s)", filepath);

	GU_PrimPacked *pack = GU_PrimPacked::build(*gdp, "VRaySceneRef");
	if (!pack) {
		gdp->addWarning(GU_WARNING_NO_METAOBJECTS,  "Can't create packed primitive VRaySceneRef");
		return GA_Detail::IOStatus(false);
	}

	const UT_Vector3 pivot(0.0, 0.0, 0.0);
	pack->setPivot(pivot);
	gdp->setPos3(pack->getPointOffset(0), pivot);

	UT_Options options;
	options.setOptionS("filepath", filepath);

	GU_PackedImpl *primImpl = pack->implementation();
	if (primImpl) {
#if HDK_16_5
		primImpl->update(pack, options);
#else
		primImpl->update(options);
#endif
	}

	return GA_Detail::IOStatus(true);
}
