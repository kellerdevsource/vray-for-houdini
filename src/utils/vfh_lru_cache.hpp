#include "vfh_defines.h"

#include <vector>
#include <list>
#include <unordered_map>
#include <utility>
#include <memory>
#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <vassert.h>

namespace VRayForHoudini {
namespace Caches {

////-------------------------------------------------------------
//// LRU Cache
////-------------------------------------------------------------

template < class Key,
		   class T,
		   class Hash = std::hash<Key>,
		   class Pred = std::equal_to<Key>,
		   int defaultCapacity = 127 >
class LRUCache
{
public:
	typedef Key    key_type;
	typedef T      value_type;
	typedef Hash   hasher;
	typedef Pred   key_equal;

	typedef boost::function< void (const key_type &, value_type &) > CbFetch;
	typedef boost::function< void (const key_type &, value_type &) > CbEvict;

private:
	typedef std::list< key_type > MLRUQueue;
	typedef std::pair< value_type, typename MLRUQueue::iterator > CachedData;
	typedef std::unordered_map< key_type, CachedData, hasher, key_equal > CacheMap;

	struct Iter :
			std::iterator< std::bidirectional_iterator_tag, value_type >
	{
		Iter() : m_keyIt(), m_cm(nullptr) { }
		explicit Iter(const typename MLRUQueue::const_iterator& it, CacheMap * cm) : m_keyIt(it), m_cm(cm) { }
//		using default copy constructor & copy assignment

		const key_type &key() const
		{ vassert( m_cm ); return (*m_keyIt); }

		typename Iter::pointer value()
		{ vassert( m_cm ); return &(*m_cm)[ (*m_keyIt) ].first; }

		typename Iter::reference operator*() const
		{ vassert( m_cm ); return (*m_cm)[ (*m_keyIt) ].first; }

		typename Iter::pointer operator->() const
		{ vassert( m_cm ); return &(*m_cm)[ (*m_keyIt) ].first; }

		Iter &operator++()
		{ m_keyIt++; return *this; }

		Iter operator++(int)
		{ Iter tmp = *this; ++*this; return tmp; }

		Iter &operator--()
		{ m_keyIt--; return *this; }

		Iter operator--(int)
		{ Iter tmp = *this; --*this; return tmp; }

		bool operator== (const Iter& other) const
		{ return (m_cm == other.m_cm && m_keyIt == other.m_keyIt); }

		bool operator!= (const Iter& other) const
		{ return (m_cm != other.m_cm || m_keyIt != other.m_keyIt); }

	private:
		typename MLRUQueue::const_iterator m_keyIt;
		CacheMap *m_cm;
	};

public:
	typedef typename CacheMap::size_type size_type;
	typedef Iter iterator;

public:
	LRUCache():
		m_capacity(defaultCapacity),
		m_cacheMap(defaultCapacity),
		m_mlruQueue(0)
	{ vassert( defaultCapacity > 0); }

	LRUCache(size_type capacity):
		m_capacity(capacity),
		m_cacheMap(capacity),
		m_mlruQueue(0)
	{ vassert( capacity > 0); }

	~LRUCache()
	{ clear(); }

	size_type capacity() const { return m_capacity; }
	size_type size() const { return m_cacheMap.size(); }
	int empty() const { return (size() == 0); }

	void setFetchCallback(const CbFetch& cb) { m_cbfetchValue = cb; }
	void setEvictCallback(const CbEvict& cb) { m_cbEvictValue = cb; }

	iterator begin() { return iterator(m_mlruQueue.begin(), &m_cacheMap); }
	iterator end() { return iterator(m_mlruQueue.end(), &m_cacheMap); }

