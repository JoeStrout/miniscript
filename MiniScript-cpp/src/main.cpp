//
//  main.cpp
//  MiniScript
//
//  Created by Joe Strout on 3/9/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include "MiniScript/SimpleString.h"
#include "MiniScript/UnicodeUtil.h"
#include "MiniScript/UnitTest.h"
#include "MiniScript/SimpleVector.h"
#include "MiniScript/List.h"
#include "MiniScript/Dictionary.h"
#include "MiniScript/MiniscriptParser.h"
#include "MiniScript/MiniscriptInterpreter.h"
#include "OstreamSupport.h"
#include "MiniScript/SplitJoin.h"
#include "ShellIntrinsics.h"
#include "DateTimeUtils.h"	// TEMP for initial testing

// YIELD_NANOSECONDS: How many nano-seconds to sleep when yielding.
#define YIELD_NANOSECONDS 10000000

using namespace MiniScript;

bool printHeaderInfo = true;

static bool dumpTAC = false;

static void Print(String s, bool lineBreak=true) {
	std::cout << s.c_str();
	if (lineBreak) std::cout << std::endl; else std::cout << std::flush;
}

static void PrintErr(String s, bool lineBreak=true) {
	std::cerr << s.c_str();
	if (lineBreak) std::cerr << std::endl;
}

static int ReturnErr(String s, int errCode = -1) {
	std::cerr << s.c_str() << std::endl;
	return errCode;
}

static void PrintHeaderInfo() {
	if (!printHeaderInfo) return;
	std::cout << "MiniScript " << std::endl
	<< MiniScript::hostName << " v" << MiniScript::hostVersion
	<< "; language v" << VERSION;
#if(DEBUG)
	std::cout << " (DEBUG)";
#endif
//	std::cout << std::endl;
	std::cout <<  " (" << __DATE__ << ")" << std::endl;
	printHeaderInfo = false;	// (print this stuff only once)
}

static void PrintHelp(String cmdPath) {
	Print(String("usage: ") + cmdPath + " [option] ... [-c cmd | file | -]");
	Print("Options and arguments:");
	Print("-c cmd : program passed in as String (terminates option list)");
	Print("--dumpTAC : print intermediate code");
	Print("-h     : print this help message and exit (also -? or --help)");
	Print("-i     : enter interactive mode after executing 'file'");
	Print("--itest suite_file : run integration tests");
	Print("-q     : suppress header info");
	Print("file   : program read from script file");
	Print("-      : program read from stdin (default; interactive mode if a tty)");
}

void ConfigInterpreter(Interpreter &interp) {
	interp.standardOutput = &Print;
	interp.errorOutput = &PrintErr;
	interp.implicitOutput = &Print;
}

static int DoREPL(Interpreter &interp) {
	while (true) {
		const char *prompt;
		if (interp.NeedMoreInput()) {
			prompt = ">>> ";
		} else {
			prompt = "> ";
			std::cout << std::flush;
		}
		
		try {
			#if useEditline
				char *buf;
				buf = readline(prompt);
				if (buf == nullptr) return 0;
				interp.REPL(buf);
				free(buf);
			#else
				// Standard C++ I/O:
				char buf[1024];
				std::cout << prompt;
				if (not std::cin.getline(buf, sizeof(buf))) {
					std::cout << std::endl;
					return 0;
				}
				interp.REPL(buf);
			#endif
		} catch (MiniscriptException& mse) {
			std::cerr << "Runtime Exception: " << mse.message << std::endl;
			interp.vm->Stop();
		}
		
		if (exitASAP) return exitResult;
	}
}

