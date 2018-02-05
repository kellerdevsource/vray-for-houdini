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
	/// Method to be used to obtain the GU_DetailHandle to be cached
	/// @param filepath[in] File path.
	/// @param settings[in] Specific settings that the cached detail adheres to.
	/// @param t[in] Frame at which the detail is required.
	/// @param rval[out] Return value, for any data that is created during the creation of the detail and is required after the fact.
	/// @retval detail to be cached, if cache is enabled, and returned.
	virtual GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const key &settings, fpreal t, ReturnValue &rval) = 0;

	/// Method called when there are no more references to the given filepath
	/// @param filepath[in] File path.
	virtual void cleanResource(const VUtils::CharString &filepath) = 0;
};

template <typename ReturnValue, typename T, typename KeyHash = VUtils::DefaultHash<T>, typename KeysEqual = VUtils::DefaultKeyCompare<T>>
class DetailCachePrototype
{
public:
	/// Cache constructor
	/// @param builder[in] Builder object, to be used to create the details.
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

	/// Method for registering a filepath and settings pair, increments the reference
	/// counter for the given filepath and settings pair
	/// @param filepath[in] File path.
	/// @param settings[in] Specific settings that the cached detail will adheres to.
	void registerInCache(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty()) {
			return;
		}
		detailCache[filepath.ptr()][settings].references++;
	}

	/// Method for unregistering a filepath and settings pair, decrements the reference
	/// counter. Calls the cleanResource method when counter reaches 0
	/// @param filepath[in] File path.
	/// @param settings[in] Specific settings that the cached detail adheres to.
	void unregister(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty()) {
			return;
		}

		CacheElementMap &tempVal = detailCache[filepath.ptr()];
		if (--(tempVal[settings].references) < 1) {
			tempVal.erase(settings);
			if (tempVal.empty()) {
				detailCache.erase(filepath.ptr());
				detailBuilder->cleanResource(filepath.ptr());
			}
		}
	}
	
	/// Retrieve the detail for the given filepath and settings pair at the given frame, 
	/// if the cache is disabled or the detail is not cached, creates the detail using the builder
	/// @param filepath[in] File path.
	/// @param settings[in] Specific settings that the cached detail adheres to.
	/// @param frame[in] Frame for the corresponding detail.
	/// @retval Requested detail, empty if filepath is empty or wrong.
	GU_DetailHandle& getDetail(const VUtils::CharString &filepath, const T &settings, fpreal frame) {
		ReturnValue temp;
		return getDetail(filepath, settings, frame, temp);
	}

	/// Retrieve the detail for the given filepath and settings pair at the given frame,
	/// if the cache is disabled or the detail is not cached, creates the detail using the builder
	/// @param filepath[in] File path.
	/// @param settings[in] Specific settings that the cached detail adheres to.
	/// @param frame[in] Frame for the corresponding detail.
	/// @param rval[out] Return value, for any data that is created during the creation of the detail and is required after the fact.
	/// @retval Requested detail, empty if filepath is empty or wrong.
	GU_DetailHandle& getDetail(const VUtils::CharString &filepath, const T &settings, fpreal frame, ReturnValue &rval) {
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

	/// Method for checking if sepcific filepath and settings pair is cached for a given frame
	/// @param filepath[in] File path.
	/// @param settings[in] Specific settings that the cached detail adheres to.
	/// @param frame[in] Frame for the corresponding detail.
	/// @retval true if the detail is cached and valid
	bool isCached(const VUtils::CharString &filepath, const T &settings, fpreal frame) {
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

	/// Method for checking if there are any cached details for a given filepath
	/// @param filepath[in] File path.
	/// @retval true if there are any cached details with given filepath
	bool isCached(const VUtils::CharString &filepath) {
		if (filepath.empty()) {
			return false;
		}

		return detailCache.find(filepath.ptr()) != detailCache.end();
	}
private:

	int getFrameKey(fpreal frame) {
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