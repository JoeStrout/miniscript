//
//  MSList.cpp
//  MiniScript
//
//  Created by Joe Strout on 3/11/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "List.h"
#include "UnitTest.h"

namespace MiniScript {
	
	class TestList : public UnitTest
	{
	public:
		TestList() : UnitTest("List") {}
		virtual void Run();
		
	private:
		
		inline void check(const List<int>& list) {
			Assert( list.Count() == 0 );
		}
		
		inline void check(const List<int>& list, int a) {
			Assert( list.Count() == 1
				   and list[0]==a );
		}
		
		inline void check(const List<int>& list, int a, int b) {
			Assert( list.Count() == 2
				   and list[0]==a and list[1]==b );
		}
		
		inline void check(const List<int>& list, int a, int b, int c) {
			Assert( list.Count() == 3
				   and list[0]==a and list[1]==b and list[2]==c );
		}
		
		inline void check(const List<int>& list, int a, int b, int c, int d) {
			Assert( list.Count() == 4
				   and list[0]==a and list[1]==b and list[2]==c and list[3]==d );
		}
		
		inline void check(const List<int>& list, int a, int b, int c, int d, int e) {
			Assert( list.Count() == 5
				   and list[0]==a and list[1]==b and list[2]==c and list[3]==d and list[4]==e );
		}
		
	};
	
	void TestList::Run() {
		List<int> list;
		check(list);
		
		list.Add(0);
		check(list, 0);
		
		list.Add(1);
		check(list, 0, 1);
		
		List<int> list2 = list;
		check(list2, 0, 1);
		
		list2.Insert(42, 0);
		check(list2, 42, 0, 1);
		
		list2.RemoveAt(0);
		list2.Add(42);
		check(list2, 0, 1, 42);
		
		list2.Reposition(2, 1);
		check(list2, 0, 42, 1);
		
		list2.Reposition(0, 2);
		check(list2, 42, 1, 0);
		
		list2.Reposition(2, 0);
		check(list2, 0, 42, 1);
		
		list2.RemoveAt(1);
		list2.Insert(42, 0);
		list2.Add(4);
		list2.Reposition(1, 2);
		check(list2, 42, 1, 0, 4);
		Assert(list2.Last() == 4);
		Assert(list2.Pop() == 4);
		check(list2, 42, 1, 0);
		
		list2.ResizeBuffer(13);
		Assert(list2.Count() == 3);
		list2.Resize(13);
		Assert(list2.Count() == 13);
		list2.ResizeBuffer(4);
		Assert(list2.Count() == 4);
		list2.Pop();
		
		check(list2, 42, 1, 0);
		Assert(list2.Item(0) == 42);
		Assert(list2.Item(6) == 42);
		Assert(list2.Item(1) == 1);
		Assert(list2.Item(-1) == 0);
		Assert(list2.Item(-2) == 1);
		Assert(list2.Item(-3) == 42);
		Assert(list2.Item(-4) == 0);
		
		Assert(list2.IndexOf(42) == 0);
		Assert(list2.Contains(42));
		
		Assert(list2.IndexOf(0) == 2);
		Assert(list2.Contains(0));
		
		Assert(list2.IndexOf(17) == -1);
		Assert(!list2.Contains(17));
		
		List<int> list3;
		list3.Add(0);
		list3.Add(1);
		list3.Add(2);
		list3.Add(3);
		list3.Reverse();
		check(list3, 3, 2, 1, 0);
		list3.Add(4);
		list3.Reverse();
		check(list3, 4, 0, 1, 2, 3);
		list3.RemoveRange(1, 3);
		check(list3, 4, 3);
	}
	
	RegisterUnitTest(TestList);
}

