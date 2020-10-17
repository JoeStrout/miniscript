//	SimpleVector.h                                      JJS  10 Dec 1999

//	SimpleVector.h
//
//	This is a simple replacement for the STL vector template class.
//	It's considerably smaller, leaner, and easier to use than the STL vector.
//	Much of the interface is the same, however, so if we later decide to
//	switch to the STL vector class or some other container, it shouldn't
//	be too difficult.
//
//	NOTE: this container does bytewise copies of its elements, so it won't
//	work for classes with non-trivial copy-constructors.  (It's mainly
//	intended for small elements like numbers and pointers.)

#ifndef SIMPLEVECTOR_H
#define SIMPLEVECTOR_H

#include "QA.h"

#include <iostream> // HACK for debugging

template <class T>
class SimpleVector {

  public:
	// constructors
	inline SimpleVector();						// sets size to 0
	inline SimpleVector(long n);			// allocates n slots
	inline SimpleVector(const SimpleVector<T>&);	// copy-constructor (copies data)

	// assignment-op
	inline SimpleVector& operator=(const SimpleVector<T>&);	// (copies data)

	// destructor
	inline ~SimpleVector();
	
	// inspectors
	inline unsigned long size() const;			// get number of items
	inline unsigned long bufitems() const;		// number of items the buffer can hold
	inline unsigned long bufbytes() const;		// size of buffer in bytes
	inline bool empty() const { return size() == 0; }
    
	// insertion (moves all following items)
	inline void insert(const T& item, const long idx);

    // repositioning -- pluck an element out of idx1, and insert it at idx2
    inline void reposition(const long idx1, const long idx2);
    
	// treating it like an array
	inline T& operator[](long idx);		// get item reference by index
	inline T& operator[](long idx) const;
	inline T& item(long idx) const;		// get item at index with wrap-around
	inline void setItem(long idx, const T& item); 
	
	
	// treating it like a stack
	inline void push_back(const T& item);		// push onto end of the vector
	inline T pop_back();						// get last item, remove from vector
	inline T& peek_back();						// get last item, don't remove from vector

	// other ways to delete items
	inline void deleteIdx(long idx);		// delete an item by its index
	inline void deleteAll();					// delete all items

	// containment inspectors
	inline long indexOf(const T& item);
	inline bool Contains(const T& item);
	
	// buffer management
	inline void resizeBuffer(long n);		// allocate n slots, keeping current data and qty
    inline void resize(long n);           // resize buffer AND change size()

	unsigned long mBlockItems;		// number of items to allocate when we expand; or if 0,
									// then expand by simply doubling the buffer size
    
    // major mutators               
    inline void reverse();                 // reverses the elements in the array in place.
    
    
  protected:
	T *mBuf;						// array of items
	unsigned long mQtyItems;		// how many items we actually have
	unsigned long mBufItems;		// number of items the buffer can hold
};

#define VecIterate(var,vec) for (unsigned long var=0;var<(vec).size();var++)
#define VecReverseIterate(var,vec) for (long var=(vec).size()-1;var>=0;var--)

void runSimpleVectorUnitTests();

template <class T>
inline SimpleVector<T>::SimpleVector()
:  mBlockItems(0), mBuf(nullptr), mQtyItems(0), mBufItems(0)
{
//	std::cout << "created default SimpleVector at " << (long)(this) << std::endl;
}

template <class T>
inline SimpleVector<T>::SimpleVector(long n)
: mBlockItems(0), mQtyItems(0), mBufItems(n)
{
//	std::cout << "created SimpleVector with capacity " << n << " at " << (long)(this) << std::endl;
	if (n) {
		#if USE_EXCEPTIONS
			try {
				mBuf = new T[mBufItems];
			} catch (...) {
				throw memFullErr;
			}
		#else
			mBuf = new T[mBufItems];
			Assert(mBuf);
		#endif
	} else mBuf = nullptr;
}

template <class T>
inline SimpleVector<T>::SimpleVector(const SimpleVector<T>& vec)
: mBuf(nullptr), mBufItems(0)
{
//	std::cout << "created SimpleVector at " << (long)(this) << " by copying one at " << (long)(&vec) << std::endl;

	*this = vec;
}
	
