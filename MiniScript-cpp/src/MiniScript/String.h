// SimpleString.h
//
//	This is a simple String class based on the interface of the STL String,
//	but simplified a bit and not requiring the rest of the STL.

#ifndef SIMPLESTRING_H
#define SIMPLESTRING_H

#include <ciso646>  // (force non-conforming compilers to join the 21st century)
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <cctype>
#include "RefCountedStorage.h"

namespace MiniScript {

	using std::tolower;
	using std::toupper;

	class String;
	
	class StringStorage : public RefCountedStorage {
	private:
		StringStorage() : data(nullptr), dataSize(0), charCount(-1) {
#if(DEBUG)
			instanceCount++;
#endif
		}
		StringStorage(size_t bufSize) : dataSize(bufSize), charCount(-1) {
			data = new char[bufSize];
			memset(data, 0, bufSize);
#if(DEBUG)
			instanceCount++;
#endif
		}
		virtual ~StringStorage() {
			if (data) delete[] data;
#if(DEBUG)
			instanceCount--;
#endif
		}
		
		char *data;
		size_t dataSize;
		
		// some cached data for efficiency:
		long charCount; // -1 when not yet known
		bool isASCII;   // if charCount > 0 and isASCII==true, then this String is 1 byte per character
		
		friend class String;
		friend class Value;
		inline friend String operator+ (const char *c, const String& s);
		void analyzeChars();
#if(DEBUG)
	public:
		static long instanceCount;
#endif
	};

	#pragma mark -

	class String {
	public:
		// constructors
		String() :ss(nullptr), isTemp(false) {}
		String(const String& other) : isTemp(false) { ss = other.ss; retain(); }
		inline String(int count, char c);
		inline String(const char* c);
		inline String(const char* buf, size_t bytes);
		inline String(const char);
		
		// destructor
		~String() { release(); }
		
		// operators
		String& operator= (const String& other) { if (other != *this) { if (other.ss) other.ss->refCount++; release(); ss = other.ss; isTemp = false; } return *this; }
		inline String& operator=(const char c);
		inline String& operator= (const char* c);
		inline String operator+ (const String& other) const;
		inline String operator+ (const char* c) const;
		inline friend String operator+ (const char *c, const String& s);
		
		// inspectors
		// NOTE: variants that end in "B" are byte-oriented; without the "B",
		// it is UTF-8 character-oriented (or is a method where it doesn't matter).
		// Single UTF-8 characters are returned as ints.
		size_t sizeB() const { return ss ? ss->dataSize - 1 : 0; }
		size_t LengthB() const { return sizeB(); }
		bool empty() const { return !ss or ss->dataSize <= 1; }
		const char *c_str() const { return ss ? ss->data : ""; }
		const char *data() const { return ss ? ss->data : nullptr; }
		inline unsigned char operator[] (size_t posB) const;		// important: [] works on bytes, not characters!
		inline char atB(size_t posB) const;
		int at(size_t pos) const;
		size_t bytePosOfCharPos(size_t pos) const;
		size_t charPosOfBytePos(size_t posB) const;
		inline size_t copyB(char* dest, size_t numBytes, long posB=0) const;
		inline long IndexOfB(const char *c, long posB=0) const;
		inline long IndexOfB(const String &s, long posB=0) const;
		inline long LastIndexOfB(const char *c, long posB=-1) const;
		inline long LastIndexOfB(const String &s, long posB=-1) const;
		inline long LastIndexOf(const char *c, long posB=-1) const;
		inline long LastIndexOf(const String &s, long posB=-1) const;
		inline String SubstringB(long posB, long LengthB=-1) const;
		inline bool Contains(const String &other) const;
		
		inline long Length() const { if (!ss) return 0; if (ss->charCount < 0) ss->analyzeChars(); return ss->charCount; }
		inline long IndexOf(const char *c, long pos=0) const;
		inline long IndexOf(const String &s, long pos=0) const;
		// ToDo: LastIndexOf (character-oriented)
		String Substring(long pos, long numChars=-1) const;
		bool EndsWith(const String& s) const;
		bool StartsWith(const String& s) const;
		
		// comparison operators and methods
		inline static int Compare(const String& lhs, const String& rhs) { return lhs.Compare(rhs); }
		inline int Compare(const String& s) const;
		inline int Compare(const char *c) const;
		bool operator== (const String& s) const { return Compare(s) == 0; }
		bool operator!= (const String& s) const { return Compare(s) != 0; }
		bool operator> (const String& s) const { return Compare(s) > 0; }
		bool operator< (const String& s) const { return Compare(s) < 0; }
		bool operator>= (const String& s) const { return Compare(s) >= 0; }
		bool operator<= (const String& s) const { return Compare(s) <= 0; }

