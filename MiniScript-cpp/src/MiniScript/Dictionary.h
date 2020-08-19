/*
 *  Dictionary.h
 *
 *  Created by Ryan on 3/15/11.
 *
 */

#ifndef HASHMAP_H
#define HASHMAP_H

#include "List.h"

namespace MiniScript {

	// This is the number of hash cells in the table
	// this will work best as a prime number
	// samples: 101, 521, 1009
	#define TABLE_SIZE 251

	template <class K, class V, unsigned int HASH(const K&)> class Dictionary;
	
	template <class K, class V>
	class HashMapEntry
	{
	public:
		HashMapEntry() { next = nullptr; }
		~HashMapEntry() { if (next) delete next; }
		
		HashMapEntry *Clone() {
			HashMapEntry *result = new HashMapEntry();
			if (next) result->next = next->Clone();
			return result;
		}
		
		HashMapEntry<K, V> *next;
		K key;
		V value;
	};

	template <class K, class V>
	class DictionaryStorage : public RefCountedStorage {
	private:
		DictionaryStorage() : RefCountedStorage(), mSize(0), assignOverride(NULL) { for (int i=0; i<TABLE_SIZE; i++) mTable[i] = nullptr; }
		~DictionaryStorage() { RemoveAll(); }

		void RemoveAll() {
			for (int i = 0; i < TABLE_SIZE; i++) {
				if (mTable[i]) {
					delete mTable[i];
					mTable[i] = nullptr;
				}
			}
			mSize = 0;
		}
		
		long mSize;
		HashMapEntry<K, V> *mTable[TABLE_SIZE];

		void *assignOverride;
		
		template <class K2, class V2, unsigned int HASH(const K2&)> friend class Dictionary;
		template <class K2, class V2> friend class DictIterator;
		friend class Value;
	};
	
	template <class K, class V>
	class DictIterator {
	public:
		bool Done() const { return entry == nullptr; }
		K Key() const { return entry->key;}
		V Value() const { return entry->value; }
		void Next();
		
		bool operator==(const DictIterator<K, V>& other) {
			return storage == other.storage and binIndex == other.binIndex and entry == other.entry;
		}
		
		bool operator!=(const DictIterator<K, V>& other) {
			return not (*this == other);
		}
		
	private:
		DictIterator(DictionaryStorage<K, V> *storage);
		DictionaryStorage<K, V> *storage;
		int binIndex;
		HashMapEntry<K, V> *entry;

		template <class K2, class V2, unsigned int HASH(const K2&)> friend class Dictionary;
	};

	template <class K, class V, unsigned int HASH(const K&)>
	class Dictionary {

	public:
		/// LIFECYCLE
		
		// Default Constructor
		inline Dictionary(void) : ds(nullptr), isTemp(false) {}
		
		// Copy Constructor
		inline Dictionary(const Dictionary &other) : isTemp(false) { ((Dictionary&)other).ensureStorage(); ds = other.ds; retain(); }

		// Destructor
		virtual inline ~Dictionary(void) { release(); }
		
		/// OPERATORS
		
		// Assignment Operator
		Dictionary& operator=(const Dictionary &other) { ((Dictionary&)other).ensureStorage(); other.ds->refCount++; release(); ds = other.ds; isTemp = false; return *this; }
		
		/// OPERATIONS
		inline void SetValue(const K& key, const V& value);
		inline bool Remove(const K& key, V *output = nullptr);
		inline void RemoveAll();
		
		/// ACCESS
		inline V Lookup(const K& key, const V& defaultValue) const;
		inline const V operator[](const K& key) const;
		inline bool Get(const K& key, V *outValue) const;

		/// INQUIRY
		long Count() const { return ds ? ds->mSize : 0; }
		inline bool ContainsKey(const K& key) const;
		inline List<K> Keys() const;
		inline List<V> Values() const;
		inline bool empty() const { return Count() == 0; }
		
		/// ITERATION
		DictIterator<K,V> GetIterator() const { return DictIterator<K,V>(ds); }
		
		/// ASSIGNMENT OVERRIDE
		typedef bool (*AssignOverrideCallback)(Dictionary<K,V,HASH> &dict, K key, V value);
		void SetAssignOverride(AssignOverrideCallback callback) { ensureStorage(); ds->assignOverride = (void*)callback; }
		bool ApplyAssignOverride(K key, V value) {
			if (ds == NULL or ds->assignOverride == NULL) return false;
			AssignOverrideCallback cb = (AssignOverrideCallback)(ds->assignOverride);
			return cb(*this, key, value);
		}
		
		/// DEBUGGING
		inline int BinEntries(int binNum) const;
		
	protected:
		Dictionary(DictionaryStorage<K, V>* storage, bool temp=true) : ds(storage), isTemp(temp) { retain(); }

	private:
		friend class Value;
		
		inline int hashKey(const K& key) const;

		
		void forget() { ds = nullptr; }
		void retain() { if (ds && !isTemp) ds->retain(); }
		void release() { if (ds && !isTemp) { ds->release(); ds = nullptr; } }
		void ensureStorage() { if (!ds) ds = new DictionaryStorage<K, V>(); }
		DictionaryStorage<K, V> *ds;
		bool isTemp;	// indicates a temp dict, which does not participate in reference counting
};


	#pragma mark -
	#pragma mark Inline Method Implementation

	#pragma mark LIFECYCLE

	template <class K, class V, unsigned int HASH(const K&)>
	void Dictionary<K, V, HASH>::SetValue(const K& key, const V& value) {
		int hash = hashKey(key);
		ensureStorage();
		HashMapEntry<K, V> *entry = ds->mTable[hash];
		while (entry) {
			if (entry->key == key) {
				entry->value = value;
				return;
			}
			entry = entry->next;
		}
		
		// A KeyValuePair does not exist yet so make a new one
		entry = new HashMapEntry<K, V>();
		entry->key = key;
		entry->value = value;
		entry->next = ds->mTable[hash];
		ds->mTable[hash] = entry;

		ds->mSize++;
	}
	
