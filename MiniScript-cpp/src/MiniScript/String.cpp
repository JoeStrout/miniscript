/*
 *  SimpleString.cpp
 *  drawgame
 *
 *  Created by April on 12/23/10.
 *  Copyright 2010 Luminary Apps. All rights reserved.
 *
 */
#include "String.h"
#include "UnicodeUtil.h"
#include "UnitTest.h"
#include <stdio.h>
#include <cmath>

namespace MiniScript {

#if(DEBUG)
	long RefCountedStorage::instanceCount = 0;
	long StringStorage::instanceCount = 0;
#endif

	
	using std::fabs;
	
	// at
	//
	//	Get the UTF-8 character at the given character index.
	int String::at(size_t pos) const {
		if (!ss) return 0;
		if (ss->charCount < 0) ss->analyzeChars();
		if (ss->isASCII) {
			// Easy case: this String is all ASCII, so we can grab the requested character directly.
			return (pos < ss->dataSize ? ss->data[pos] : 0);
		}
		// Harder case: we have some multi-byte characters, so we have to iterate.
		unsigned char *c = (unsigned char*)(ss->data);
		unsigned char *maxc = c + ss->dataSize;
		AdvanceUTF8(&c, maxc, (int)pos);
		return (int)UTF8Decode(c);
	}

	size_t String::bytePosOfCharPos(size_t pos) const {
		if (pos <= 0 || !ss) return 0;
		if (ss->charCount < 0) ss->analyzeChars();
		if (ss->isASCII) return pos;
		unsigned char *c = (unsigned char*)ss->data;
		unsigned char *maxc = c + ss->dataSize;
		AdvanceUTF8(&c, maxc, (int)pos);
		return (size_t)(c - (unsigned char*)ss->data);
	}
	
	size_t String::charPosOfBytePos(size_t posB) const {
		if (posB <= 0 || !ss) return 0;
		if (ss->charCount < 0) ss->analyzeChars();
		if (posB > ss->dataSize) return ss->charCount;
		if (ss->isASCII) return posB;
		int count = 0;
		for (size_t i=0; i<posB; i++) {
			if (!IsUTF8IntraChar(ss->data[i])) count++;
		}
		return count;
	}

	String String::Substring(long pos, long numChars) const {
		if (!ss) return *this;
		long posB = bytePosOfCharPos(pos);	// (also ensures ss->isASCII is known)
		if (ss->isASCII) return SubstringB(pos, numChars);
		unsigned char *startPtr = (unsigned char*)ss->data + posB;
		unsigned char *endPtr = startPtr;
		unsigned char *max = (unsigned char*)ss->data + ss->dataSize;
		if (numChars < 0) endPtr = max;
		else AdvanceUTF8(&endPtr, max, (int)numChars);
		return SubstringB(posB, endPtr - startPtr);
	}
	
	bool String::StartsWith(const String& s) const {
		if (s.empty()) return true;
		long byteCount = s.ss->dataSize - 1;  // (ignoring terminating null)
		if (!ss or ss->dataSize < byteCount) return false;
		char *p1 = ss->data;
		char *p2 = s.ss->data;
		for (int i=0; i<byteCount; i++) if (*p1++ != *p2++) return false;
		return true;
	}
	
	bool String::EndsWith(const String& s) const {
		if (s.empty()) return true;
		long byteCount = s.ss->dataSize - 1;  // (ignoring terminating null)
		if (!ss or ss->dataSize < byteCount) return false;
		char *p1 = ss->data + ss->dataSize - byteCount - 1;
		char *p2 = s.ss->data;
		for (int i=0; i<byteCount; i++) if (*p1++ != *p2++) return false;
		return true;
	}

	String String::Format(int num, const char* formatSpec) {
		char buf[32];
		snprintf(buf, 32, formatSpec, num);
		return buf;
	}

	String String::Format(long num, const char* formatSpec) {
		char buf[32];
		snprintf(buf, 32, formatSpec, num);
		return buf;
	}

	String String::Format(float num, const char* formatSpec) {
		char buf[32];
		snprintf(buf, 32, formatSpec, num);
		return buf;
	}

	String String::Format(double num, const char* formatSpec) {
		char buf[32];
		snprintf(buf, 32, formatSpec, num);
		return buf;
	}

	String String::Format(bool value, const char* trueString, const char* falseString) {
		return value ? trueString : falseString;
	}

	int String::IntValue(const char* formatSpec) const {
		int retval = 0;
		sscanf(c_str(), formatSpec, &retval);
		if (retval == 0 and BooleanValue()) return 1;
		return retval;
	}

	long String::LongValue(const char* formatSpec) const {
		long retval = 0;
		sscanf(c_str(), formatSpec, &retval);
		if (retval == 0 and BooleanValue()) return 1;
		return retval;
	}

	float String::FloatValue(const char* formatSpec) const {
		float retval = 0;
		sscanf(c_str(), formatSpec, &retval);
		if (retval == 0 and BooleanValue()) return 1;
		return retval;
	}

	double String::DoubleValue(const char* formatSpec) const {
		double retval = 0;
		sscanf(c_str(), formatSpec, &retval);
		if (retval == 0 and BooleanValue()) return 1;
		return retval;
	}

	bool String::BooleanValue() const {
		String lower = this->ToLower();
		if (0 == lower.Compare("true") or 0 == lower.Compare("yes")
			or 0 == lower.Compare("t") or 0 == lower.Compare("y")) {
			return true;
		}
		
		// Check if the String has a float value but don't call FloatValue() since it calls this method
		float floatVal = 0;
		sscanf(c_str(), "%f", &floatVal);
		return (fabs(floatVal) > 0.0001);
	}