	void setCapacity(const size_type &capacity)
	{
		vassert( capacity > 0);
		m_capacity = capacity;
		while (size() > m_capacity ) {
			evict();
		}
	}

/// @brief Checks if there is a cached value under key
/// @param key - key used to search in cache
/// @return true - if there exists a cached value with the given key
///         false - otherwise
	int contains(const key_type &key)
	{
		typename CacheMap::const_iterator it = m_cacheMap.find(key);
		return (it != m_cacheMap.end());
	}

/// @brief Attempts to insert value into cache under the given key
/// @param key - key used to later search in cache
/// @param value - value to be cached
/// @return true if value was inserted successfully
///         false otherwise
	int insert(const key_type &key, const value_type& value)
	{
		typename CacheMap::const_iterator it = m_cacheMap.find(key);
//		key has already been cached  => exit
		if (it != m_cacheMap.end()) {
			return false;
		}

//		when cache is full evict item
		if (m_mlruQueue.size() >= m_capacity) {
			evict();
		}

//		cache item under key and make key MRU(MRU at the front of the queue)
//		if key not in m_cacheMap => key not in m_mlruQueue
		typename MLRUQueue::iterator mruIt = m_mlruQueue.emplace(m_mlruQueue.begin(), key);
		m_cacheMap.emplace(key, std::make_pair(value, mruIt));

		return true;
	}

/// @brief Attempts to find value stored under key in cache
///        if no key is found in cache inserts a default value and
///        fetches data init if fetchValue callback is set
/// @param key - the key used to search in cache
/// @return reference to the value stored in cache
	value_type &operator[](const key_type &key)
	{
		typename CacheMap::iterator it = m_cacheMap.find(key);
//		key is in cache => return stored value
		if (it != m_cacheMap.end()) {
//			make key MRU (MRU key is at the front of the queue)
			CachedData &item = it->second;
			m_mlruQueue.splice(m_mlruQueue.begin(), m_mlruQueue, item.second);
			return item.first;
		}

//		create new element in cache and try fetch
//		when cache is full evict item
		if (m_mlruQueue.size() >= m_capacity) {
			evict();
		}

		typename MLRUQueue::iterator mruIt = m_mlruQueue.emplace(m_mlruQueue.begin(), key);
		CachedData &item = m_cacheMap[key];
		item.second = mruIt;
		if (m_cbfetchValue) {
			m_cbfetchValue(key, item.first);
		}

		return item.first;
	}

/// @brief Attempts to erase the value stored under key from cache
/// @param key - key used to search in cache
/// @return true if entry was erased successfully
///         false otherwise
	int erase(const key_type &key)
	{
		typename CacheMap::const_iterator it = m_cacheMap.find(key);
//		key is not in cache => exit
		if (it == m_cacheMap.end()) {
			return false;
		}

//		remove key from m_mlruQueue and item from cache
		const CachedData &item = it->second;
		m_mlruQueue.erase(item.second);
		m_cacheMap.erase(it);

		return true;
	}

/// @brief Attempts to find the value stored under key in cache
/// @param key - key used to search in cache
/// @return An iterator pointing to value in cache
///         or iterator end() if no such key exists in cache
	iterator find(const key_type &key)
	{
		typename CacheMap::iterator it = m_cacheMap.find(key);
//		key is not in cache => exit
		if (it == m_cacheMap.end()) {
			return end();
		}

//		make key MRU (MRU key is at the front of the queue)
		CachedData &item = it->second;
		m_mlruQueue.splice(m_mlruQueue.begin(), m_mlruQueue, item.second);

		return iterator(item.second, &m_cacheMap);
	}

/// @brief Attempts to update the value stored under key in cache
/// @param key - key used to search in cache
/// @param value - the new value to store in cache
/// @return An iterator pointing to the new value in cache
///         or iterator end() if no such key exists in cache
	iterator update(const key_type &key,  const value_type& value)
	{
		typename CacheMap::iterator it = m_cacheMap.find(key);
//		key is not in cache => exit
		if (it == m_cacheMap.end()) {
			return end();
		}

//		make key MRU (MRU key is at the front of the queue)
		CachedData &item = it->second;
		item.first = value;
		m_mlruQueue.splice(m_mlruQueue.begin(), m_mlruQueue, item.second);

		return iterator(item.second, &m_cacheMap);
	}

/// @brief Evicts least recently used(LRU) element from cache
	void evict()
	{
		vassert( NOT(m_mlruQueue.empty()) );

//		find LRU item
//		MRU item is at the front of the queue
//		LRU item is at the back of the queue
		const key_type& key = m_mlruQueue.back();
		typename CacheMap::iterator it = m_cacheMap.find(key);
		vassert( it != m_cacheMap.end() );

		if (m_cbEvictValue){
			CachedData &item = it->second;
			m_cbEvictValue(key, item.first);
		}

//		erase item from cache
		m_cacheMap.erase(it);
		m_mlruQueue.pop_back();
	}

/// @brief Clears all data from cache
	void clear()
	{
		m_mlruQueue.clear();
		m_cacheMap.clear();
	}

private:
///	avoid copying
	LRUCache(const LRUCache& other);
	LRUCache &operator =(const LRUCache& other);

private:
	size_type m_capacity;
	CacheMap m_cacheMap;
	MLRUQueue m_mlruQueue;
	CbFetch m_cbfetchValue;
	CbEvict m_cbEvictValue;
};


template < class Key,
		   class Sequence,
		   class ItemHash,
		   int defaultCapacity = 127 >
class CacheList
{
public:
	typedef Key key_type;
	typedef Sequence value_type;
	typedef typename std::shared_ptr< value_type > value_ptr;

private:
	typedef typename ItemHash::result_type ItemKey;
	typedef typename Sequence::value_type Item;
	struct CachedData
	{
		key_type m_key;
		std::vector<ItemKey> m_itemKeys;
	};
	struct ItemData
	{
		Item m_item;
		unsigned m_refCnt;
	};