static int DoCommand(Interpreter &interp, String cmd) {
	interp.Reset(cmd);
	interp.Compile();
	
//	std::cout << cmd << std::endl;
	
	if (dumpTAC) {
		Context *c = interp.vm->GetGlobalContext();
		for (long i=0; i<c->code.Count(); i++) {
			std::cout << i << ". " << c->code[i].ToString() << std::endl;
		}
	}
	
	while (!interp.Done()) {
		try {
			interp.RunUntilDone();
			std::this_thread::sleep_for(std::chrono::nanoseconds(YIELD_NANOSECONDS));
		} catch (MiniscriptException& mse) {
			std::cerr << "Runtime Exception: " << mse.message << std::endl;
			interp.vm->Stop();
			return -1;
		}
	}
	if (interp.Done()) return 0;
	PrintErr("Command timed out.");
	return -1;
}

static int DoScriptFile(Interpreter &interp, String path) {
	// Read the file
	List<String> source;
	std::ifstream infile(path.c_str());
	if (!infile.is_open()) {
		std::cerr << "Error opening file: " << path.c_str() << std::endl;
		return -1;
	}
	std::string line;
	while (std::getline(infile, line)) {
		source.Add(line.c_str());
	}
	//Print(String("Read ") + String::Format(source.Count()) + (source.Count() == 1 ? " line" : " lines") + " from: " + path);

	// Comment out the first line, if it's a hashbang
	if (source.Count() > 0 and source[0].StartsWith("#!")) source[0] = "// " + source[0];
	
	// Concatenate and execute the code.
	return DoCommand(interp, Join("\n", source));
}

static List<String> testOutput;
static void PrintToTestOutput(String s, bool lineBreak=true) {
	testOutput.Add(s);
}

static void DoOneIntegrationTest(List<String> sourceLines, long sourceLineNum,
				 List<String> expectedOutput, long outputLineNum) {
//	std::cout << "Running test starting at line " << sourceLineNum << std::endl;
	
	testOutput.Clear();
	{
		Interpreter miniscript(sourceLines);
		miniscript.standardOutput = &PrintToTestOutput;
		miniscript.errorOutput = &PrintToTestOutput;
		miniscript.implicitOutput = &PrintToTestOutput;
		miniscript.RunUntilDone(60, false);
	}
	
	long minLen = expectedOutput.Count() < testOutput.Count() ? expectedOutput.Count() : testOutput.Count();
	for (long i = 0; i < minLen; i++) {
		if (testOutput[i] != expectedOutput[i]) {
			Print("TEST FAILED AT LINE " + String::Format(outputLineNum + i)
			+ "\n  EXPECTED: " + expectedOutput[i]
			+ "\n    ACTUAL: " + testOutput[i]);
		}
	}
	if (expectedOutput.Count() > testOutput.Count()) {
		Print("TEST FAILED: MISSING OUTPUT AT LINE " + String::Format(outputLineNum + testOutput.Count()));
		for (long i = testOutput.Count(); i < expectedOutput.Count(); i++) {
			Print("  MISSING: " + expectedOutput[i]);
		}
	} else if (testOutput.Count() > expectedOutput.Count()) {
		Print("TEST FAILED: EXTRA OUTPUT AT LINE " + String::Format(outputLineNum + expectedOutput.Count()));
		for (long i = expectedOutput.Count(); i < testOutput.Count(); i++) {
			Print("  EXTRA: " + testOutput[i]);
		}
	}
	testOutput.Clear();
}

void RunIntegrationTests(String path) {
	std::ifstream infile(path.c_str());

	if (!infile.is_open()) {
		Print(String("\nFailed to open ") + path + "\n");
		return;
	}

	List<String> sourceLines;
	List<String> expectedOutput;
	long testLineNum = 0;
	long outputLineNum = 0;
	
	char buf[1024];
	String line;
	long lineNum = 0;
	bool inOutputSection = false;
	while (infile.good()) {
		infile.getline(buf, sizeof(buf));
		lineNum++;
		line = buf;

		if (line.StartsWith("====")) {
			if (sourceLines.Count() > 0 && sourceLines[0][0] < 0x80) {
				DoOneIntegrationTest(sourceLines, testLineNum, expectedOutput, outputLineNum);
			}
			sourceLines.Clear();
			expectedOutput.Clear();
			testLineNum = lineNum + 1;
			inOutputSection = false;
		} else if (line.StartsWith("----")) {
			expectedOutput.Clear();
			inOutputSection = true;
			outputLineNum = lineNum + 1;
		} else if (inOutputSection) {
			expectedOutput.Add(line);
		} else {
			sourceLines.Add(line);
		}
	}
	if (sourceLines.Count() > 0) {
		DoOneIntegrationTest(sourceLines, testLineNum, expectedOutput, outputLineNum);
	}
	Print("\nIntegration tests complete.\n");
}

