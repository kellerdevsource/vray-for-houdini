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

template <typename key>
class DetailBuilder {
public:
	virtual GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const key &settings, const fpreal &t, UT_BoundingBox &box) = 0;
};

template <typename BD, typename T, typename KeyHash = VUtils::DefaultHash<T>, typename KeysEqual = VUtils::DefaultKeyCompare<T>>
class DetailCachePrototype
{
public:
	DetailCachePrototype(DetailBuilder<T> &builder)
		: detailBuilder(&builder)
		, isOn(true)
	{}
	
	DetailCachePrototype() = delete;

	~DetailCachePrototype()
	{}

	void enableCache(bool state) {
		isOn = state;
	}

	void registerInCache(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty()) {
			return;
		}
		Log::getLog().debug("Incrementing: %s", filepath.ptr());
		detailCache[filepath.ptr()][settings].references++;
	}

	void unregister(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty()) {
			return;
		}

		CacheElementMap &tempVal = detailCache[filepath.ptr()];
		int temp = tempVal[settings].references;
		if (--tempVal[settings].references < 1) {
			tempVal.erase(settings);
			Log::getLog().debug("erasing: %s", filepath.ptr());
			if (tempVal.empty())
				detailCache.erase(filepath.ptr());
		}
	}

	GU_DetailHandle& getDetail(const VUtils::CharString &filepath, const T &settings, const fpreal &frame, UT_BoundingBox &box = UT_BoundingBox()) {
		if (filepath.empty()) {
			return GU_DetailHandle();
		}

		if (isOn) {
			if (isCached(filepath, settings, frame)) {
				return detailCache[filepath.ptr()][settings].frameDetailMap[getFrameKey(frame)].detail;
			}
			else {
				return detailCache[filepath.ptr()][settings].frameDetailMap[getFrameKey(frame)].detail = detailBuilder->buildDetail(filepath, settings, frame, box);
			}
		}

		return detailBuilder->buildDetail(filepath, settings, frame, box);
	}

	bool isCached(const VUtils::CharString &filepath, const T &settings, const fpreal &frame) {
		if (filepath.empty()) {
			return false;
		}

		CacheElement &temp = detailCache[filepath.ptr()][settings];
		Data &data = temp.frameDetailMap[getFrameKey(frame)];

		// --- check if it's here
		if (data.detail.isValid()) {
			return true;
		}

		// clear cache entries associated with this
		data.detail.clear();// could this be an issue due to possible concurent access?

		return false;
	}

	bool isCached(const VUtils::CharString &filepath) {
		if (filepath.empty()) {
			return false;
		}

		return detailCache.find(filepath.ptr()) != detailCache.end();
	}
private:

	int getFrameKey(const fpreal &frame) {
		return frame * 1000;
	}

	struct Data {
		Data()
		{}

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
	VUtils::StringHashMap<CacheElementMap> detailCache;

	DetailBuilder<T> *detailBuilder;
	bool isOn;

	VUTILS_DISABLE_COPY(DetailCachePrototype)
};

#endif