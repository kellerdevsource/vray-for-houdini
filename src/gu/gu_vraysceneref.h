//
// Copyright (c) 2015-2018, Chaos Software Ltd
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

#include <vrscene_preview.h>

namespace VUtils {
namespace Vrscene {
namespace Preview {
struct VrsceneDesc;
} // Preview
} // Vrscene
}

namespace VRayForHoudini {

extern VUtils::Vrscene::Preview::VrsceneDescManager vrsceneMan;

struct SettingsWrapper {
	SettingsWrapper()
		: flipAxis(false)
	{}

	explicit SettingsWrapper(const VUtils::Vrscene::Preview::VrsceneSettings &other)
		: settings(other)
		, flipAxis(false)
	{}

	Hash::MHash getHash() const;

	bool operator ==(const SettingsWrapper &other) const;
	bool operator !=(const SettingsWrapper &other) const;
	bool operator <(const SettingsWrapper &other) const;

	VUtils::Vrscene::Preview::VrsceneSettings settings;
	VUtils::CharString objectName;
	int flipAxis;
};

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

	/// Returns Node plugin names list from the *.vrscene
	/// this primitive is loading.
	VRay::VUtils::CharStringRefList getObjectNames() const;

private:
	int detailRebuild() VRAY_OVERRIDE;

	/// Returns mesh sample time based on animation overrides settings.
	/// @param t Current time.
	double getFrame(fpreal t) const;

	/// Get cache entry settings.
	SettingsWrapper getSettings() const;

	/// Currently loaded *.vrscene file path.
	VUtils::CharString filePath;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYSCENEREF_H