template <class T>
inline SimpleVector<T>& SimpleVector<T>::operator=(const SimpleVector<T>& vec)
{
	if (mBuf) delete[] mBuf;
	mBlockItems = vec.mBlockItems;
	mBufItems = vec.mBufItems;
	mQtyItems = vec.mQtyItems;
	#if USE_EXCEPTIONS
		try {
			mBuf = new T[mBufItems];
		} catch (...) {
			mBufItems = mQtyItems = 0;
			throw memFullErr;
		}
	#else
		mBuf = nullptr;
		if (mBufItems > 0) {
			mBuf = new T[mBufItems];
			Assert(mBuf);
		}
	#endif
	
	if (mBuf) {
		// Mar 04 2002 -- MJS (1)
		T* src = vec.mBuf;
		T* dest = mBuf;
		T* end = &vec.mBuf[vec.mQtyItems];
		while( src < end ) {
			*dest++ = *src++;
		}
	}
	
	return *this;
}

template <class T>
inline SimpleVector<T>::~SimpleVector()
{
	if (mBuf) delete[] mBuf;
//	std::cout << "Delete SimpleVector at " << (long)(this);
}

template <class T>
inline unsigned long SimpleVector<T>::size() const
{
	return mQtyItems;
}

template <class T>
inline unsigned long SimpleVector<T>::bufitems() const
{
	return mBufItems;
}

template <class T>
inline unsigned long SimpleVector<T>::bufbytes() const
{
	return mBufItems * sizeof(T);
}

template <class T>
inline T& SimpleVector<T>::operator[](const long idx)
{
	if (idx < 0 or idx >= (long)mQtyItems) {
		#if USE_EXCEPTIONS
			throw memFullErr;
		#else
			Error("invalid index in SimpleVector::operator[]");
		#endif
		return mBuf[0];
	}

	return mBuf[idx];
}

template <class T>
inline T& SimpleVector<T>::operator[](const long idx) const
{
	if (idx < 0 or idx >= (long)mQtyItems) {
		#if USE_EXCEPTIONS
			throw memFullErr;
		#else
			Error("invalid index in SimpleVector::operator[]");
		#endif
		return mBuf[0];
	}

	return mBuf[idx];
}
template <class T>
inline T& SimpleVector<T>::item(long idx) const {
	if (mQtyItems == 0) {
		#if USE_EXCEPTIONS
			throw memFullErr;
		#else
			Error("invalid index in SimpleVector::item");
		#endif
		return mBuf[0];
	}
	
	idx = idx % (long)mQtyItems;
	if (idx < 0) idx += mQtyItems;
	return mBuf[idx];
}

template <class T>
inline void SimpleVector<T>::setItem(long idx, const T& item) {
	if (mQtyItems == 0) {
		#if USE_EXCEPTIONS
			throw memFullErr;
		#else
			Error("invalid index in SimpleVector::setItem");
		#endif
		return;
	}
	
	idx =idx % (long)mQtyItems;
	if (idx < 0) idx += mQtyItems;
	mBuf[idx] = item;
}

template <class T>
inline void SimpleVector<T>::push_back(const T& item)
{
	// do we need to increase the buffer size?
	while (mQtyItems >= mBufItems) {
		// yes -- expand it by one block (should never need more than that!),
		// or by double the size
		unsigned long expandBy = (mBlockItems > 0 ? mBlockItems : mBufItems);
		if (expandBy < 16) expandBy = 16;
		resizeBuffer( mBufItems + expandBy );
	}
	// stuff the item
	mBuf[mQtyItems] = item;
	mQtyItems++;
}

template <class T>
inline T SimpleVector<T>::pop_back()
{
	#if USE_EXCEPTIONS
		if (mQtyItems > mBufItems || mQtyItems <= 0) throw memAdrErr;
	#else
		if (mQtyItems > mBufItems || mQtyItems <= 0) Error("pop_back called on empty SimpleVector");;
	#endif
	return mBuf[--mQtyItems];
}

template <class T>
inline T& SimpleVector<T>::peek_back()
{
	#if USE_EXCEPTIONS
		if (mQtyItems > mBufItems || mQtyItems <= 0) throw memAdrErr;
	#else
		if (mQtyItems > mBufItems || mQtyItems <= 0) Error("peek_back called on empty SimpleVector");
	#endif
	return mBuf[mQtyItems-1];
}

template <class T>
inline void SimpleVector<T>::insert(const T& item, const long idx)
{
	if (idx < 0 or idx > (long)mQtyItems) {
		#if USE_EXCEPTIONS
			throw memFullErr;
		#else
			Error("invalid index in SimpleVector::insert");;
		#endif
		return;
	}

	// resize the buffer if needed
	while (mQtyItems >= mBufItems) {
		// yes -- expand it by one block (should never need more than that!),
		// or by double the size
		unsigned long expandBy = (mBlockItems > 0 ? mBlockItems : mBufItems);
		if (expandBy < 16) expandBy = 16;
		resizeBuffer( mBufItems + expandBy );
	}
	
	// move all items past idx
	if (idx < (long)mQtyItems) {
		T* src = &mBuf[mQtyItems-1];
		T* dest = &mBuf[mQtyItems];
		T* end = &mBuf[idx];
		while (src >= end) {
			*dest-- = *src--;
		}
	}
	
	// finally, stuff the item
	mBuf[idx] = item;
	mQtyItems++;	
}

