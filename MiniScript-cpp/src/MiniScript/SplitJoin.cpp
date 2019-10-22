/*
 *  SplitJoin.cpp
 *  drawgame
 *
 *  Created by Joe Strout on 12/21/10.
 *  Copyright 2010 Luminary Apps. All rights reserved.
 *
 */

#include "SplitJoin.h"
#include "UnitTest.h"

namespace MiniScript {

	#pragma mark -
	#pragma mark Splits

	StringList Split(const String& s, const String& delimiter, int maxSplits) {
		StringList out;
		long posB = 0;
		int splitCount = 0;
		while (1) {
			long endPosB = s.IndexOfB(delimiter, posB);
			if (endPosB < 0) {
				// The delimiter wasn't found, so push the rest of the String.
				out.Add(s.SubstringB(posB));
				break;
			}
			// The delimiter was found, so push everything from the last start to the delimiter pos.
			out.Add(s.SubstringB(posB, endPosB-posB));
			// Start the next search at the found position + the delimiter length
			posB = endPosB + delimiter.Length();
			splitCount++;
			// We've hit or exceeded the max number of splits, so push the last bit of the String.
			if (maxSplits > 0 and splitCount >= maxSplits) {
				out.Add(s.SubstringB(posB));
				break;
			}
		}
		return out;
	}

	StringList Split(const String& s, char delimiter, int maxSplits) {
		String delim(&delimiter, 1);
		return Split(s, delim, maxSplits);
	}


	#pragma mark -
	#pragma mark Joins

	String Join(const String& delimiter, const StringList& parts) {
		String out;
		
		// Take care of empty and single strings first
		if (1 > parts.Count()) {
			return out;
		} else if (1 == parts.Count()) {
			out = parts[0];
			return out;
		}
		
		// Calculate the length of the new String
		unsigned long LengthSum = delimiter.LengthB() * (parts.Count()-1) +1; // +1 for the null char
		for (int i=0; i < parts.Count(); i++) {
			LengthSum += parts[i].LengthB();
		}
		
		// Create a new buffer for it
		char *buffer = new char[LengthSum];
		
		// Copy each String into the buffer delimited by the passed in String
		char *dest = buffer;
		for (int i=0; i < parts.Count(); i++) {
			strncpy(dest, parts[i].c_str(), parts[i].LengthB());
			dest += parts[i].LengthB();
			
			if (delimiter.LengthB() > 0 and i < parts.Count()-1) {
				strncpy(dest, delimiter.c_str(), delimiter.LengthB());
				dest += delimiter.LengthB();
			}
		}
		Assert(dest - buffer == LengthSum - 1);
		*dest = 0;  // add null termination byte
		
		// Give the buffer to the String, it is now responsible for freeing the memory
		out.takeoverBuffer(buffer);
		
		return out;
	}

	String join(char delimiter, const StringList& parts) {
		String delim(&delimiter, 1);
		return Join(delim, parts);
	}


	#pragma mark -

	class TestSplitJoin : public UnitTest
	{
	public:
		TestSplitJoin() : UnitTest("SplitJoin") {}
		virtual void Run();
	};
	
	void TestSplitJoin::Run()
	{
		String s1("foo bar baz bamf");
		StringList list = Split(s1);
		Assert(list.Count() == 4 and list[0] == "foo" and list[1] == "bar"
			   and list[2] == "baz" and list[3] == "bamf");
		Assert(list[2].Length() == 3);  // "baz"
		Assert(list[3].Length() == 4);  // "bamf"
		
		list = Split(s1, ' ', 1);
		Assert(list.Count() == 2 and list[0] == "foo" and list[1] == "bar baz bamf");
		
		list = Split(s1, "ba");
		Assert(list.Count() == 4 and list[0] == "foo " and list[1] == "r "
			   and list[2] == "z " and list[3] == "mf");
		
		StringList list2;
		list2.Add("str1");
		list2.Add("str2");
		list2.Add("str3");
		
		String res = Join(" ", list2);
		Assert(res == "str1 str2 str3");
		
		String s2("日本 楽しみ");
		list = Split(s2);
		Assert(list.Count() == 2 and list[0] == "日本" and list[1] == "楽しみ");
		res = Join(" ", list);
		Assert(res == s2);
	}

	RegisterUnitTest(TestSplitJoin);

}