	// Comparision between cstrings to strings
	bool operator==(const char *cstring, const String &str) {
		return str == cstring;
	}
	bool operator!=(const char *cstring, const String &str) {
		return str != cstring;
	}
	bool operator<(const char *cstring, const String &str) {
		return str > cstring;
	}
	bool operator<=(const char *cstring, const String &str) {
		return str >= cstring;
	}
	bool operator>(const char *cstring, const String &str) {
		return str < cstring;
	}
	bool operator>=(const char *cstring, const String &str) {
		return str <= cstring;
	}

	// UTF-8-savvy character-oriented code
	void StringStorage::analyzeChars() {
		charCount = 0;
		isASCII = true;
		for (const unsigned char *c = (const unsigned char*)data; *c; c++) {
			if (IsUTF8IntraChar(*c)) isASCII = false;
			else charCount++;
		}
	}


	//--------------------------------------------------------------------------------
	// Unit Tests

	class TestString : public UnitTest
	{
	public:
		TestString() : UnitTest("String") {}
		virtual void Run();
	};
	
	void TestString::Run()
	{
		String foo("foo");
		String bar("barber");
		
		Assert(foo != bar);
		Assert(bar < foo);
		Assert(foo > bar);
		
		Assert(foo.IndexOfB("f") == 0);
		Assert(foo.IndexOfB("o", 1) == 1);
		Assert(foo.IndexOfB("b") == -1);

		Assert(foo.LastIndexOfB("o") == 2);
		Assert(bar.LastIndexOfB("b") == 3);
		Assert(foo.LastIndexOfB("b", 2) == -1);
		Assert(bar.LastIndexOfB("b", 2) == 0);
		
		foo.Append(bar);
		Assert(foo == "foobarber");

		Assert(bar.at(0) == 'b');
		
		String s;
		s = String::Format(17);
		Assert(s == "17");
		Assert(s.IntValue() == 17);
		
		
		s = String::Format(17L);
		Assert(s == "17");
		Assert(s.LongValue() == 17L);
		
		s = String::Format(1.7);
		Assert(s == "1.7");
		Assert(s.DoubleValue() == 1.7);
		
		s = String::Format(1.7f);
		Assert(s == "1.7");
		Assert(s.FloatValue() == 1.7f);
		
		s = String::Format(17, "%4x");
		Assert(s == "  11");
		Assert(s.IntValue("%4x") == 17);
		
		String empty;
		s = "foo";
		Assert(s > empty);
		Assert(empty < s);
		// Test null and empty String are equal
		Assert(empty == NULL);
		Assert(empty == "");
		
		
		s = "      words surrounded by spaces     ";
		Assert(s.TrimEnd() == "      words surrounded by spaces");
		Assert(s.TrimStart() == "words surrounded by spaces     ");
		Assert(s.Trim() == "words surrounded by spaces");
		
		
		s = "88888words surrounded by eights88888";
		Assert(s.TrimEnd('8') == "88888words surrounded by eights");
		Assert(s.TrimStart('8') == "words surrounded by eights88888");
		Assert(s.Trim('8') == "words surrounded by eights");
		
		Assert(empty.Trim() == empty);
		
		
		s = "no spaces to remove";
		Assert( s.Trim() == "no spaces to remove");
		
		s = "One";
		String Trimmed = s.Trim();
		Assert( Trimmed == s);
		
		s = s.SubstringB(1);
		Assert( s == "ne" );
		Assert( s.LengthB() == 2 );
		
		s = "this is a test String.";
		s = s.ReplaceB(9, 5, "n example");
		Assert(s == "this is an example String.");
		
		s = s.Replace("is", "at");
		Assert(s == "that at an example String.");
		
		s = "foobarbazaroo";
		s = s.Replace("oo", "oooo");	// scary, but should be safe!
		Assert(s == "foooobarbazaroooo");
		
		s = "another simple String";
		char *str = new char[22];
		strcpy(str,"an array of characters");
		s.takeoverBuffer(str);
		Assert(s == "an array of characters");
		Assert(s.Length() == 22);
		Assert(s.Contains("a"));
		Assert(!s.Contains("z"));
		Assert(s.Contains("of"));
		Assert(s.Contains("character"));
		Assert(!s.Contains("hippo"));
		Assert(s.at(7) == 'y');
		Assert(s.StartsWith("an"));
		Assert(s.EndsWith("ters"));
		Assert(not s.StartsWith("foo"));
		Assert(not s.EndsWith("bar"));
		
		// There's bytes, and there's characters.  They're different.
		s = "日本語";
		Assert(s.LengthB() == 9);
		Assert(s.Length() == 3);
		Assert(s[0] == 230);
		Assert(s.atB(0) == -26);
		Assert(s.at(0) == 0x65E5);
		Assert(s.at(2) == 0x8A9E);
		Assert(s.charPosOfBytePos(0) == 0);
		Assert(s.charPosOfBytePos(3) == 1);
		Assert(s.charPosOfBytePos(6) == 2);
		Assert(s.bytePosOfCharPos(0) == 0);
		Assert(s.bytePosOfCharPos(1) == 3);
		Assert(s.bytePosOfCharPos(2) == 6);
		Assert(s.SubstringB(0, 6) == "日本");
		Assert(s.Substring(0, 2) == "日本");
		Assert(s.Substring(1) == "本語");
		Assert(s.Substring(2) == "語");
		Assert(s.IndexOfB("本語", 1) == 3);
		Assert(s.IndexOfB("本語", 4) == -1);
		Assert(s.IndexOf("本語", 1) == 1);
		Assert(s.IndexOf("本語", 2) == -1);
		Assert(s.StartsWith("日"));
		Assert(not s.StartsWith("本語"));
		Assert(s.EndsWith("本語"));
		Assert(not s.EndsWith("本"));
	}

	RegisterUnitTest(TestString);
}