	typedef LRUCache< key_type, CachedData, std::hash<key_type>, std::equal_to<key_type> >SequenceCache;
	typedef LRUCache< ItemKey, ItemData, std::hash<ItemKey>, std::equal_to<ItemKey> >ItemCache;
	typedef CacheList< key_type, value_type, ItemHash, defaultCapacity > Self;

public:
	typedef typename SequenceCache::size_type size_type;

public:
	CacheList():
		m_capacity(defaultCapacity),
		m_sequenceCache(defaultCapacity),
		m_itemCache(defaultCapacity)
	{
		vassert( defaultCapacity > 0);
		m_sequenceCache.setEvictCallback(typename SequenceCache::CbEvict(boost::bind(&Self::evictSequence, this, _1, _2)));
	}

	CacheList(size_type capacity):
		m_capacity(capacity),
		m_sequenceCache(capacity),
		m_itemCache(capacity)
	{
		vassert( capacity > 0);
		m_sequenceCache.setEvictCallback(typename SequenceCache::CbEvict(boost::bind(&Self::evictSequence, this, _1, _2)));
	}

	~CacheList()
	{ clear(); }

	size_type capacity() const { return m_capacity; }
	size_type size() const { return m_sequenceCache.size(); }
	int empty() const { return (size() == 0); }

