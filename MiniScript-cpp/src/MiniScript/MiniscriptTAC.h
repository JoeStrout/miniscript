//
//  MiniscriptTAC.hpp
//  MiniScript
//
//  Created by Joe Strout on 6/1/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MINISCRIPTTAC_H
#define MINISCRIPTTAC_H

#include "MiniscriptTypes.h"
#include "MiniscriptErrors.h"
#include "MiniscriptIntrinsics.h"

namespace MiniScript {
	class Context;
	class Machine;
	class IntrinsicResult;
	class Interpreter;
	
	class TACLine {
	public:
		enum class Op {
			Noop = 0,
			AssignA,
			AssignImplicit,
			APlusB,
			AMinusB,
			ATimesB,
			ADividedByB,
			AModB,
			APowB,
			AEqualB,
			ANotEqualB,
			AGreaterThanB,
			AGreatOrEqualB,
			ALessThanB,
			ALessOrEqualB,
			AisaB,
			AAndB,
			AOrB,
			BindAssignA,
			CopyA,
			NotA,
			GotoA,
			GotoAifB,
			GotoAifTrulyB,
			GotoAifNotB,
			PushParam,
			CallFunctionA,
			CallIntrinsicA,
			ReturnA,
			ElemBofA,
			ElemBofIterA,
			LengthOfA
		};
		
		Value lhs;
		Op op;
		Value rhsA;
		Value rhsB;
		String comment;
		SourceLoc location;
		
		TACLine() : op(Op::Noop) {}
		TACLine(Value lhs, Op op, Value rhsA, Value rhsB=Value::null) : lhs(lhs), op(op), rhsA(rhsA), rhsB(rhsB) {}
		TACLine(Op op, Value rhsA, Value rhsB=Value()) : op(op), rhsA(rhsA), rhsB(rhsB) {}

		String ToString();
		Value Evaluate(Context *context);
	};
		
	class Context {
	public:
		List<TACLine> code;			// TAC lines we're executing
		long lineNum;				// next line to be executed
		ValueDict variables;		// local variables for this call frame
		ValueDict outerVars;		// variables of the context where this function was defined
		ValueList args;				// pushed arguments for upcoming calls
		Context *parent;			// parent (calling) context
		Value resultStorage;		// where to store the return value (in the calling context)
		Machine *vm;				// virtual machine
		IntrinsicResult partialResult;	// work-in-progress of our current intrinsic
		long implicitResultCounter;	// how many times we have stored an implicit result
		
		Context() : lineNum(0), parent(nullptr), vm(nullptr), implicitResultCounter(0) {}
		
		bool Done() { return lineNum >= code.Count(); }

		Context* Root() {
			Context* c = this;
			while (c->parent != nullptr) c = c->parent;
			return c;
		}
		
        void ClearCodeAndTemps() {
            code.Clear();
            lineNum = 0;
            temps.Clear();
        }
        
		void StoreValue(Value lhs, Value value);

		void SetTemp(int tempNum, Value value) {
			while (temps.Count() <= tempNum) temps.Add(Value::null);
			temps[tempNum] = value;
		}
		
		Value GetTemp(int tempNum) { return temps.Count() ? temps[tempNum] : Value::null; }

		Value GetTemp(int tempNum, Value defaultValue) {
			if (tempNum < temps.Count()) return temps[tempNum];
			return defaultValue;
		}
	
		void SetVar(String identifier, Value value);
		Value GetVar(String identifier);
		
		/// <summary>
		/// Store a parameter argument in preparation for an upcoming call
		/// (which should be executed in the context returned by NextCallContext).
		/// </summary>
		/// <param name="arg">Argument.</param>
		void PushParamArgument(Value arg) {
			if (args.Count() > 255) throw new RuntimeException("Argument limit exceeded");
			args.Add(arg);
		}

		/// <summary>
		/// Get a context for the next call, which includes any parameter arguments
		/// that have been set.
		/// </summary>
		/// <returns>The call context.</returns>
		/// <param name="func">Function to call.</param>
		/// <param name="argCount">How many arguments to pop off the stack.</param>
		/// <param name="gotSelf">Whether this method was called with dot syntax.</param>
		/// <param name="resultStorage">Value to stuff the result into when done.</param>
		Context* NextCallContext(FunctionStorage *func, long argCount, bool gotSelf, Value resultStorage);

		void JumpToEnd() { lineNum = code.Count(); }
		
	private:
		List<Value> temps;			// values of temporaries; temps[0] is always return value
	};
	
	class Machine {
	public:
//		Machine();
		Machine(Context *context, TextOutputMethod standardOutput);
		~Machine();
		
		bool Done() { return stack.Count() <= 1 and stack.Last()->Done(); }
		void Step();
		void Stop();
		void Reset();
		
		Context* GetGlobalContext() { return stack[0]; }
		Context* GetTopContext() { return stack.Last(); }
		String FindShortName(const Value& val);
		
		double RunTime() { return startTime  == 0 ? 0 : CurrentWallClockTime() - startTime; }
		
		TextOutputMethod standardOutput;
		bool storeImplicit;
		Interpreter *interpreter;		// (weak reference to interpreter that owns this VM)
		bool yielding;					// set to true by the yield intrinsic
		Value functionType;
		Value listType;
		Value mapType;
		Value numberType;
		Value stringType;
		Value versionMap;

	private:
		static double CurrentWallClockTime();
		
		void DoOneLine(TACLine& line, Context *context);
		void PopContext();
		
		List<Context*> stack;
		double startTime;		// value of CurrentWallClockTime() when machine began its run
	};
}


#endif /* MINISCRIPTTAC_H */