		bool operator== (const char *c) const { return Compare(c) == 0; }
		bool operator!= (const char *c) const { return Compare(c) != 0; }
		bool operator> (const char *c) const { return Compare(c) > 0; }
		bool operator< (const char *c) const { return Compare(c) < 0; }
		bool operator>= (const char *c) const { return Compare(c) >= 0; }
		bool operator<= (const char *c) const { return Compare(c) <= 0; }
		
		// mutators (note: these bind this String to a new storage;
		// we NEVER mutate the String storage itself)
		inline String& Append(const String& other);
		inline String& operator+= (const String& other) { return this->Append(other); }
		inline String& assign(const String& s) { return (*this = s); }
		inline String& assign(const char *c) { return (*this = c); }

		inline String Trim(char c = ' ');
		inline String TrimEnd(char c = ' ');
		inline String TrimStart(char c = ' ');
		
		inline String& Replace(String replaceWhat, String withWhat);
		inline String& ReplaceB(long startPosB, long LengthB, String newString);
		
		inline String& takeoverBuffer(char *buffer, long strBytes = -1);
		
		static String Format(int num, const char* formatSpec = "%d");
		static String Format(long num, const char* formatSpec = "%ld");
		static String Format(float num, const char* formatSpec = "%g");
		static String Format(double num, const char* formatSpec = "%lg");
		static String Format(bool value, const char* trueString = "True", const char* falseString = "False" );

		int IntValue(const char* formatSpec = "%d") const;
		long LongValue(const char* formatSpec = "%ld") const;
		float FloatValue(const char* formatSpec = "%f") const;
		double DoubleValue(const char* formatSpec = "%lf") const;
		bool BooleanValue() const;
		
		inline String ToLower() const;
		inline String ToUpper() const;

		inline unsigned int Hash() const;
		
		friend class Value;
		
	private:
		// Constructor to make a string directly from StringStorage.  If temp is true,
		// then the storage will not be retained or released (useful when quickly
		// making a String out of some other storage we know won't go away while we
		// work with it).  If false, we still don't retain it (since the storage
		// should already have a refCount of 1), but we will release it when done.
		String(StringStorage* storage, bool temp=true) : ss(storage), isTemp(temp) {}
		void retain() { if (ss and !isTemp) ss->retain(); }
		void release() { if (ss and !isTemp) { ss->release(); ss = nullptr; } }
		
		void forget() { ss = nullptr; }	// used for unretained temps, or when a Value has adopted the reference
		
		inline String TrimHelper(char c, bool left, bool right);
		
		StringStorage *ss;
		bool isTemp;	// true when we are a temp string, and don't participate in reference counting
	};

	#pragma mark -

	String::String(int count, char c) : isTemp(false) {
		if (count < 1) {
			ss = nullptr;
		} else {

			ss = new StringStorage(count+1);
			for (int i = 0; i < count; i++) ss->data[i] = c;
			ss->data[count+1] = 0;
		}
	}

	String::String(const char* c) : isTemp(false) {
		size_t n = strlen(c);
		if (!n) {
			ss = nullptr;
		} else {
			ss = new StringStorage(n+1);
			memcpy(ss->data, c, n+1);
		}
	}

	String::String(const char* buf, size_t bytes) : isTemp(false) {
		if (!bytes) {
			ss = nullptr;
		} else {
			ss = new StringStorage(bytes+1);
			memcpy(ss->data, buf, bytes);
			ss->data[bytes] = 0;
		}
	}

	inline String::String(const char c) : isTemp(false) {
		ss = new StringStorage(2);
		ss->data[0] = c;
		ss->data[1] = 0;
	}

	String& String::operator=(const char* c) {
		release();
		size_t n = strlen(c);
		if (!n) {
			ss = nullptr;
		} else {
			ss = new StringStorage(n+1);
			memcpy(ss->data, c, n+1);
		}
		isTemp = false;
		return *this;
	}

	String& String::operator=(const char c) {
		release();
		
		ss = new StringStorage(2);
		ss->data[0] = c;
		ss->data[1] = 0;
		
		isTemp = false;
		return *this;
	}


	unsigned char String::operator[] (size_t pos) const {
		return ss->data[pos];
	}

	char String::atB(size_t pos) const {
		// NOTE: the standard String class raises an exception for out-of-bounds
		// values, but we use an assert here instead.  The caller can (and probably
		// should) do their own range checking if such a case is expected.
		assert(ss and pos >= 0 and pos < ss->dataSize);
		return ss->data[pos];
	}

	size_t String::copyB(char* dest, size_t n, long pos) const {
		if (!ss) return 0;
		if (n > ss->dataSize) n = ss->dataSize;
		strncpy(dest, ss->data, n);
		return n;
	}

