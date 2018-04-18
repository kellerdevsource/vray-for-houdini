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
		: flipAxis(0)
	{}

	explicit SettingsWrapper(const VUtils::Vrscene::Preview::VrsceneSettings &other)
		: settings(other)
		, flipAxis(0)
	{}

	bool operator ==(const SettingsWrapper &other) const {
		return settings == other.settings &&
		       VUtils::isEqual(objectName, other.objectName) &&
		       flipAxis == other.flipAxis;
	}

	bool operator !=(const SettingsWrapper &other) const {
		return !(*this == other);
	}

	bool operator <(const SettingsWrapper &other) const {
		return settings.previewFacesCount < other.settings.previewFacesCount;
	}

	Hash::MHash getHash() const {
		Hash::MHash nameHash = 0;
		if (!objectName.empty()) {
			Hash::MurmurHash3_x86_32(objectName.ptr(), objectName.length(), 42, &nameHash);
		}

#pragma pack(push, 1)
		struct SettingsKey {
			int usePreview;
			int previewFacesCount;
			int minPreviewFaces;
			int masPreviewFaces;
			int previewType;
			uint32 previewFlags;
			int shouldFlip;
			Hash::MHash nameHash;
		} settingsKey = {
			settings.usePreview,
			settings.previewFacesCount,
			settings.minPreviewFaces,
			settings.maxPreviewFaces,
			settings.previewType,
			settings.previewFlags,
			flipAxis,
			nameHash
		};
#pragma pack(pop)

		Hash::MHash keyHash;
		Hash::MurmurHash3_x86_32(&settingsKey, sizeof(SettingsKey), 42, &keyHash);

		return keyHash;
	}
	
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

	VRay::VUtils::CharStringRefList getObjectNames() const;

private:
	int detailRebuild() VRAY_OVERRIDE;

	/// Returns mesh sample time based on animation overrides settings.
	/// @param t Current time.
	double getFrame(fpreal t) const;

	void updateCacheRelatedVars();

	VUtils::CharString vrsceneFile;

	SettingsWrapper getSettings() const;

	VUtils::Vrscene::Preview::VrsceneSettings vrsSettings;
};

} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYSCENEREF_H
