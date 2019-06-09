//
//  Dictionary.cpp
//  MiniScript
//
//  Created by Joe Strout on 3/14/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "Dictionary.h"
#include "UnitTest.h"
#include "String.h"

namespace MiniScript {
	
	class TestDictionary : public UnitTest
	{
	public:
		TestDictionary() : UnitTest("Dictionary") {}
		virtual void Run();
	private:
		void RunInnerTests();
	};

	void TestDictionary::Run() {
#if(DEBUG)
		// Make sure that we're not leaking any storage from running our tests.
		long prevCount = RefCountedStorage::instanceCount;
#endif
		RunInnerTests();
#if(DEBUG)
		long postCount = RefCountedStorage::instanceCount;
		Assert(prevCount == postCount);
#endif
	}
	void TestDictionary::RunInnerTests()
	{
		Dictionary<String, int, hashString> d;
		d.SetValue("one", 1);
		d.SetValue("two", 2);
		Assert(d.Count() == 2);
		Assert(d.ContainsKey("one"));
		Assert(d.ContainsKey("two"));
		Assert(not d.ContainsKey("nosuch"));
		Assert(d.Lookup("one", 0) == 1);
		Assert(d.Lookup("two", 0) == 2);
		Assert(d.Lookup("nosuch", 0) == 0);

		d.Remove("one");
		Assert(d.Count() == 1);
		Assert(not d.ContainsKey("one"));
		Assert(d.ContainsKey("two"));
		Assert(d.Lookup("one", 0) == 0);
		Assert(d.Lookup("two", 0) == 2);

		d.RemoveAll();
		Assert(d.Count() == 0);
		Assert(not d.ContainsKey("one"));
		Assert(not d.ContainsKey("two"));

		Dictionary<int, int, hashInt> d2;
		for (int i=0; i<1000; i++) {
			d2.SetValue(i, i*i);
		}
		Assert(d2.Count() == 1000);
		for (int i=0; i<1000; i++) {
			Assert(d2.Lookup(i, -1) == i*i);
		}

	}

	RegisterUnitTest(TestDictionary);

}