	int String::Compare(const String& s) const {
		StringStorage *sa = ss;
		if (sa and sa->dataSize <= 1) sa = nullptr;		// normalize empty string and null string
		StringStorage *sb = s.ss;
		if (sb and sb->dataSize <= 1) sb = nullptr;
		if (sa == sb) return 0;		// same data (including both nullptr): equal
		if (not sb) return 1;		// second String nullptr: first is greater
		if (not sa) return -1;		// first String nullptr: second is greater
		return strcmp(sa->data, sb->data);
	}

	int String::Compare(const char *c) const {
		if ((!c or *c == 0) and !ss) return 0;   // both nullptr: equal
		if (!c or *c == 0) return 1;           // second String nullptr: first is greater
		if (!ss) return -1;         // first String nullptr: second is greater
		return strcmp(ss->data, c);
	}

	bool String::Contains(const String &other) const {
		return IndexOfB(other, 0) > -1;
	}

	long String::IndexOfB(const char *c, long posB) const {
		if (!ss) return -1;
		if (!c) return posB;
		long clen = strlen(c);
		long maxi = ss->dataSize - 1 - clen;
		for (long i = posB; i <= maxi; i++) {
			if (strncmp(ss->data+i, c, clen) == 0) return i;
		}
		return -1;
	}

	long String::IndexOfB(const String &other, long posB) const {
		if (!ss) return -1;
		if (!other.ss) return posB;
		long clen = other.ss->dataSize - 1;
		long maxi = ss->dataSize - 1 - clen;
		for (long i = posB; i <= maxi; i++) {
			if (strncmp(ss->data+i, other.ss->data, clen) == 0) return i;
		}
		return -1;
	}
	
	long String::IndexOf(const char *c, long pos) const {
		long result = IndexOfB(c, bytePosOfCharPos(pos));
		return result > 0 ? charPosOfBytePos(result) : result;
	}

	long String::IndexOf(const String &s, long pos) const {
		long result = IndexOfB(s, bytePosOfCharPos(pos));
		return result > 0 ? charPosOfBytePos(result) : result;
	}

	long String::LastIndexOfB(const char *c, long pos) const {
		if (!ss) return -1;
		if (!c) return pos;
		long clen = strlen(c);
		long maxi = ss->dataSize - 1 - clen;
		if (pos != -1 and pos < maxi) maxi = pos;
		for (long i = maxi; i >= 0; i--) {
			if (strncmp(ss->data+i, c, clen) == 0) return i;
		}
		return -1;
	}

	long String::LastIndexOfB(const String &other, long pos) const {
		if (!ss) return -1;
		if (!other.ss) return pos;
		size_t clen = other.ss->dataSize - 1;
		size_t maxi = ss->dataSize - 1 - clen;
		if (pos != -1 and pos < (long)maxi) maxi = pos;
		for (long i = maxi; i >= 0; i--) {
			if (strncmp(ss->data+i, other.ss->data, clen) == 0) return i;
		}
		return -1;
	}

	long String::LastIndexOf(const char *c, long pos) const {
		long result = LastIndexOfB(c, bytePosOfCharPos(pos));
		return result > 0 ? charPosOfBytePos(result) : result;
	}
	
	long String::LastIndexOf(const String &s, long pos) const {
		long result = LastIndexOfB(s, bytePosOfCharPos(pos));
		return result > 0 ? charPosOfBytePos(result) : result;
	}
	

	String String::SubstringB(long posB, long LengthB) const {
		if (!ss) return *this;
		if (posB >= ss->dataSize) return String();
		if (LengthB == -1 or LengthB > (long)ss->dataSize-1 - posB) {
			LengthB = ss->dataSize-1 - posB;
		}
		if (posB == 0 and LengthB == (long)ss->dataSize) return *this;
		
		StringStorage *newbie = new StringStorage(LengthB+1);
		memcpy(newbie->data, ss->data+posB, LengthB);

		#if DEBUG
			assert(newbie->dataSize-1 == strlen(newbie->data));
		#endif
		
		return String(newbie, false);	// LEAK
	}
	
	inline String& String::Append(const String& other) {
		if (!other.ss) return *this;  // appending empty String; nothing to do
		if (!ss) {
			*this = other;
		} else {
			size_t n1 = ss ? ss->dataSize - 1 : 0;
			size_t n2 = other.ss ? other.ss->dataSize - 1 : 0;
			StringStorage* newbie = new StringStorage(n1 + n2 + 1);
			memcpy(newbie->data, ss->data, n1);
			memcpy(newbie->data+n1, other.ss->data, n2+1);
			release();
			ss = newbie;
			isTemp = false;
		}
		return *this;
	}

	inline String String::Trim(char c) {
	   return TrimHelper(c,true,true);
	}

	inline String String::TrimEnd(char c) {
	   return TrimHelper(c,false,true);
	}

	inline String String::TrimStart(char c) {
	   return TrimHelper(c,true,false);
	}

