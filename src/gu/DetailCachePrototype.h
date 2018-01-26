//
// Copyright (c) 2015-2017, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_DETAILCACHE

#define VRAY_FOR_HOUDINI_DETAILCACHE

#include "vfh_defines.h"
#include "vfh_log.h"
#include "vfh_includes.h"
#include "hash_map.h"

#include <GU/GU_PackedFactory.h>
#include <GU/GU_PrimPacked.h>
#include <GU/GU_PrimPoly.h>
#include <GU/GU_PackedContext.h>
#include <GU/GU_PackedGeometry.h>
#include <FS/UT_DSO.h>

template <typename BD, typename T, typename KeyHash = VUtils::DefaultHash<T>, typename KeysEqual = VUtils::DefaultKeyCompare<T>>
class DetailCachePrototype
{
public:
	DetailCachePrototype()
	{}
	
	~DetailCachePrototype()
	{}

	void registerInCache(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty()) {
			return;
		}
		Log::getLog().debug("Registering: %s", filepath.ptr());
		vrsceneCache[filepath.ptr()][settings].references++;
	}

	void unregister(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty()) {
			return;
		}

		CacheElementMap &tempVal = vrsceneCache[filepath.ptr()];
		if (--tempVal[settings].references < 1) {
			Log::getLog().debug("Erasing: %s", filepath.ptr());
			tempVal.erase(settings);
			if (tempVal.empty())
				vrsceneCache.erase(filepath.ptr());
		}
	}

	// have GU_DetailHandle be a GU_Detail* here?
	void setDetail(const VUtils::CharString &filepath, const T &settings, const fpreal &frame, BD *baseData, const GU_DetailHandle &detail) {
		if (filepath.empty()) {
			return;
		}
		CacheElement &temp = vrsceneCache[filepath.ptr()][settings];
		int key = 1000 * frame;
		Data &data = temp.frameDetailMap[key];

		data.baseData = baseData;
		data.detail = detail;
	}

	GU_DetailHandle& getDetail(const VUtils::CharString &filepath, const T &settings, const fpreal &frame) {
		vassert(!filepath.empty());
		int key = frame * 1000;
		Log::getLog().debug("Returning detail for: %s", filepath.ptr());
		return vrsceneCache[filepath.ptr()][settings].frameDetailMap[key].detail;
	}

	bool isCached(const VUtils::CharString &filepath, const T &settings, const fpreal &frame, const BD *baseData) {
		if (filepath.empty()) {
			return false;
		}

		CacheElement &temp = vrsceneCache[filepath.ptr()][settings];
		int key = frame * 1000;
		Data &data = temp.frameDetailMap[key];

		if (baseData == data.baseData) {
			return data.detail.isValid();
		}

		// clear cache entries associated with this
		data.detail.clear();// could this be an issue due to possible concurent access?
		data.baseData = nullptr;

		return false;
	}

	bool isCached(const VUtils::CharString &filepath) {
		if (filepath.empty()) {
			return false;
		}

		return vrsceneCache.find(filepath.ptr()) != vrsceneCache.end();
	}
private:
	struct Data {
		Data()
			: baseData(nullptr)
		{}

		BD* baseData;
		GU_DetailHandle detail;
	};

	struct CacheElement {
		CacheElement()
			: references(0)
		{}

		bool operator == (const CacheElement &other) {
			return references == other.references;
		}

		int references;
		VUtils::HashMap<int, Data> frameDetailMap;
	};

	typedef VUtils::HashMap<T, CacheElement, KeyHash> CacheElementMap;
	VUtils::StringHashMap<CacheElementMap> vrsceneCache;

	VUTILS_DISABLE_COPY(DetailCachePrototype)
};

#endif