void PrepareShellArgs(int argc, const char* argv[], int startingAt) {
	ValueList args;
	for (int i=startingAt; i<argc; i++) {
		args.Add(String(argv[i]));
	}
	shellArgs = args;
}

int main(int argc, const char * argv[]) {
	
#if(DEBUG)
	std::cout << "StringStorage instances at start (from static keywords, etc.): " << StringStorage::instanceCount << std::endl;
	std::cout << "total RefCountedStorage instances at start (from static keywords, etc.): " << RefCountedStorage::instanceCount << std::endl;
#endif

	UnitTest::RunAllTests();

#if(DEBUG)
	std::cout << "StringStorage instances left: " << StringStorage::instanceCount << std::endl;
	std::cout << "total RefCountedStorage instances left (includes 2 Unicode case maps): " << RefCountedStorage::instanceCount << std::endl;
#endif
	
	MiniScript::hostVersion = 1.3;
#if _WIN32 || _WIN64
	MiniScript::hostName = "Command-Line (Windows)";
#elif defined(__APPLE__) || defined(__FreeBSD__)
	MiniScript::hostName = "Command-Line (Unix)";
#else
	MiniScript::hostName = "Command-Line (Linux)";
#endif
	MiniScript::hostInfo = "https://miniscript.org/cmdline/";
	
	AddPathEnvVars();
	AddScriptPathVar("");
	AddShellIntrinsics();
	
	Interpreter interp;
	ConfigInterpreter(interp);
	
	bool launchReplAfterScript = false;
	for (int i=1; i<argc; i++) {
		String arg = argv[i];
		if (arg == "-h" or arg == "-?" or arg == "--help") {
			PrintHeaderInfo();
			PrintHelp(argv[0]);
			return 0;
		} else if (arg == "-i") {
			launchReplAfterScript = true;
		} else if (arg == "-q") {
			printHeaderInfo = false;
		} else if (arg == "-c") {
			i++;
			if (i >= argc) return ReturnErr("Command expected after -c option");
			String cmd = argv[i];
			return DoCommand(interp, cmd);
		} else if (arg == "--dumpTAC") {
			dumpTAC = true;
		} else if (arg == "--itest") {
			PrintHeaderInfo();
			i++;
			if (i >= argc) return ReturnErr("Path to test suite expected after --itest option");
			RunIntegrationTests(argv[i]);
			return 0;
		} else if (arg == "-") {
			PrintHeaderInfo();
			PrepareShellArgs(argc, argv, i);
			return DoREPL(interp);
		} else if (not arg.StartsWith("-")) {
			PrepareShellArgs(argc, argv, i);
			AddScriptPathVar(arg.c_str());
			int rc = DoScriptFile(interp, arg);
			if (!launchReplAfterScript) return rc;
			break;
		} else {
			PrintHeaderInfo();
			return ReturnErr(String("Unknown option: ") + arg);
		}
	}
	
	// If we get to here, then we exhausted all our options.
	// (We might have executed a script by this point if `-i` was provided.)
	// Drop into the REPL.
	if (launchReplAfterScript) std::cout << std::endl;  // separate visualy script's output from REPL
	PrintHeaderInfo();
	PrepareShellArgs(argc, argv, 1);
	return DoREPL(interp);
}
