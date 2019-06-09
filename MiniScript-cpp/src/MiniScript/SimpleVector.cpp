/*
 *  SimpleVector.cpp
 *  drawgame
 *
 *  Created by Joe Strout on 12/23/10.
 *  Copyright 2010 Luminary Apps. All rights reserved.
 *
 */

#include "SimpleVector.h"
#include "UnitTest.h"

namespace MiniScript {
	

	class TestSimpleVector : public UnitTest
	{
	public:
		TestSimpleVector() : UnitTest("SimpleVector") {}
		virtual void Run();

	private:

		inline void check(SimpleVector<int> list) {
			Assert( list.size() == 0 );
		}

		inline void check(SimpleVector<int> list, int a) {
			Assert( list.size() == 1
				   and list[0]==a );
		}

		inline void check(SimpleVector<int> list, int a, int b) {
			Assert( list.size() == 2
				   and list[0]==a and list[1]==b );
		}

		inline void check(SimpleVector<int> list, int a, int b, int c) {
			Assert( list.size() == 3
				   and list[0]==a and list[1]==b and list[2]==c );
		}

		inline void check(SimpleVector<int> list, int a, int b, int c, int d) {
			Assert( list.size() == 4
				   and list[0]==a and list[1]==b and list[2]==c and list[3]==d );
		}

		inline void check(SimpleVector<int> list, int a, int b, int c, int d, int e) {
			Assert( list.size() == 5
				   and list[0]==a and list[1]==b and list[2]==c and list[3]==d and list[4]==e );
		}
		
	};

	void TestSimpleVector::Run()
	{
		
		SimpleVector<int> list;
		check(list);
		
		list.push_back(0);
		check(list, 0);
		
		list.push_back(1);
		check(list, 0, 1);
		
		SimpleVector<int> list2 = list;
		check(list2, 0, 1);

		list2.insert(42, 0);
		check(list2, 42, 0, 1);
		
		list2 = list;
		list2.insert(42, 1);
		check(list2, 0, 42, 1);
		
		list2 = list;
		list2.insert(42, 2);
		check(list2, 0, 1, 42);

		list2.reposition(2, 1);
		check(list2, 0, 42, 1);
		
		list2.reposition(0, 2);
		check(list2, 42, 1, 0);
		
		list2.reposition(2, 0);
		check(list2, 0, 42, 1);

		list2 = list;
		list2.insert(42, 0);
		list2.push_back(4);
		list2.reposition(1, 2);
		check(list2, 42, 1, 0, 4);
		Assert(list2.peek_back() == 4);
		Assert(list2.pop_back() == 4);
		check(list2, 42, 1, 0);
		
		list2.resizeBuffer(13);
		Assert(list2.size() == 3);
		list2.resize(13);
		Assert(list2.size() == 13);
		list2.resizeBuffer(4);
		Assert(list2.size() == 4);
		list2.pop_back();
		
		check(list2, 42, 1, 0);
		Assert(list2.item(0) == 42);
		Assert(list2.item(6) == 42);
		Assert(list2.item(1) == 1);
		Assert(list2.item(-1) == 0);
		Assert(list2.item(-2) == 1);
		Assert(list2.item(-3) == 42);
		Assert(list2.item(-4) == 0);
		
		Assert(list2.indexOf(42) == 0);
		Assert(list2.Contains(42));

		Assert(list2.indexOf(0) == 2);
		Assert(list2.Contains(0));
		
		Assert(list2.indexOf(17) == -1);
		Assert(!list2.Contains(17));
		
		SimpleVector<int> list3;
		list3.push_back(0);
		list3.push_back(1);
		list3.push_back(2);
		list3.push_back(3);
		list3.reverse();
		check(list3, 3, 2, 1, 0);
		list3.push_back(4);
		list3.reverse();
		check(list3, 4, 0, 1, 2, 3);		
	}

	RegisterUnitTest(TestSimpleVector);
}
