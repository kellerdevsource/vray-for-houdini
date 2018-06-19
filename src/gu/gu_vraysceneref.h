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

#include <OP/OP_Options.h>

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

class VRaySceneRef;

struct SettingsWrapper {
	SettingsWrapper() = default;

	explicit SettingsWrapper(const VUtils::Vrscene::Preview::VrsceneSettings &settings)
		: settings(settings)
	{}

	SettingsWrapper(const SettingsWrapper &other)
		: settings(other.settings)
		, objectPath(other.objectPath)
		, addNodes(other.addNodes)
		, addLights(other.addLights)
		, flipAxis(other.flipAxis)
	{
		options.merge(other.options);
	}

	/// Returns current settings hash.
	Hash::MHash getHash() const;

	VUtils::Vrscene::Preview::VrsceneSettings settings;
	VUtils::CharString objectPath;
	int addNodes = true;
	int addLights = true;
	int flipAxis = false;

	OP_Options options;
};

/// VRayScene preview mesh implemented as a packed primitive.
class VRaySceneRef
	: public VRaySceneRefBase
{
public:
	static GA_PrimitiveTypeId typeId();
	static void install(GA_PrimitiveFactory *primFactory);

	VRaySceneRef();
	VRaySceneRef(const VRaySceneRef &src);
	virtual ~VRaySceneRef();

	// From GU_PackedImpl.
	GU_PackedFactory *getFactory() const VRAY_OVERRIDE;
	GU_PackedImpl *copy() const VRAY_OVERRIDE;
	bool unpack(GU_Detail &destGdp) const VRAY_OVERRIDE;

	/// Collect plugin names list from the *.vrscene that this primitive is loading.
	VRay::VUtils::CharStringRefList getObjectNamesFromPath() const;

private:
	int detailRebuild(GU_PrimPacked *prim) VRAY_OVERRIDE;

	/// Returns mesh sample time based on animation overrides settings.
	/// @param t Current time.
	double getFrame(fpreal t) const;

	/// Get cache entry settings.
	SettingsWrapper getSettings() const;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYSCENEREF_H
