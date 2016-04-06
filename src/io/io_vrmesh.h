//
// Copyright (c) 2015-2016, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_IO_VRMESH_H
#define VRAY_FOR_HOUDINI_IO_VRMESH_H

#include "vfh_vray.h"

#include <GEO/GEO_IOTranslator.h>

namespace VRayForHoudini {
namespace IO {

class Vrmesh
		: public GEO_IOTranslator
{
public:
	constexpr static const char *extension = "vrmesh";

	Vrmesh() {}
	Vrmesh(const Vrmesh&);

	virtual                     ~Vrmesh() {}

	virtual GEO_IOTranslator    *duplicate() const VRAY_OVERRIDE;
	virtual const char          *formatName() const VRAY_OVERRIDE;
	virtual int                  checkExtension(const char *name) VRAY_OVERRIDE;
	virtual int                  checkMagicNumber(unsigned magic) VRAY_OVERRIDE;
	virtual GA_Detail::IOStatus  fileLoad(GEO_Detail *, UT_IStream &, bool ate_magic) VRAY_OVERRIDE;
	virtual GA_Detail::IOStatus  fileSave(const GEO_Detail *, std::ostream &) VRAY_OVERRIDE;
};

} // namespace IO
} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_IO_VRMESH_H
