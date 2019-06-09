// UnitTest.h
//
//	This is REAL Software's implementation of an xUnit-like testing framework.
//
//	To use:
//		1. Define a class derived from UnitTest.  Implement at least Run().
//		   Don't allocate memory or do much of anything inside your constructor;
//		   use SetUp and TearDown instead.
//		2. Inside Run(), use Error, ErrorIf, and Assert to find/report errors.
//		3. Register your test case by putting this line in C-file scope:
//			RegisterUnitTest(MyCustomUnitTest);
//
//	That's it!  Your test will be automatically added to a list of test cases,
//	run when RunAllTests() is called, and destroyed upon DeleteAllTests().
//
// (c) 2013 Xojo, Inc. -- All Rights Reserved
// (Based on code owned by Joe Strout -- used with permission.)
//
// Document Scope: hopefully all projects, targets, & platforms
// Document Author: Joe Strout
// Document Contributor(s): 
//
// Revision History
//	Jul 06 2001 -- JJS (1) Changed RegisterTest to statically allocate test objects;
//							this makes them compatible with memory management in the IDE
//							by avoiding dynamic allocation during the initialization phase.

#ifndef UNITTEST_H
#define UNITTEST_H

#include "QA.h"

namespace MiniScript {
	

	class UnitTest
	{
	  public:
		UnitTest(const char* testName) : name(testName), mNext(0) {}
		
		// Method which all test cases must override:
		virtual void Run()=0;
		
		// Methods which some test cases may want to override:
		virtual void SetUp() {}
		virtual void TearDown() {}

		// Call this to register a test:
		static void RegisterTestCase(UnitTest *test);
		
		// Static methods to run tests, delete tests:
		static void RunAllTests();
		static void DeleteAllTests();

		const char *name;

	  protected:
		// Methods which test cases may use to report errors (via the macros below):

	  private:
		UnitTest *mNext;
		
		static UnitTest *cTests;
	};

	class UnitTestRegistrar
	{
	  public:
		UnitTestRegistrar(UnitTest *test) { UnitTest::RegisterTestCase(test); }
	};

}

// Macro to register a unit test class
#define RegisterUnitTest(testclass) static testclass _inst##testclass; \
									UnitTestRegistrar _reg##testclass(&_inst##testclass)

#endif
