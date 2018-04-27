//
// Copyright (c) 2015-2018, Chaos Software Ltd
//
// V-Ray For Houdini
//
// ACCESSIBLE SOURCE CODE WITHOUT DISTRIBUTION OF MODIFICATION LICENSE
//
// Full license text: https://github.com/ChaosGroup/vray-for-houdini/blob/master/LICENSE
//

#ifndef VRAY_FOR_HOUDINI_VFH_GU_CACHE_H
#define VRAY_FOR_HOUDINI_VFH_GU_CACHE_H

#include "hash_map.h"

#include <GU/GU_PackedGeometry.h>
#include <FS/UT_DSO.h>

template <typename key, typename ReturnValue>
class DetailBuilder {
public:
	virtual ~DetailBuilder() = default;

	/// Method to be used to obtain the GU_DetailHandle to be cached
	/// @param filepath File path.
	/// @param settings Specific settings that the cached detail adheres to.
	/// @param t Frame at which the detail is required.
	/// @param[out] rval Return value, for any data that is created during the creation of the detail and is required after the fact.
	/// @returns detail to be cached, if cache is enabled, and returned.
	virtual GU_DetailHandle buildDetail(const VUtils::CharString &filepath, const key &settings, fpreal t, ReturnValue &rval) = 0;

	/// Method called when there are no more references to the given filepath
	/// @param filepath File path.
	virtual void cleanResource(const VUtils::CharString &filepath) = 0;
};

template <typename ReturnValue, typename T, typename KeyHash = VUtils::DefaultHash<T>, typename KeysEqual = VUtils::DefaultKeyCompare<T>>
class DetailCachePrototype
{
public:
	typedef DetailBuilder<T, ReturnValue> DetailBuilderType;

	/// Cache constructor
	/// @param builder Builder object, to be used to create the details.
	explicit DetailCachePrototype(DetailBuilderType &builder)
		: detailBuilder(builder)
	{}
	
	DetailCachePrototype() = delete;
	~DetailCachePrototype() = default;

	/// Sets cache enabled state.
	/// @param state Cache state.
	void enableCache(bool state) {
		isOn = state;
	}

	/// Method for registering a filepath and settings pair, increments the reference
	/// counter for the given filepath and settings pair
	/// @param filepath File path.
	/// @param settings Specific settings that the cached detail will adheres to.
	void registerInCache(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty())
			return;

