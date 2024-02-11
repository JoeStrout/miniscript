cmake_minimum_required(VERSION 3.12)
project(Miniscript VERSION 1.6.2 LANGUAGES C CXX)

option(MINISCRIPT_BUILD_TESTING "Build unit test executable" OFF)
option(MINISCRIPT_BUILD_CSHARP "Build CSharp binaries" OFF)
set(MINISCRIPT_CMD_NAME "miniscript" CACHE STRING
	"Specifies the command-line MiniScript executable filename")

if(MINISCRIPT_BUILD_CSHARP)
	enable_language(CSharp)
	set(CMAKE_CSharp_FLAGS "/langversion:7.3")
	set(CMAKE_DOTNET_TARGET_FRAMEWORK "net45")
	set(CMAKE_DOTNET_TARGET_FRAMEWORK_VERSION "v4.5")
	set(DOTNET_REFS "System;System.Core")
	add_library(miniscript-cs SHARED
		MiniScript-cs/Miniscript.cs
		MiniScript-cs/MiniscriptErrors.cs
		MiniScript-cs/MiniscriptInterpreter.cs
		MiniScript-cs/MiniscriptIntrinsics.cs
		MiniScript-cs/MiniscriptKeywords.cs
		MiniScript-cs/MiniscriptLexer.cs
		MiniScript-cs/MiniscriptParser.cs
		MiniScript-cs/MiniscriptTAC.cs
		MiniScript-cs/MiniscriptTypes.cs
		MiniScript-cs/MiniscriptUnitTest.cs
	)
	set_target_properties(miniscript-cs PROPERTIES
		VS_GLOBAL_ROOTNAMESPACE "Miniscript"
		VS_DOTNET_REFERENCES "${DOTNET_REFS}"
	)
endif()

set(MINISCRIPT_HEADERS
	MiniScript-cpp/src/MiniScript/Dictionary.h
	MiniScript-cpp/src/MiniScript/List.h
	MiniScript-cpp/src/MiniScript/MiniscriptErrors.h
	MiniScript-cpp/src/MiniScript/MiniscriptInterpreter.h
	MiniScript-cpp/src/MiniScript/MiniscriptIntrinsics.h
	MiniScript-cpp/src/MiniScript/MiniscriptKeywords.h
	MiniScript-cpp/src/MiniScript/MiniscriptLexer.h
	MiniScript-cpp/src/MiniScript/MiniscriptParser.h
	MiniScript-cpp/src/MiniScript/MiniscriptTAC.h
	MiniScript-cpp/src/MiniScript/MiniscriptTypes.h
	MiniScript-cpp/src/MiniScript/QA.h
	MiniScript-cpp/src/MiniScript/RefCountedStorage.h
	MiniScript-cpp/src/MiniScript/SimpleString.h
	MiniScript-cpp/src/MiniScript/SimpleVector.h
	MiniScript-cpp/src/MiniScript/SplitJoin.h
	MiniScript-cpp/src/MiniScript/UnicodeUtil.h
	MiniScript-cpp/src/MiniScript/UnitTest.h
)

set(MINICMD_HEADERS
	MiniScript-cpp/src/DateTimeUtils.h
	MiniScript-cpp/src/OstreamSupport.h
	MiniScript-cpp/src/ShellExec.h
	MiniScript-cpp/src/ShellIntrinsics.h
	MiniScript-cpp/src/editline/editline.h
	MiniScript-cpp/src/editline/unix.h
	MiniScript-cpp/src/whereami/whereami.h
)

add_library(miniscript-cpp
	MiniScript-cpp/src/MiniScript/Dictionary.cpp
	MiniScript-cpp/src/MiniScript/List.cpp
	MiniScript-cpp/src/MiniScript/MiniscriptInterpreter.cpp
	MiniScript-cpp/src/MiniScript/MiniscriptIntrinsics.cpp
	MiniScript-cpp/src/MiniScript/MiniscriptKeywords.cpp
	MiniScript-cpp/src/MiniScript/MiniscriptLexer.cpp
	MiniScript-cpp/src/MiniScript/MiniscriptParser.cpp
	MiniScript-cpp/src/MiniScript/MiniscriptTAC.cpp
	MiniScript-cpp/src/MiniScript/MiniscriptTypes.cpp
	MiniScript-cpp/src/MiniScript/QA.cpp
	MiniScript-cpp/src/MiniScript/SimpleString.cpp
	MiniScript-cpp/src/MiniScript/SimpleVector.cpp
	MiniScript-cpp/src/MiniScript/SplitJoin.cpp
	MiniScript-cpp/src/MiniScript/UnicodeUtil.cpp
	MiniScript-cpp/src/MiniScript/UnitTest.cpp
	${MINISCRIPT_HEADERS}
)

target_include_directories(miniscript-cpp PUBLIC MiniScript-cpp/src/MiniScript)

if(NOT WIN32)
	set(EDITLINE_SRC
		MiniScript-cpp/src/editline/complete.c
		MiniScript-cpp/src/editline/editline.c
		MiniScript-cpp/src/editline/sysunix.c
	)
endif()

add_executable(minicmd
	MiniScript-cpp/src/main.cpp
	MiniScript-cpp/src/DateTimeUtils.cpp
	MiniScript-cpp/src/OstreamSupport.cpp
	MiniScript-cpp/src/ShellIntrinsics.cpp
	MiniScript-cpp/src/ShellExec.cpp
	MiniScript-cpp/src/whereami/whereami.c
	${EDITLINE_SRC}
	${MINICMD_HEADERS}
)
target_include_directories(minicmd PRIVATE MiniScript-cpp/src/editline)
target_link_libraries(minicmd PRIVATE miniscript-cpp)

set_target_properties(miniscript-cpp minicmd PROPERTIES
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON)
set_target_properties(minicmd PROPERTIES OUTPUT_NAME ${MINISCRIPT_CMD_NAME})

if(MINISCRIPT_BUILD_TESTING)
	enable_testing()
	add_custom_target(TestSuite SOURCES TestSuite.txt)
	add_executable(tests-cpp MiniScript-cpp/src/MiniScript/UnitTest.cpp)
	target_compile_definitions(tests-cpp PRIVATE UNIT_TEST_MAIN)
	target_link_libraries(tests-cpp PRIVATE miniscript-cpp)
	add_test(NAME Miniscript.cpp.UnitTests COMMAND tests-cpp)
	add_test(NAME Miniscript.cpp.Integration COMMAND minicmd --itest ${CMAKE_SOURCE_DIR}/TestSuite.txt)
	set_tests_properties(Miniscript.cpp.UnitTests Miniscript.cpp.Integration PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL|Error")
	if(MINISCRIPT_BUILD_CSHARP)
		add_executable(tests-cs MiniScript-cs/Program.cs)
		target_link_libraries(tests-cs PRIVATE miniscript-cs)
		set_target_properties(tests-cs PROPERTIES
			VS_GLOBAL_ROOTNAMESPACE "Miniscript"
			VS_DOTNET_REFERENCES "${DOTNET_REFS}"
			CXX_STANDARD 14
			CXX_STANDARD_REQUIRED ON
		)
		add_test(NAME Miniscript.cs.UnitTests COMMAND tests-cs --test)
		add_test(NAME Miniscript.cs.Integration COMMAND tests-cs --test --integration ${CMAKE_SOURCE_DIR}/TestSuite.txt)
		set_tests_properties(Miniscript.cs.UnitTests Miniscript.cs.Integration PROPERTIES FAIL_REGULAR_EXPRESSION "FAIL|Error")
	endif()
endif()

install(TARGETS miniscript-cpp minicmd)
