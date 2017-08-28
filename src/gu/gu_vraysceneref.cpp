#include "misc.h"

#include "gu_vraysceneref.h"

#include "GU/GU_PackedFactory.h"

using namespace VRayForHoudini;

class VRaySceneFactory
	: public GU_PackedFactory
{
public:
	static VRaySceneFactory &getInstance() {
		static VRaySceneFactory theFactory;
		return theFactory;
	}

	GU_PackedImpl* create() const VRAY_OVERRIDE {
		return new VRaySceneRef();
	}

private:
	VRaySceneFactory();

	VUTILS_DISABLE_COPY(VRaySceneFactory);
};


VRaySceneRef::VRaySceneRef():
	GU_PackedImpl()
{}

VRaySceneRef::VRaySceneRef(const VRaySceneRef &src):
	GU_PackedImpl(src)
{
	update(src.m_options);
}

VRaySceneRef::~VRaySceneRef()
{
	clearDetail();
}

void VRaySceneRef::clearDetail()
{
	m_detail = GU_ConstDetailHandle();
}

bool VRaySceneRef::updateFrom(const UT_Options &options)
{
	if (m_options == options) {
		return false;
	}

	m_options = options;
	m_dirty = true;
	// Notify base primitive that topology has changed
	topologyDirty();

	return true;
}
