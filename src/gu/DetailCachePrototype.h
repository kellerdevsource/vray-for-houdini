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

template <typename key, typename ReturnValue>
class DetailBuilder {
public:
	virtual GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const key &settings, const fpreal &t, ReturnValue &rval) = 0;
};

template <typename ReturnValue, typename T, typename KeyHash = VUtils::DefaultHash<T>, typename KeysEqual = VUtils::DefaultKeyCompare<T>>
class DetailCachePrototype
{
public:
	DetailCachePrototype(DetailBuilder<T, ReturnValue> &builder)
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
		detailCache[filepath.ptr()][settings].references++;
	}

	void unregister(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty()) {
			return;
		}

		CacheElementMap &tempVal = detailCache[filepath.ptr()];
		if (--(tempVal[settings].references) < 1) {
			tempVal.erase(settings);
			if (tempVal.empty())
				detailCache.erase(filepath.ptr());
		}
	}

	GU_DetailHandle& getDetail(const VUtils::CharString &filepath, const T &settings, const fpreal &frame) {
		ReturnValue temp;
		return getDetail(filepath, settings, frame, temp);
	}

	GU_DetailHandle& getDetail(const VUtils::CharString &filepath, const T &settings, const fpreal &frame, ReturnValue &rval) {
		if (filepath.empty()) {
			return GU_DetailHandle();
		}

		if (isOn) {
			if (isCached(filepath, settings, frame)) {
				return detailCache[filepath.ptr()][settings].frameDetailMap[getFrameKey(frame)];
			}
			else {
				return detailCache[filepath.ptr()][settings].frameDetailMap[getFrameKey(frame)] = detailBuilder->buildDetail(filepath, settings, frame, rval);
			}
		}

		return detailBuilder->buildDetail(filepath, settings, frame, rval);
	}

	bool isCached(const VUtils::CharString &filepath, const T &settings, const fpreal &frame) {
		if (filepath.empty()) {
			return false;
		}

		CacheElement &temp = detailCache[filepath.ptr()][settings];
		GU_DetailHandle &detail = temp.frameDetailMap[getFrameKey(frame)];

		// --- check if it's here
		if (detail.isValid()) {
			return true;
		}
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

	struct CacheElement {
		CacheElement()
			: references(0)
		{}

		bool operator == (const CacheElement &other) {
			return references == other.references;
		}

		int references;
		VUtils::HashMap<int, GU_DetailHandle> frameDetailMap;
	};

	typedef VUtils::HashMap<T, CacheElement, KeyHash> CacheElementMap;
	VUtils::StringHashMap<CacheElementMap> detailCache;

	DetailBuilder<T, ReturnValue> *detailBuilder;
	bool isOn;

	VUTILS_DISABLE_COPY(DetailCachePrototype)
};

#endif