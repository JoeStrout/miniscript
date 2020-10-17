//
//  MiniscriptInterpreter.cpp
//  MiniScript
//
//  Created by Joe Strout on 7/2/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "MiniscriptInterpreter.h"
#include "MiniscriptParser.h"
#include "SplitJoin.h"

namespace MiniScript {
	
	Interpreter::Interpreter() : standardOutput(nullptr), errorOutput(nullptr), implicitOutput(nullptr),
								parser(nullptr), vm(nullptr), hostData(nullptr) {
		
	}

	Interpreter::Interpreter(String source) : standardOutput(nullptr), errorOutput(nullptr), implicitOutput(nullptr),
	parser(nullptr), vm(nullptr), hostData(nullptr) {
		Reset(source);
	}
	
	Interpreter::Interpreter(List<String> source) : standardOutput(nullptr), errorOutput(nullptr), implicitOutput(nullptr),
	parser(nullptr), vm(nullptr), hostData(nullptr) {
		Reset(source);
	}

	void Interpreter::Reset(List<String> source) {
		Reset(Join("\n", source));
	}

	void Interpreter::Compile() {
		if (vm) return;		// already compiled
		if (not parser) parser = new Parser();
		try {
			parser->Parse(source);
			vm = parser->CreateVM(standardOutput);
			vm->interpreter = this;
		} catch (const MiniscriptException* mse) {
			ReportError(mse);
			delete mse;
		}
	}
	
	/// <summary>
	/// Run one step of the virtual machine.  This method is not very useful
	/// except in special cases; usually you will use RunUntilDone (above) instead.
	/// </summary>
	void Interpreter::Step() {
		try {
			Compile();
			if (vm) vm->Step();
		} catch (const MiniscriptException* mse) {
			ReportError(mse);
			delete mse;
		}
	}
	
	/// <summary>
	/// Run the compiled code until we either reach the end, or we reach the
	/// specified time limit.  In the latter case, you can then call RunUntilDone
	/// again to continue execution right from where it left off.
	///
	/// Or, if returnEarly is true, we will also return if we reach an intrinsic
	/// method that returns a partial result, indicating that it needs to wait
	/// for something.  Again, call RunUntilDone again later to continue.
	///
	/// Note that this method first compiles the source code if it wasn't compiled
	/// already, and in that case, may generate compiler errors.  And of course
	/// it may generate runtime errors while running.  In either case, these are
	/// reported via errorOutput.
	/// </summary>
	/// <param name="timeLimit">maximum amout of time to run before returning, in seconds</param>
	/// <param name="returnEarly">if true, return as soon as we reach an intrinsic that returns a partial result</param>
	void Interpreter::RunUntilDone(double timeLimit, bool returnEarly) {
		try {
			if (not vm) {
				Compile();
				if (not vm) return;	// (must have been some error)
			}
			double startTime = vm->RunTime();
			vm->yielding = false;
			while (not vm->Done() && !vm->yielding) {
				if (vm->RunTime() - startTime > timeLimit) return;	// time's up for now!
				vm->Step();		// update the machine
				if (returnEarly and not vm->GetTopContext()->partialResult.Done()) return;	// waiting for something
			}
		} catch (const MiniscriptException* mse) {
			ReportError(mse);
			delete mse;
			vm->GetTopContext()->JumpToEnd();
		}
	}

	/// <summary>
	/// Read Eval Print Loop.  Run the given source until it either terminates,
	/// or hits the given time limit.  When it terminates, if we have new
	/// implicit output, print that to the implicitOutput stream.
	/// </summary>
	/// <param name="sourceLine">Source line.</param>
	/// <param name="timeLimit">Time limit.</param>
	void Interpreter::REPL(String sourceLine, double timeLimit) {
		if (not parser) parser = new Parser();
		if (not vm) {
			vm = parser->CreateVM(standardOutput);
			vm->interpreter = this;
        } else if (vm->Done() && !parser->NeedMoreInput()) {
            // Since the machine and parser are both done, we don't really need the previously-compiled
            // code.  So let's clear it out, as a memory optimization.
            vm->GetTopContext()->ClearCodeAndTemps();
			parser->PartialReset();
        }
		if (sourceLine == "#DUMP") {
			//ToDo:			vm->DumpTopContext();
			return;
		}
		
		double startTime = vm->RunTime();
		Context *globalContext = vm->GetGlobalContext();
		long startImpResultCount = globalContext->implicitResultCounter;
		vm->storeImplicit = (implicitOutput != nullptr);
		
		try {
			if (not sourceLine.empty()) parser->Parse(sourceLine, true);
			if (not parser->NeedMoreInput()) {
				while (not vm->Done()) {
					if (vm->RunTime() - startTime > timeLimit) return;	// time's up for now!
					vm->Step();
				}
				if (implicitOutput and globalContext->implicitResultCounter > startImpResultCount) {
					Value result = globalContext->GetVar("_");
					if (!result.IsNull()) (*implicitOutput)(result.ToString(vm));
				}
			}
			
		} catch (const MiniscriptException* mse) {
			ReportError(mse);
			delete mse;
			// Attempt to recover from an error by jumping to the end of the code.
			vm->GetTopContext()->JumpToEnd();
		}
        
        
	}

	
	/// <summary>
	/// Return whether the parser needs more input, for example because we have
	/// run out of source code in the middle of an "if" block.  This is typically
	/// used with REPL for making an interactive console, so you can change the
	/// prompt when more input is expected.
	/// </summary>
	/// <returns></returns>
	bool Interpreter::NeedMoreInput() {
		return parser and parser->NeedMoreInput();
	}
	
	void Interpreter::ReportError(const MiniscriptException* mse) {
		if (errorOutput) (*errorOutput)(mse->Description());
	}

}
