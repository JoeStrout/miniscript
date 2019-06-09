// UnitTest.cpp
//
//	Implements the basic unit testing framework.  See UnitTest.h
//	for usage details.
//
// (c) 2013 Xojo, Inc. -- All Rights Reserved
// (Based on code owned by Joe Strout -- used with permission.)
//
// Document Scope: hopefully all projects, targets, & platforms
// Document Author: Joe Strout
// Document Contributor(s): 
//
// Revision History
//	Jul 06 2001 -- JJS (1) Changed DeleteAllTests to not delete the test objects,
//							since they are now static objects (not deletable).

#include "UnitTest.h"
#include "QA.h"
#include <stdio.h>
#include <iostream>
#include "String.h"

namespace MiniScript {
	
	UnitTest *UnitTest::cTests = 0;

	// UnitTest::RegisterUnitTest
	//
	//	Add the given test case to our global list of test cases.
	//
	// Author: JJS
	// Used in: TestCaseRegistrar constructor
	// Gets: test -- test case to register
	// Returns: <nothing>
	// Comment: Jul 05 2001 -- JJS (1)
	void UnitTest::RegisterTestCase(UnitTest *test)
	{
		if (cTests) {
			test->mNext = cTests;
		}
		cTests = test;
	}

	// UnitTest::RunAllTests
	//
	//	Run all tests in our global list of tests.
	//
	// Author: JJS
	// Used in:
	// Gets: <nothing>
	// Returns: <nothing>
	// Comment: Jul 05 2001 -- JJS (1)
	void UnitTest::RunAllTests()
	{
		for (UnitTest *test = cTests; test; test = test->mNext) {
//			std::cout << "Running " << test->name << std::endl;
			test->SetUp();
			test->Run();
			test->TearDown();
//			std::cout << "StringStorage instances left: " << StringStorage::instanceCount << std::endl;
//			std::cout << "total RefCountedStorage instances left: " << RefCountedStorage::instanceCount << std::endl;
		}
	}

	// UnitTest::DeleteAllTests
	//
	//	This method is intended to delete all the tests from the global
	//	list of tests.  However, since tests are now declared as static
	//	globals, we can't do that.  So this just empties our list
	//	(without deleting the actual test objects).
	//
	// Author: JJS
	// Used in:
	// Gets: <nothing>
	// Returns: <nothing>
	// Comment: Jul 05 2001 -- JJS (1)
	void UnitTest::DeleteAllTests()
	{
		UnitTest *next;
		for (UnitTest *test = cTests; test; test = next) {
			next = test->mNext;
			// Jul 06 2001 -- JJS (1) -- commented out:
			//delete test;
		}
		cTests = 0;
	}

	//--------------------------------------------------------------------------------
	// MARK: -

	// class TestCaseItself
	//
	//	Here's a test case to test UnitTest itself.
	//
	class TestCaseItself : public UnitTest
	{
	public:
		TestCaseItself() : UnitTest("TestCaseItself") {}
		virtual void Run();
	};

	void TestCaseItself::Run()
	{
		ErrorIf(1==0);
		Assert(2+2==4);
		// NOTE: can't really test the actual error reporting, because if
		// it works it'd look like an error!
	}

	RegisterUnitTest(TestCaseItself);

}