	void setCapacity(size_type capacity)
	{
		vassert( capacity > 0);
		m_capacity = capacity;
		m_itemCache.setCapacity(capacity);
		m_sequenceCache.setCapacity(capacity);
	}

/// @brief Checks if there is an element stored under key in cache
///        NOTE: if an item from the sequence is missing
///              it removes the sequence itself and returns false
///	@return true - sequence is in cache = key exists and all sequence items are cached
///         false - otherwise
	int contains(const key_type &key)
	{
		if (NOT(m_sequenceCache.contains(key))) {
			return false;
		}

//		if in cache check if all items from the collection are cached
		int inCache = true;
		CachedData& seqData = m_sequenceCache[key];
		for (const auto &itemKey : seqData.m_itemKeys) {
			if (NOT(m_itemCache.contains(itemKey))) {
				erase(key);
				inCache = false;
				break;
			}
		}
		return inCache;
	}

/// @brief Attempts to insert sequence into cache under the given key
///        each item from the sequence is cached induvidually
///        i.e. if same element is present in 2 different sequences it won't be stored twice
/// @param key - key used to later search in cache
/// @param sequence - value to be cached
/// @return true if value was inserted successfully
///         false otherwise
	int insert(const key_type &key, const value_type &sequence)
	{
		if (contains(key)) {
			return false;
		}

//		insert new item in sequenceCache
		CachedData &seqData = m_sequenceCache[key];
		seqData.m_key = key;
		seqData.m_itemKeys.resize(sequence.size());

//		cache each item individually
		ItemHash hasher;
		for (int i = 0; i < sequence.size(); ++i) {
			const Item &item = sequence[i];
			ItemKey itemKey = hasher( item );
			seqData.m_itemKeys[i] = itemKey;

			if (m_itemCache.contains(itemKey)) {
//				in itemCache only increase ref count
				ItemData &itemData = m_itemCache[itemKey];
				++itemData.m_refCnt;
			} else {
//				not in itemCache insert as new item and init ref count to 1
				ItemData &itemData = m_itemCache[itemKey];
				itemData.m_item = item;
				itemData.m_refCnt = 1;
			}
		}

		return true;
	}

/// @brief Attempts to erase the sequence stored under key from cache
/// @param key - key used to search in cache
/// @return true if entry was erased successfully
///         false otherwise
	int erase(const key_type &key)
	{
		if (NOT(m_sequenceCache.contains(key))) {
			return false;
		}

		CachedData &seqData = m_sequenceCache[key];
		evictSequence(key, seqData);

		return m_sequenceCache.erase(key);
	}

/// @brief Attempts to find the sequnce stored under key in cache
/// @param key - key used to search in cache
/// @return a shared pointer to a new sequence that contains all
///         items from the cached one or empty pointer if key is not found in cache
	value_ptr find(const key_type &key)
	{
		if (NOT(contains(key))) {
			return value_ptr();
		}

		CachedData& seqData = m_sequenceCache[key];

		value_ptr res = std::make_shared<Sequence>(seqData.m_itemKeys.size());
		int i = 0;
		for (auto const &itemKey : seqData.m_itemKeys) {
			typename ItemCache::iterator itemIt = m_itemCache.find(itemKey);
			vassert( itemIt != m_itemCache.end() );

			ItemData &itemData = *itemIt;
			(*res)[i++] = itemData.m_item;
		}

		return res;
	}

/// @brief Attempts to update the sequnce stored under key in cache
/// @param key - key used to search in cache
/// @param sequence - the new value to store in cache
/// @return true if updated successfully
///         false otherwise
	int update(const key_type &key, const value_type &sequence)
	{
		if (NOT(m_sequenceCache.contains(key))) {
			return false;
		}

		CachedData &seqData = m_sequenceCache[key];
		evictSequence(key, seqData);

		seqData.m_key = key;
		seqData.m_itemKeys.resize(sequence.size());

//		cache each item individually
		ItemHash hasher;
		for (int i = 0; i < sequence.size(); ++i) {
			const Item &item = sequence[i];
			ItemKey itemKey = hasher( item );
			seqData.m_itemKeys[i] = itemKey;

			if (m_itemCache.contains(itemKey)) {
//				in itemCache only increase ref count
				ItemData &itemData = m_itemCache[itemKey];
				++itemData.m_refCnt;
			} else {
//				not in itemCache insert as new item and init ref count to 1
				ItemData &itemData = m_itemCache[itemKey];
				itemData.m_item = item;
				itemData.m_refCnt = 1;
			}

		}

		return true;
	}

/// @brief Clears all data from cache
	void clear()
	{
		m_sequenceCache.clear();
		m_itemCache.clear();
	}

private:
	CacheList(const Self &other);
	Self & operator =(const Self &other);

/// @brief When evicting an element from the sequenceCache
///        we need to remove all items in the itemCache for that sequence
///        that are not part of another sequence
/// @param key - key under which the sequeceis stored in cache
/// @param seqData - stores keys corresponding to actual items in itemCache
	void evictSequence(const key_type &key, CachedData &seqData)
	{
		for (const auto &itemKey : seqData.m_itemKeys) {
			if (m_itemCache.contains(itemKey)) {
				ItemData& itemData = m_itemCache[itemKey];
				--itemData.m_refCnt;
				if (itemData.m_refCnt <= 0) {
					m_itemCache.erase(itemKey);
				}
			}
		}
	}


private:
	size_type m_capacity;
	SequenceCache m_sequenceCache;
	ItemCache m_itemCache;
};

}  // namespace Caches
}  // namespace VRayForHoudini