	inline String String::TrimHelper(char TrimC, bool left, bool right) {
		String out;
		if (!ss) return out;
		
		size_t newSize = ss->dataSize-1;
		char *start = ss->data;
		char *end = start + ss->dataSize - 2; // point at the last character not the terminating nullptr char
		
		if (left) {
			while(*start == TrimC and newSize > 0) {
				newSize--;
				start++;
			}
		}
		
		if (right) {
			while(*end == TrimC and newSize > 0) {
				newSize--;
				end--;
			}
		}
		
		if (0 == newSize) {
			return out;
		}
		
		StringStorage *newbie = new StringStorage(newSize +1);
		memcpy(newbie->data, start, newSize);
		out.ss = newbie;
		
		return out;
	}

	inline String& String::Replace(String replaceWhat, String withWhat) {
		long replLenB = replaceWhat.LengthB();
		long withLenB = withWhat.LengthB();
		long posB = IndexOfB(replaceWhat);
		while (posB >= 0) {
			*this = ReplaceB(posB, replLenB, withWhat);
			posB = IndexOfB(replaceWhat, posB + withLenB);
		}
		return *this;
	}

	
	inline String& String::ReplaceB(long startPos, long numBytes, String newString) {
		*this = SubstringB(0, startPos) + newString + SubstringB(startPos+numBytes, -1);
		return *this;
	}

	inline String& String::takeoverBuffer(char *buffer, long strBytes) {
		release();
		StringStorage* newbie = new StringStorage();
		newbie->data = buffer;
		if (strBytes >= 0) newbie->dataSize = strBytes + 1; // +1 for the nullptr character
		else if (buffer) newbie->dataSize = strlen(buffer) + 1; // (same)
		ss = newbie;
		isTemp = false;
		return *this;
	}

	String String::operator+ (const String& other) const {
		if (!other.ss) return *this;
		if (!ss) return other;
		
		size_t n1 = ss ? ss->dataSize - 1 : 0;
		size_t n2 = other.ss ? other.ss->dataSize - 1 : 0;
		StringStorage* newbie = new StringStorage(n1 + n2 + 1);
		memcpy(newbie->data, ss->data, n1);
		memcpy(newbie->data+n1, other.ss->data, n2+1);
		return String(newbie, false);		// LEAK
	}

	String String::operator+ (const char* c) const {
		if (!c) return *this;
		if (!ss) return String(c);
		
		size_t n1 = ss ? ss->dataSize - 1 : 0;
		size_t n2 = strlen(c);
		StringStorage* newbie = new StringStorage(n1 + n2 + 1);
		memcpy(newbie->data, ss->data, n1);
		memcpy(newbie->data+n1, c, n2+1);
		return String(newbie, false);	// LEAK
	}

	String operator+ (const char *c, const String& s) {
		if (!c) return s;
		if (!s.ss) return String(c);
		size_t n1 = strlen(c);
		size_t n2 = s.ss ? s.ss->dataSize - 1 : 0;
		StringStorage* newbie = new StringStorage(n1 + n2 + 1);
		memcpy(newbie->data, c, n1);
		memcpy(newbie->data+n1, s.ss->data, n2+1);
		return String(newbie, false);	// LEAK
	}

	inline String String::ToLower() const {
		String out;
		if (ss != nullptr) {
			out.ss = new StringStorage(ss->dataSize);
			memcpy(out.ss->data, ss->data, ss->dataSize);
			for (unsigned int i = 0; i < out.ss->dataSize; i++) {
				out.ss->data[i] = tolower(out.ss->data[i]);
			}
		}
		return out;
	}

	inline String String::ToUpper() const {
		String out;
		if (ss) {
			out.ss = new StringStorage(ss->dataSize);
			memcpy(out.ss->data, ss->data, ss->dataSize);
			for (unsigned int i = 0; i < out.ss->dataSize; i++) {
				out.ss->data[i] = toupper(out.ss->data[i]);
			}
		}
		return out;
	}

	// Comparision between cstrings and strings
	bool operator==(const char *cstring, const String &str);
	bool operator!=(const char *cstring, const String &str);
	bool operator<(const char *cstring, const String &str);
	bool operator<=(const char *cstring, const String &str);
	bool operator>(const char *cstring, const String &str);
	bool operator>=(const char *cstring, const String &str);

	unsigned int String::Hash() const {
		// http://isthe.com/chongo/tech/comp/fnv/#FNV-1a
		
		const unsigned int fnv_prime = 16777619u;
		unsigned int hash = 2166136261u;
		unsigned long bytes = LengthB();
		for (unsigned long i = 0; i < bytes; i++) {
			hash ^= ss->data[i];
			hash *= fnv_prime;
		}
		
		return hash;
	}
	
	// hash interface compatible with Dictionary:
	inline unsigned int hashString(const String& key) {
		return key.Hash();
	}

}

#endif // SIMPLESTRING_H