		detailCache[filepath.ptr()][settings].references += 1;
	}

	/// Method for unregistering a filepath and settings pair, decrements the reference
	/// counter. Calls the cleanResource method when counter reaches 0
	/// @param filepath File path.
	/// @param settings Specific settings that the cached detail adheres to.
	void unregister(const VUtils::CharString &filepath, const T &settings) {
		if (filepath.empty())
			return;

		CacheElementMap &tempVal = detailCache[filepath.ptr()];
		if (--tempVal[settings].references < 1) {
			tempVal.remove(settings);
			if (tempVal.isEmpty()) {
				detailCache.remove(filepath.ptr());
				detailBuilder.cleanResource(filepath.ptr());
			}
		}
	}
	
	/// Retrieve the detail for the given filepath and settings pair at the given frame, 
	/// if the cache is disabled or the detail is not cached, creates the detail using the builder
	/// @param filepath File path.
	/// @param settings Specific settings that the cached detail adheres to.
	/// @param frame Frame for the corresponding detail.
	/// @returns Requested detail, empty if filepath is empty or wrong.
	GU_DetailHandle getDetail(const VUtils::CharString &filepath, const T &settings, fpreal frame) {
		ReturnValue temp;
		return getDetail(filepath, settings, frame, temp);
	}

	/// Retrieve the detail for the given filepath and settings pair at the given frame,
	/// if the cache is disabled or the detail is not cached, creates the detail using the builder
	/// @param filepath File path.
	/// @param settings Specific settings that the cached detail adheres to.
	/// @param frame Frame for the corresponding detail.
	/// @param[out] rval Return value for any data that is created during the creation of the detail and is required after the fact.
	/// @returns Requested detail, empty if filepath is empty or wrong.
	GU_DetailHandle getDetail(const VUtils::CharString &filepath, const T &settings, fpreal frame, ReturnValue &rval) {
		GU_DetailHandle gdpHndl;

		if (filepath.empty())
			return gdpHndl;

		if (!isOn) {
			gdpHndl = detailBuilder.buildDetail(filepath, settings, frame, rval);
		}
		else {
			gdpHndl = getCachedCached(filepath, settings, frame);
			if (!gdpHndl.isValid()) {
				gdpHndl = detailBuilder.buildDetail(filepath, settings, frame, rval);

				detailCache[filepath.ptr()][settings].frameDetailMap.insert(getFrameKey(frame), gdpHndl);
			}
		}

		return gdpHndl;
	}

	/// Return GU_DetailHandle from cache.
	/// @param filepath File path.
	/// @param settings Specific settings that the cached detail adheres to.
	/// @param frame Frame for the corresponding detail.
	/// @returns GU_DetailHandle. May be invalid if cache entry doesn't exists.
	GU_DetailHandle getCachedCached(const VUtils::CharString &filepath, const T &settings, fpreal frame) const {
		if (filepath.empty())
			return GU_DetailHandle();

		const typename DetailCacheMap::const_iterator fIt = detailCache.find(filepath.ptr());
		if (fIt == detailCache.end())
			return GU_DetailHandle();

		const CacheElementMap &cacheElements = fIt.value();
		const typename CacheElementMap::const_iterator ceIt = cacheElements.find(settings);
		if (ceIt == cacheElements.end())
			return GU_DetailHandle();

		const CacheElement &cacheElement = ceIt.value();

		const typename CacheElement::DetailMap &detailMap = cacheElement.frameDetailMap;
		const typename CacheElement::DetailMap::const_iterator dIt = detailMap.find(getFrameKey(frame));
		if (dIt == detailMap.end())
			return GU_DetailHandle();

		return dIt.value();
	}

	/// Method for checking if sepcific filepath and settings pair is cached for a given frame
	/// @param filepath File path.
	/// @param settings Specific settings that the cached detail adheres to.
	/// @param frame Frame for the corresponding detail.
	/// @returns true if the detail is cached and valid
	bool isCached(const VUtils::CharString &filepath, const T &settings, fpreal frame) const {
		const GU_DetailHandle &detail = getCachedCached(filepath, settings, frame);
		return detail.isValid();
	}

	/// Method for checking if there are any cached details for a given filepath
	/// @param filepath File path.
	/// @returns true if there are any cached details with given filepath
	bool isCached(const VUtils::CharString &filepath) const {
		if (filepath.empty())
			return false;

		return detailCache.find(filepath.ptr()) != detailCache.end();
	}

private:
	/// Frame key type.
	typedef int FrameKey;

	/// Returns cache key for the float frame.
	/// @param frame Float frame number.
	static FrameKey getFrameKey(fpreal frame) {
		return VUtils::fast_ceil(frame * 1000.0);
	}

	/// Reference counted cache element. Contains frame / detail map.
	struct CacheElement {
		typedef QMap<FrameKey, GU_DetailHandle> DetailMap;

		bool operator == (const CacheElement &other) const {
			return references == other.references;
		}

		/// Reference counter.
		int references = 0;

		/// Frame to detail map.
		DetailMap frameDetailMap;
	};

	typedef QMap<T, CacheElement> CacheElementMap;
	typedef QMap<QString, CacheElementMap> DetailCacheMap;

	/// Cache container.
	DetailCacheMap detailCache;

	/// A class for building new detail if cache entry is not found.
	DetailBuilderType &detailBuilder;

	/// Cache enabled state.
	bool isOn = true;

	VUTILS_DISABLE_COPY(DetailCachePrototype)
};

#endif // VRAY_FOR_HOUDINI_VFH_GU_CACHE_H