	template <class K, class V, unsigned int HASH(const K&)>
	bool Dictionary<K, V, HASH>::Remove(const K& key, V *output) {
		if (!ds) return false;
		int hash = hashKey(key);
		HashMapEntry<K, V> *entry = ds->mTable[hash];
		HashMapEntry<K, V> *prev = nullptr;
		while (entry) {
			if (entry->key == key) {
				if (prev) prev->next = entry->next;
				else ds->mTable[hash] = entry->next;
				entry->next = nullptr;
				delete entry;
				ds->mSize--;
				return true;
			}
			prev = entry;
			entry = entry->next;
		}

		return false;
	}

	template <class K, class V, unsigned int HASH(const K&)>
	void Dictionary<K, V, HASH>::RemoveAll() {
		if (ds) ds->RemoveAll();
	}

	template <class K, class V, unsigned int HASH(const K&)>
	V Dictionary<K, V, HASH>::Lookup(const K& key, const V& defaultValue) const {
		if (!ds) return defaultValue;
		int hash = hashKey(key);
		HashMapEntry<K, V> *entry = ds->mTable[hash];
		while (entry) {
			if (entry->key == key) return entry->value;
			entry = entry->next;
		}
		
		return defaultValue;
	}

	template <class K, class V, unsigned int HASH(const K&)>
	bool Dictionary<K, V, HASH>::Get(const K& key, V *outValue) const {
		if (!ds) return false;
		int hash = hashKey(key);
		HashMapEntry<K, V> *entry = ds->mTable[hash];
		while (entry) {
			if (entry->key == key) {
				*outValue = entry->value;
				return true;
			}
			entry = entry->next;
		}
		
		return false;
	}

	template <class K, class V, unsigned int HASH(const K&)>
	const V Dictionary<K, V, HASH>::operator[](const K& key) const {
		Assert(ds);
		int hash = hashKey(key);
		HashMapEntry<K, V> *entry = ds->mTable[hash];
		while (entry) {
			if (entry->key == key) return entry->value;
			entry = entry->next;
		}
		Error("Dictionary key not found");
		return V();
	}
	

	#pragma mark -
	#pragma mark INQUIRY
	/// INQUIRY

	template <class K, class V, unsigned int HASH(const K&)>
	List<K> Dictionary<K, V, HASH>::Keys() const {
		List<K> keys;
		if (!ds) return keys;
		
		for (int i=0; i<TABLE_SIZE; i++) {
			HashMapEntry<K, V> *entry = ds->mTable[i];
			while (entry) {
				keys.Add(entry->key);
				entry = entry->next;
			}
		}
		
		return keys;
	}

	template <class K, class V, unsigned int HASH(const K&)>
	List<V> Dictionary<K, V, HASH>::Values() const {
		List<K> values;
		if (!ds) return values;
		
		for (int i=0; i<TABLE_SIZE; i++) {
			HashMapEntry<K, V> *entry = ds->mTable[i];
			while (entry) {
				values.Add(entry->value);
				entry = entry->next;
			}
		}
		
		return values;
	}

	template <class K, class V, unsigned int HASH(const K&)>
	bool Dictionary<K, V, HASH>::ContainsKey(const K& key) const {
		if (!ds) return false;
		int hash = hashKey(key);
		HashMapEntry<K, V> *entry = ds->mTable[hash];
		while (entry) {
			if (entry->key == key) return true;
			entry = entry->next;
		}

		return false;
	}
	
	template <class K, class V, unsigned int HASH(const K&)>
	int Dictionary<K, V, HASH>::BinEntries(int binNum) const {
		if (!ds) return 0;
		int count = 0;
		HashMapEntry<K, V> *entry = ds->mTable[binNum];
		while (entry) {
			count++;
			entry = entry->next;
		}
		return count;
	}

	#pragma mark -
	#pragma mark Private

	template <class K, class V, unsigned int HASH(const K&)>
	int Dictionary<K, V, HASH>::hashKey(const K& key) const {
		unsigned int hash = ((unsigned int)HASH(key)) % TABLE_SIZE;
		return hash;
	}

	// DictIterator methods:
	
	template <class K, class V>
	DictIterator<K, V>::DictIterator(DictionaryStorage<K, V> *storage) : storage(storage), binIndex(0) {
		// Find and attach to the first bin with any data in it.
		if (storage) {
			for (int i=0; i<TABLE_SIZE; i++) {
				if (storage->mTable[i]) {
					binIndex = i;
					entry = storage->mTable[i];
					return;
				}
			}
		}
		entry = nullptr;	// (dictionary is empty)
	}

	template <class K, class V>
	void DictIterator<K, V>::Next() {
		entry = entry->next;
		if (!entry) {
			// Advance to the next bin with any data in it.
			while (binIndex+1 < TABLE_SIZE) {
				binIndex++;
				entry = storage->mTable[binIndex];
				if (entry) return;
			}
		}
	}


	
	// Some hash methods convenient for use with Dictionary:
	
	inline unsigned int hashUInt(const unsigned int &xin) {
		unsigned int x = xin;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
		x = (x >> 16) ^ x;
		return x;
	}

	inline unsigned int hashInt(const int &x) {
		return hashUInt((unsigned int)x);
	}
	
	inline unsigned int hashUShort(const unsigned short &x) {
		return hashUInt(x);
	}

	inline unsigned int hashShort(const short &x) {
		return hashUInt((unsigned int)x);
	}
	
}
#endif  // HASHMAP_H
