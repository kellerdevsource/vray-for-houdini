//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYSCENEREF_H
#define VRAY_FOR_HOUDINI_VRAYSCENEREF_H

#include "vfh_includes.h"
#include "vfh_VRaySceneRefBase.h"

namespace VUtils {
namespace Vrscene {
namespace Preview {
struct VrsceneDesc;
} // Preview
} // Vrscene
}

namespace VRayForHoudini {

/// VRayScene preview mesh implemented as a packed primitive.
class VRaySceneRef
	: public VRaySceneRefBase
{
public:
	static GA_PrimitiveTypeId typeId();
	static void install(GA_PrimitiveFactory *gafactory);

	VRaySceneRef();
	VRaySceneRef(const VRaySceneRef &src);
	virtual ~VRaySceneRef();

	// From GU_PackedImpl.
	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;
	GU_PackedImpl *copy() const VRAY_OVERRIDE;
	bool unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;

private:
	int detailRebuild() VRAY_OVERRIDE;

	/// Re-builds *.vrscene preview detail.
	/// @param vrsceneDesc *.vrscene file preview data.
	/// @param flipAxis Flip axis Z-Y.
	int detailRebuild(VUtils::Vrscene::Preview::VrsceneDesc *vrsceneDesc, int flipAxis);

	/// Returns mesh sample time based on animation overrides settings.
	/// @param t Current time.
	double getFrame(fpreal t) const;

	VUtils::CharString vrsceneFile;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYSCENEREF_H
