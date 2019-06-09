//
//  MiniscriptInterpreter.h
//  MiniScript
//
//  Created by Joe Strout on 7/2/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MINISCRIPTINTERPRETER_H
#define MINISCRIPTINTERPRETER_H

#include "String.h"
#include "MiniscriptTAC.h"

namespace MiniScript {

	
	class Parser;
	
	class Interpreter {
		
	public:
		/// standardOutput: receives the output of the "print" intrinsic.
		TextOutputMethod standardOutput;

		/// implicitOutput: receives the value of expressions entered when
		/// in REPL mode.  If you're not using the REPL() method, you can
		/// safely ignore this.
		TextOutputMethod implicitOutput;
		
		/// errorOutput: receives error messages from the runtime.  (This happens
		/// via the ReportError method, which is virtual; so if you want to catch
		/// the actual exceptions rather than get the error messages as strings,
		/// you can subclass Interpreter and override that method.)
		TextOutputMethod errorOutput;

		/// hostData is just a convenient place for you to attach some arbitrary
		/// data to the interpreter.  It gets passed through to the context object,
		/// so you can access it inside your custom intrinsic functions.  Use it
		/// for whatever you like (or don't, if you don't feel the need).
		void* hostData;
		
		/// vm: the virtual machine this interpreter is running.  Most applications will
		/// not need to use this, but it's provided for advanced users.
		Machine *vm;
		
		/// Constructors
		Interpreter();
		Interpreter(String source);
		Interpreter(List<String> source);
		
		/// <summary>
		/// done: returns true when we don't have a virtual machine, or we do have
		/// one and it is done (has reached the end of its code).
		/// </summary>
		bool Done() { return vm == nullptr or vm->Done(); }

		/// <summary>
		/// Stop the virtual machine, and jump to the end of the program code.
		/// </summary>
		void Stop() { if (vm) vm->Stop(); }

		/// <summary>
		/// Reset the interpreter with the given source code.
		/// </summary>
		/// <param name="source"></param>
		void Reset(String source="") {
			this->source = source;
			parser = nullptr;
			vm = nullptr;
		}
		
		void Reset(List<String> source);

		/// <summary>
		/// Reset the virtual machine to the beginning of the code.  Note that this
		/// does *not* reset global variables; it simply clears the stack and jumps
		/// to the beginning.  Useful in cases where you have a short script you
		/// want to run over and over, without recompiling every time.
		/// </summary>
		void Restart() { if (vm) vm->Reset();	}

		/// <summary>
		/// Compile our source code, if we haven't already done so, so that we are
		/// either ready to run, or generate compiler errors (reported via errorOutput).
		/// </summary>
		void Compile();

		/// <summary>
		/// Run one step of the virtual machine.  This method is not very useful
		/// except in special cases; usually you will use RunUntilDone (above) instead.
		/// </summary>
		void Step();
		
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
		void RunUntilDone(double timeLimit=60, bool returnEarly=true);

		
		/// <summary>
		/// Read Eval Print Loop.  Run the given source until it either terminates,
		/// or hits the given time limit.  When it terminates, if we have new
		/// implicit output, print that to the implicitOutput stream.
		/// </summary>
		/// <param name="sourceLine">Source line.</param>
		/// <param name="timeLimit">Time limit.</param>
		void REPL(String sourceLine, double timeLimit=60);
		
		/// <summary>
		/// Return whether the parser needs more input, for example because we have
		/// run out of source code in the middle of an "if" block.  This is typically
		/// used with REPL for making an interactive console, so you can change the
		/// prompt when more input is expected.
		/// </summary>
		/// <returns></returns>
		bool NeedMoreInput();

		
	protected:
		virtual void ReportError(const MiniscriptException* mse);

	private:
		String source;
		Parser *parser;
	};
}

#endif // MINISCRIPTINTERPRETER_H
