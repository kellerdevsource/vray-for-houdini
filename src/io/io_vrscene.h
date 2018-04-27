//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IO_Vrscene_H
#define VRAY_FOR_HOUDINI_IO_Vrscene_H

#include "vfh_vray.h"

#include <GEO/GEO_IOTranslator.h>

namespace VRayForHoudini {
namespace IO {

class Vrscene
	: public GEO_IOTranslator
{
public:
	static const char *const fileExtension;

	GEO_IOTranslator *duplicate() const VRAY_OVERRIDE;
	const char *formatName() const VRAY_OVERRIDE;
	int checkExtension(const char *name) VRAY_OVERRIDE;
	int checkMagicNumber(unsigned magic) VRAY_OVERRIDE;
	GA_Detail::IOStatus fileLoad(GEO_Detail *geo, UT_IStream &stream, bool ate_magic) VRAY_OVERRIDE;
	GA_Detail::IOStatus fileSave(const GEO_Detail *geo, std::ostream &stream) VRAY_OVERRIDE;
};

} // namespace IO
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_IO_Vrscene_H
