//
//  MSList.h
//  A reference-counted resizable array.
//
//  Created by Joe Strout on 3/11/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MSLIST_H
#define MSLIST_H

#include "QA.h"

// For now, we'll build it around SimpleVector.
// ToDo: merge that into here, and SimpleVector goes away.
#include "SimpleVector.h"
#include "RefCountedStorage.h"

namespace MiniScript {
	
	template <class T>
	class ListStorage : public RefCountedStorage, public SimpleVector<T> {
	private:
		ListStorage() {}
		ListStorage(long slots) : SimpleVector<T>(slots) {}
		virtual ~ListStorage() {}
		
		template <class T2> friend class List;
	};

	template <class T>
	class List {
	public:
		// constructors and assignment-op
		List(long sizeHint=0) : ls(nullptr), isTemp(false) { if (sizeHint) ls = new ListStorage<T>(sizeHint); }
		List(const List& other) : isTemp(false) { ((List&)other).ensureStorage(); ls = other.ls; retain(); }
		List& operator= (const List& other) { ((List&)other).ensureStorage(); other.ls->refCount++; release(); ls = other.ls; isTemp = false; return *this; }

		// inspectors
		long Count() const { return ls ? ls->size() : 0; }
		long IndexOf(T item) const { return ls ? ls->indexOf(item) : -1; }
		bool Contains(T item) const { return ls ? ls->Contains(item) : false; }
		T& Last() const { Assert(ls); return ls->peek_back(); }
		
		// mutators
		void Add(T item) { ensureStorage(); ls->push_back(item); }
		void Clear() { if (ls) ls->deleteAll(); }
		void Insert(T item, long index) { ensureStorage(); ls->insert(item, index); }
		void RemoveAt(long index) { if (ls) ls->deleteIdx(index); }
		void RemoveRange(long startIndex, long count) { for (long i=0; i<count; i++) RemoveAt(startIndex); }	// OFI: do this without looping
		void Reposition(long indexFrom, long indexTo) { if (ls) ls->reposition(indexFrom, indexTo); }
		T Pop() { Assert(ls); return ls->pop_back(); }
		void ResizeBuffer(long newBufSize) { if (newBufSize == 0) Clear(); else { ensureStorage(); ls->resizeBuffer(newBufSize); } }
		void Resize(long newLength) { if (newLength == 0) Clear(); else { ensureStorage(); ls->resize(newLength); } }
		void Reverse() { if (ls) ls->reverse(); }
		void EnsureStorage() { ensureStorage(); }	// (call before copying a reference, if you want both to refer to same object)
		
		// array-like access (both read and write)
		inline T& operator[](const long idx) { Assert(ls); return (*ls)[idx]; }
		inline T& operator[](const long idx) const { Assert(ls); return (*ls)[idx]; }
		
		// Explicit get/set item indexes wrap around, so -1 is the last item, -2 is two from the end, etc.
		inline T& Item(long idx) const { Assert(ls); return ls->item(idx); }
		inline void SetItem(long idx, const T& item) { Assert(ls); ls->setItem(idx, item); }

		// destructor
		~List() { release(); }

	private:
		friend class Value;
		
		List(ListStorage<T>* storage, bool temp=true) : ls(storage), isTemp(temp) { retain(); }
		void forget() { ls = nullptr; }
		
		void retain() { if (ls and !isTemp) ls->refCount++; }
		void release() { if (ls and !isTemp and --(ls->refCount) == 0) { delete ls; ls = nullptr; } }
		void ensureStorage() { if (!ls) ls = new ListStorage<T>(); }
		ListStorage<T> *ls;
		bool isTemp;	// indicates temp wrapper which does not participate in ref counting
	};
}

#endif /* LIST_H */
