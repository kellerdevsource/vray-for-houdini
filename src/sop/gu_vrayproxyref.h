//
// Copyright (c) 2015, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VRAYPROXYREF_H
#define VRAY_FOR_HOUDINI_VRAYPROXYREF_H

#include "vfh_vray.h"
#include <GU/GU_PackedImpl.h>


namespace VRayForHoudini {


class VRayProxyRef:
		public GU_PackedImpl
{
public:
	/// Get the type ID for the VRayProxy primitive type.
	static GA_PrimitiveTypeId typeId() { return theTypeId; }
	static void install(GA_PrimitiveFactory *gafactory);

private:
	static GA_PrimitiveTypeId theTypeId;

public:
	VRayProxyRef();
	VRayProxyRef(const VRayProxyRef &src);
	virtual ~VRayProxyRef();

	/// @{
	/// Virtual interface from GU_PackedImpl interface
	virtual GU_PackedFactory*   getFactory() const VRAY_OVERRIDE;
	virtual GU_PackedImpl*      copy() const VRAY_OVERRIDE;
	virtual void                clearData() VRAY_OVERRIDE;
	virtual bool                isValid() const VRAY_OVERRIDE;

	virtual bool   load(const UT_Options &options, const GA_LoadMap &) VRAY_OVERRIDE
	{ updateFrom(options); return true; }
	virtual void   update(const UT_Options &options) VRAY_OVERRIDE
	{ updateFrom(options); }

//	virtual bool   supportsJSONLoad() const VRAY_OVERRIDE
//	{ return true; }
//	virtual bool   loadFromJSON(const UT_JSONValueMap &options, const GA_LoadMap &) VRAY_OVERRIDE
//	{ updateFrom(options); return true; }

	virtual bool                   save(UT_Options &options, const GA_SaveMap &map) const VRAY_OVERRIDE;
	virtual bool                   getBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	virtual bool                   getRenderingBounds(UT_BoundingBox &box) const VRAY_OVERRIDE;
	virtual void                   getVelocityRange(UT_Vector3 &min, UT_Vector3 &max) const VRAY_OVERRIDE;
	virtual void                   getWidthRange(fpreal &min, fpreal &max) const VRAY_OVERRIDE;
	virtual bool                   unpack(GU_Detail &destgdp) const VRAY_OVERRIDE;
	virtual GU_ConstDetailHandle   getPackedDetail(GU_PackedContext *context = 0) const VRAY_OVERRIDE;
	/// Report memory usage (includes all shared memory)
	virtual int64                  getMemoryUsage(bool inclusive) const VRAY_OVERRIDE;
	/// Count memory usage using a UT_MemoryCounter in order to count
	/// shared memory correctly.
	virtual void                   countMemory(UT_MemoryCounter &counter, bool inclusive) const VRAY_OVERRIDE;
	/// @}

	/// @{
	/// Member data accessors for intrinsics
	const GU_ConstDetailHandle    getDetail() const { return m_detail; }

	UT_StringHolder               getFilepath() const;
	exint                         getLOD() const;
	fpreal64                      getFloatFrame() const;
	exint                         getAnimType() const;
	fpreal64                      getAnimOffset() const;
	fpreal64                      getAnimSpeed() const;
	bool                          getAnimOverride() const;
	exint                         getAnimStart() const;
	exint                         getAnimLength() const;
	/// @}

private:
	/// updateFrom() will update from UT_Options only
	bool   updateFrom(const UT_Options &options);

private:
	GU_DetailHandle        m_detail;
	UT_Options             m_options;
};


} // namespace VRayForHoudini

#endif // VRAY_FOR_HOUDINI_VRAYPROXYREF_H