template <class T>
inline void SimpleVector<T>::reposition(const long idx1, const long idx2)
{
    if (idx1 == idx2) return;
    
	if (idx1 < 0 or idx1 >= (long)mQtyItems or idx2 < 0 or idx2 >= (long)mQtyItems) {
		#if USE_EXCEPTIONS
			throw memFullErr;
		#else
			Error("invalid index in SimpleVector::reposition");
		#endif
	}
    
    // Grab the item we're moving
    T mover = mBuf[idx1];
    
    if (idx2 < idx1) {
        // Moving this item towards 0; shift all elements
        // from idx2 to idx1-1 forward one position.
		T* src = &mBuf[idx1-1];
		T* dest = &mBuf[idx1];
		T* end = &mBuf[idx2];	
		while (src >= end) {
			*dest-- = *src--;
		}
    } else {
        // Moving this item away from 0; shift all elements
        // from idx1+1 to idx2 back one position.
		T* src = &mBuf[idx1+1];
		T* dest = &mBuf[idx1];
		T* end = &mBuf[idx2+1];	
		while (src < end) {
			*dest++ = *src++;
		}		
    }
    
    // Stuff the item we're moving.
    mBuf[idx2] = mover;
}

template <class T>
inline void SimpleVector<T>::deleteIdx(const long idx)
{
	if (idx < 0 || idx >= (long)mQtyItems) {
		#if DEBUG
			//DEBUGSTR("\pInvalid SimpleVector index!");
		#endif
		return;
	}
	
	if (idx == (long)mQtyItems-1) {
		// special case -- deleting last item, no need to copy
		mQtyItems -= 1;
	} else {
		// if deleting any but the last item, move remaining ones down
		// Mar 04 2002 -- MJS (1)
		T* dest = &mBuf[idx];
		T* src = &mBuf[idx + 1];
		T* end = &mBuf[mQtyItems];
		while(src < end) {
			*dest++ = *src++;
		}
		mQtyItems -= 1;
	}
	// now check -- should we shrink the buffer down?					
	// do so if the unused spaces are more than twice the block size,
	// or in dynamic mode, if unused space is over twice the used space
	unsigned long unused = (mBufItems - mQtyItems);
	if (mBlockItems > 0) {
		if (unused > mBlockItems*2) {
			// round to the nearest even block
			unsigned long newqty = (1 + mQtyItems / mBlockItems) * mBlockItems;
			resizeBuffer(newqty);
		}
	} else {
		if (unused > mQtyItems * 2) {
			// when using automatic block resizing, just round to
			// the nearest 16 entries
			unsigned long newqty = (1 + mQtyItems / 16) * 16;
			resizeBuffer(newqty);
		}
	}
}

template <class T>
inline void SimpleVector<T>::deleteAll()
{
	delete[] mBuf;
	mBuf = 0;
	mBufItems = mQtyItems = 0;
}

template <class T>
inline long SimpleVector<T>::indexOf(const T& item) {

	VecIterate(i, *this) {
		if (mBuf[i] == item) return i;
	}
	return -1;
}

template <class T>
inline bool SimpleVector<T>::Contains(const T& item) {
	return indexOf(item) != -1;
}

template <class T>
inline void SimpleVector<T>::resizeBuffer(long n)
{
	if (n == (long)mBufItems) return;
	T *newbuf = new T[n];
//	if (!newbuf) throw memFullErr;	// (not needed, as new now throws if it fails)
	if (mBuf) {
		T* src = mBuf;
		T* dest = newbuf;
		T* end = &mBuf[((long)mQtyItems < n) ? mQtyItems : n];	// the smaller value
		while (src < end) {
			*dest++ = *src++;
		}
		delete[] mBuf;
	}
	mBuf = newbuf;
	mBufItems = n;
	if (mQtyItems > mBufItems) mQtyItems = mBufItems;
 }

template <class T>
inline void SimpleVector<T>::resize(long n)
{
    resizeBuffer(n);
    mQtyItems = n;
}

template <class T>
inline void SimpleVector<T>::reverse() {
    unsigned long low = 0;
    unsigned long high = mQtyItems - 1;
    
    while (low < high) {
        T temp = mBuf[low];
        mBuf[low] = mBuf[high];
        mBuf[high] = temp;
        low++;
        high--;
    }
}

#endif
