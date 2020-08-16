//
//  MiniscriptTAC.cpp
//  MiniScript
//
//  Created by Joe Strout on 6/1/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "MiniscriptTAC.h"
#include <math.h>		// for pow() and fmod()
#if _WIN32 || _WIN64
	#include <windows.h>	// for GetTickCount
#else
	#include <sys/time.h>	// for gettimeofday
#endif
#include <iostream>		// (for debugging)

namespace MiniScript {

	static inline double Clamp01(double d) {
		if (d < 0) return 0;
		if (d > 1) return 1;
		return d;
	}

	static inline double AbsClamp01(double d) {
		if (d < 0) d = -d;
		if (d > 1) return 1;
		return d;
	}
	
	String TACLine::ToString() {
		String text;
		switch (op) {
			case Op::AssignA:
				text = lhs.ToString() + " := " + rhsA.ToString();
				break;
			case Op::AssignImplicit:
				text = "_ := " + rhsA.ToString();
				break;
			case Op::APlusB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " + " + rhsB.ToString();
				break;
			case Op::AMinusB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " - " + rhsB.ToString();
				break;
			case Op::ATimesB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " * " + rhsB.ToString();
				break;
			case Op::ADividedByB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " / " + rhsB.ToString();
				break;
			case Op::AModB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " % " + rhsB.ToString();
				break;
			case Op::APowB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " ^ " + rhsB.ToString();
				break;
			case Op::AEqualB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " == " + rhsB.ToString();
				break;
			case Op::ANotEqualB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " != " + rhsB.ToString();
				break;
			case Op::AGreaterThanB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " > " + rhsB.ToString();
				break;
			case Op::AGreatOrEqualB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " >= " + rhsB.ToString();
				break;
			case Op::ALessThanB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " < " + rhsB.ToString();
				break;
			case Op::ALessOrEqualB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " <= " + rhsB.ToString();
				break;
			case Op::AisaB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " isa " + rhsB.ToString();
				break;
			case Op::AAndB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " and " + rhsB.ToString();
				break;
			case Op::AOrB:
				text = lhs.ToString() + " := " + rhsA.ToString() + " or " + rhsB.ToString();
				break;
			case Op::BindAssignA:
				text = rhsA.ToString() + " := " + rhsB.ToString() + "; " + rhsA.ToString() + ".outerVars = <locals>";
				break;
			case Op::CopyA:
				text = lhs.ToString() + " := copy of " + rhsA.ToString();
				break;
			case Op::NotA:
				text = lhs.ToString() + " := not " + rhsA.ToString();
				break;
			case Op::GotoA:
				text = "goto " + rhsA.ToString();
				break;
			case Op::GotoAifB:
				text = "goto " + rhsA.ToString() + " if " + rhsB.ToString();
				break;
			case Op::GotoAifTrulyB:
				text = "goto " + rhsA.ToString() + " if truly " + rhsB.ToString();
				break;
			case Op::GotoAifNotB:
				text = "goto " + rhsA.ToString() + " if not " + rhsB.ToString();
				break;
			case Op::PushParam:
				text = "push param " + rhsA.ToString();
				break;
			case Op::CallFunctionA:
				text = lhs.ToString() + " := call " + rhsA.ToString() + " with " + rhsB.ToString() + " args";
				break;
			case Op::CallIntrinsicA:
				text = "intrinsic " + Intrinsic::GetByID(rhsA.IntValue())->name;
				break;
			case Op::ReturnA:
				text = lhs.ToString() + " := " + rhsA.ToString() + "; return";
				break;
			case Op::ElemBofA:
				text = lhs.ToString() + " := " + rhsA.ToString() + "[" + rhsB.ToString() + "]";
				break;
			case Op::ElemBofIterA:
				text = lhs.ToString() + " := " + rhsA.ToString() + " iter " + rhsB.ToString();
				break;
			case Op::LengthOfA:
				text = lhs.ToString() + " := len(" + rhsA.ToString() + ")";
				break;
			default:
				throw MiniscriptException(String("unknown opcode: ") + String::Format((int)op));
				
		}
		//				if (comment != null) text = text + "\t// " + comment;
		return text;

	}

	/// <summary>
	/// Evaluate this line and return the value that would be stored
	/// into the lhs.
	/// </summary>
	Value TACLine::Evaluate(Context *context) {
		if (op == Op::AssignA || op == Op::ReturnA || op == Op::AssignImplicit) {
			// Assignment is a bit of a special case.  It's EXTREMELY common
			// in TAC, so needs to be efficient, but we have to watch out for
			// the case of a RHS that is a list or map.  This means it was a
			// literal in the source, and may contain references that need to
			// be evaluated now.
			if (rhsA.type == ValueType::List || rhsA.type == ValueType::Map) {
				return rhsA.FullEval(context);
			} else if (rhsA.IsNull()) {
				return Value::null;
			} else {
				return rhsA.Val(context);
			}
		}
		if (op == Op::CopyA) {
			// This opcode is used for assigning a literal.  We actually have
			// to copy the literal, in the case of a mutable object like a
			// list or map, to ensure that if the same code executes again,
			// we get a new, unique object.
			return rhsA.EvalCopy((context));
		}
		
		Value opA = rhsA.type == ValueType::Null ? rhsA : rhsA.Val(context);
		Value opB = rhsB.type == ValueType::Null ? rhsB : rhsB.Val(context);
		
		if (op == Op::AisaB) {
			if (opA.IsNull()) return Value::Truth(opB.IsNull());
			return Value::Truth(opA.IsA(opB, context->vm));
		}

		if (op == Op::ElemBofA && opB.type == ValueType::String) {
			// You can now look for a String in almost anything...
			// and we have a convenient (and relatively fast) method for it:
			return Value::Resolve(opA, opB.ToString(), context, NULL);
		}
		
		// check for special cases of comparison to null (works with any type)
		if (op == Op::AEqualB && (opA.IsNull() || opB.IsNull())) {
			return Value::Truth(opA == opB);
		}
		if (op == Op::ANotEqualB and (opA.IsNull() or opB.IsNull())) {
			return Value::Truth(opA != opB);
		}
	
		// check for implicit coersion of other types to string; this happens
		// when either side is a string and the operator is addition.
		if ((opA.type == ValueType::String or opB.type == ValueType::String) and op == Op::APlusB) {
			if (opB.IsNull()) return opA;
			String sA = opA.ToString();
			String sB = opB.ToString();
			if (sA.LengthB() + sB.LengthB() > Value::maxStringSize) throw LimitExceededException("string too large");
			return Value(sA + sB);
		}

		
		if (opA.type == ValueType::Number) {
			double fA = opA.data.number;
			switch (op) {
				case Op::GotoA:
					context->lineNum = (int)fA;
					return Value::null;
				case Op::GotoAifB:
					if (!opB.IsNull() and opB.BoolValue()) context->lineNum = (int)fA;
					return Value::null;
				case Op::GotoAifTrulyB:
				{
					// Unlike GotoAifB, which branches if B has any nonzero
					// value (including 0.5 or 0.001), this branches only if
					// B is TRULY true, i.e., its integer value is nonzero.
					// (Used for short-circuit evaluation of "or".)
					long i = 0;
					if (!opB.IsNull()) i = opB.IntValue();
					if (i != 0) context->lineNum = (int)fA;
					return Value::null;
				}
				case Op::GotoAifNotB:
					if (opB.IsNull() or !opB.BoolValue()) context->lineNum = (int)fA;
					return Value::null;
				case Op::CallIntrinsicA:
				{
					// NOTE: intrinsics do not go through NextFunctionContext.  Instead
					// they execute directly in the current context.  (But usually, the
					// current context is a wrapper function that was invoked via
					// Op::CallFunction, so it got a parameter context at that time.)
					IntrinsicResult result = Intrinsic::Execute((int)fA, context, context->partialResult);
					if (result.Done()) {
//						context->partialResult = null;
						return result.Result();
					}
					// OK, this intrinsic function is not yet done with its work.
					// We need to stay on this same line and call it again with
					// the partial result, until it reports that its job is complete.
					context->partialResult = result;
					context->lineNum--;
					return Value::null;
				}
				case Op::NotA:
					return Value(1.0 - AbsClamp01(fA));
				default:
					break;
			}
			if (opB.type == ValueType::Number or opB.IsNull()) {
				double fB = not opB.IsNull() ? opB.data.number : 0;
				switch (op) {
					case Op::APlusB:
						return Value(fA + fB);
					case Op::AMinusB:
						return Value(fA - fB);
					case Op::ATimesB:
						return Value(fA * fB);
					case Op::ADividedByB:
						return Value(fA / fB);
					case Op::AModB:
						return Value(fmod(fA, fB));
					case Op::APowB:
						return Value(pow(fA, fB));
					case Op::AEqualB:
						return Value::Truth(fA == fB);
					case Op::ANotEqualB:
						return Value::Truth(fA != fB);
					case Op::AGreaterThanB:
						return Value::Truth(fA > fB);
					case Op::AGreatOrEqualB:
						return Value::Truth(fA >= fB);
					case Op::ALessThanB:
						return Value::Truth(fA < fB);
					case Op::ALessOrEqualB:
						return Value::Truth(fA <= fB);
					case Op::AAndB:
						if (!(opB.type == ValueType::Number)) fB = opB.BoolValue() ? 1 : 0;
						return Value(Clamp01(fA * fB));
					case Op::AOrB:
						if (!(opB.type == ValueType::Number)) fB = opB.BoolValue() ? 1 : 0;
						return Value(Clamp01(fA + fB - fA * fB));
					default:
						break;
				}
			}
			// Handle equality testing between a number (opA) and a non-number (opB).
			// These are always considered unequal.
			if (op == Op::AEqualB) return Value::zero;
			if (op == Op::ANotEqualB) return Value::one;

		} else if (opA.type == ValueType::String) {
			String sA = opA.ToString();
			switch (op) {
				case Op::ATimesB:
				case Op::ADividedByB:
				{
					double factor = 0;
					if (op == Op::ATimesB) {
						CheckType(opB, ValueType::Number, "String replication");
						factor = opB.data.number;
					} else {
						CheckType(opB, ValueType::Number, "String division");
						factor = 1.0 / opB.data.number;
					}
					if (factor <= 0) return Value::emptyString;
					int repeats = (int)factor;
					size_t lenB = sA.LengthB();
					int extraChars = (int)(sA.Length() * (factor - repeats));
					String extraStr = sA.Substring(0, extraChars);
					size_t totalBytes = lenB * repeats + extraStr.LengthB();
					if (totalBytes > Value::maxStringSize) throw LimitExceededException("string too large");
					char *buf = new char[totalBytes+1];
					if (buf == NULL) return Value::null;
					char *ptr = buf;
					for (int i = 0; i < repeats; i++) {
						strncpy(ptr, sA.c_str(), lenB);
						ptr += lenB;
					}
					if (extraChars > 0) strncpy(ptr, extraStr.c_str(), extraStr.LengthB());
					buf[totalBytes] = 0;	// null terminator
					String result;
					result.takeoverBuffer(buf, totalBytes);
					return Value(result);
				}
				case Op::ElemBofA:
				case Op::ElemBofIterA:
				{
					long idx = opB.IntValue();
					long len = sA.Length();
					CheckRange(idx, -len, len - 1, "String index");
					if (idx < 0) idx += len;
					return Value(sA.Substring(idx, 1));
				}
			}
			if (opB.IsNull() or opB.type == ValueType::String) {
				switch (op) {
				case Op::AMinusB:
				{
					if (opB.IsNull()) return opA;
					String sB = opB.ToString();
					if (sA.EndsWith(sB)) sA = sA.SubstringB(0, sA.LengthB() - sB.LengthB());
					return Value(sA);
				} break;
				case Op::NotA:
					return Value::Truth(sA.empty());
				case Op::AEqualB:
					return Value::Truth(String::Compare(sA, opB.ToString()) == 0);
				case Op::ANotEqualB:
					return Value::Truth(String::Compare(sA, opB.ToString()) != 0);
				case Op::AGreaterThanB:
					return Value::Truth(String::Compare(sA, opB.ToString()) > 0);
				case Op::AGreatOrEqualB:
					return Value::Truth(String::Compare(sA, opB.ToString()) >= 0);
				case Op::ALessThanB:
					return Value::Truth(String::Compare(sA, opB.ToString()) < 0);
				case Op::ALessOrEqualB:
					return Value::Truth(String::Compare(sA, opB.ToString()) <= 0);
				case Op::LengthOfA:
					return Value(sA.Length());
				default:
					break;
				}
			} else {
				// RHS is neither null nor a string.
				// We no longer automatically coerce in all these cases; about
				// all we can do is equal or unequal testing.
				// (Note that addition was handled way above here.)
				if (op == Op::AEqualB) return Value::zero;
				if (op == Op::ANotEqualB) return Value::one;
			}
		 } else if (opA.type == ValueType::List) {
			 ValueList list = opA.GetList();
			if (op == Op::ElemBofA || op == Op::ElemBofIterA) {
				// list indexing
				long idx = opB.IntValue();
				long count = list.Count();
				CheckRange(idx, -count, count - 1, "list index");
				if (idx < 0) idx += count;
				return list[idx];
			} else if (op == Op::LengthOfA) {
				return Value(list.Count());
			} else if (op == Op::AEqualB) {
				return Value::Truth(Value::Equality(opA, opB));
			} else if (op == Op::ANotEqualB) {
				return Value::Truth(1.0 - Value::Equality(opA, opB));
			} else if (op == Op::APlusB) {
				// list concatenation
				CheckType(opB, ValueType::List, "list concatenation");
				ValueList list2 = opB.GetList();
				long count1 = list.Count();
				long count2 = list2.Count();
				if (count1 + count2 > Value::maxListSize) throw LimitExceededException("list too large");
				ValueList result(count1 + count2);
				for (long i=0; i<count1; i++) result.Add(list[i].Val(context));
				for (long i=0; i<count2; i++) result.Add(list2[i].Val(context));
				return Value(result);
			} else if (op == Op::ATimesB || op == Op::ADividedByB) {
				// list replication (or division)
				double factor = 0;
				if (op == Op::ATimesB) {
					CheckType(opB, ValueType::Number, "list replication");
					factor = opB.data.number;
				} else {
					CheckType(opB, ValueType::Number, "list division");
					factor = 1.0 / opB.data.number;
				}
				if (factor <= 0) return ValueList();
				long listCount = list.Count();
				long finalCount = (long)(listCount * factor);
				if (finalCount > Value::maxListSize) throw LimitExceededException("list too large");
				ValueList result(finalCount);
				for (long i = 0; i < finalCount; i++) {
					result.Add(list[i % listCount].Val(context));
				}
				return Value(result);
			} else if (op == Op::NotA) {
				return Value::Truth(!opA.BoolValue());
			}
		 } else if (opA.type == ValueType::Map) {
			if (op == Op::ElemBofA) {
				// map lookup
				// (note, cases where opB is a String are handled above, along with
				// all the other types; so we'll only get here for non-String cases)
				Value se = Value::SeqElem(opA, opB);
				return se.Val(context);
				// (This ensures we walk the "__isa" chain in the standard way.)
			} else if (op == Op::ElemBofIterA) {
				// With a map, ElemBofIterA is different from ElemBofA.  This one
				// returns a mini-map containing a key/value pair.
				return Value::GetKeyValuePair(opA, opB.IntValue());
			} else if (op == Op::LengthOfA) {
				return Value(opA.GetDict().Count());
			} else if (op == Op::AEqualB) {
				return Value::Truth(Value::Equality(opA, opB));
			} else if (op == Op::ANotEqualB) {
				return Value::Truth(1.0 - Value::Equality(opA, opB));
			} else if (op == Op::APlusB) {
				// map combination
				ValueDict map = opA.GetDict();
				CheckType(opB, ValueType::Map, "map combination");
				ValueDict map2 = opB.GetDict();
				ValueDict result;
				for (ValueDictIterator i = map.GetIterator(); !i.Done(); i.Next()) {
					result.SetValue(i.Key(), i.Value().Val(context));
				}
				for (ValueDictIterator i = map2.GetIterator(); !i.Done(); i.Next()) {
					result.SetValue(i.Key(), i.Value().Val(context));
				}
				return result;
			} else if (op == Op::NotA) {
				return Value::Truth(!opA.BoolValue());
			}
		} else if (opA.type == ValueType::Function and opB.type == ValueType::Function) {
			FunctionStorage *fA = (FunctionStorage*)(opA.data.ref);
			FunctionStorage *fB = (FunctionStorage*)(opB.data.ref);
			switch (op) {
				case Op::AEqualB:
					return Value::Truth(fA == fB);
				case Op::ANotEqualB:
					return Value::Truth(fA != fB);
				default:
					break;
			}
		} else {
			// something else... perhaps null
			switch (op) {
				case Op::BindAssignA:
				{
					FunctionStorage *fA = (FunctionStorage*)(opA.data.ref);
					return Value(fA->BindAndCopy(context->variables));
				} break;
				case Op::NotA:
					return Value::Truth(!opA.BoolValue());
				default:
					break;
			}
		}
		
		if (op == Op::AAndB or op == Op::AOrB) {
			// We already handled the case where opA was a number above;
			// this code handles the case where opA is something else.
			double fA = opA.BoolValue() ? 1 : 0;
			double fB;
			if (opB.type == ValueType::Number) fB = opB.data.number;
			else fB = opB.BoolValue() ? 1 : 0;
			double result;
			if (op == Op::AAndB) {
				result = fA * fB;
			} else {
				result = 1.0 - (1.0 - AbsClamp01(fA)) * (1.0 - AbsClamp01(fB));
			}
			return Value(result);
		}
		return Value::null;
	}
	
	
	void Context::StoreValue(Value lhs, Value value) {
//		std::cout << "Storing into " << lhs.ToString().c_str() << ": " << value.ToString().c_str() << std::endl;
		if (lhs.type == ValueType::Temp) {
			SetTemp(lhs.data.tempNum, value);
		} else if (lhs.type == ValueType::Var) {
			SetVar(lhs.GetString(), value);
		} else if (lhs.type == ValueType::SeqElem) {
			SeqElemStorage *seqElem = (SeqElemStorage*)(lhs.data.ref);
			Value seq = seqElem->sequence.Val(this);
			if (seq.IsNull()) throw RuntimeException("can't set indexed element of null");
			if (not seq.CanSetElem()) throw RuntimeException("can't set an indexed element in this type");
			Value index = seqElem->index;
			if (index.type == ValueType::Var or index.type == ValueType::SeqElem or
				index.type == ValueType::Temp) index = index.Val(this);
			seq.SetElem(index, value);
		} else {
			if (!lhs.IsNull()) throw RuntimeException("not an lvalue");
		}
	}

	void Context::SetVar(String identifier, Value value) {
		if (identifier == "globals" or identifier == "locals" or identifier == "outer") {
			throw RuntimeException("can't assign to " + identifier);
		}
		if (!variables.ApplyAssignOverride(identifier, value)) {
			variables.SetValue(identifier, value);
		}
	}
	
	/// <summary>
	/// Get the value of a variable available in this context (including
	/// locals, globals, and intrinsics).  Raise an exception if no such
	/// identifier can be found.
	/// </summary>
	/// <param name="identifier">name of identifier to look up</param>
	/// <returns>value of that identifier</returns>
	Value Context::GetVar(String identifier) {
		// check for special built-in identifiers 'locals', 'globals', and 'outer'
		if (identifier == "locals") return variables;
		if (identifier == "globals") return Root()->variables;
		if (identifier == "outer") {
			if (!outerVars.empty()) return outerVars;
			return Root()->variables;
		}

		// check for a local variable
		Value result;
		if (variables.Get(identifier, &result)) return result;
		
		// check for a module variable
		if (!outerVars.empty() && outerVars.Get(identifier, &result)) return result;
		
		// OK, we don't have a local or module variable with that name.
		// Check the global scope (if that's not us already).
		if (parent != NULL) {
			Context* globals = Root();
			if (globals->variables.Get(identifier, &result)) return result;
		}
		
		// Finally, check intrinsics.
		Intrinsic* intrinsic = Intrinsic::GetByName(identifier);
		if (intrinsic != nullptr) return intrinsic->GetFunc();
		
		// No luck there either?  Undefined identifier.
		throw new UndefinedIdentifierException(identifier);
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
	Context* Context::NextCallContext(FunctionStorage *func, long argCount, bool gotSelf, Value resultStorage) {
		Context* result = new Context();
		
		result->code = func->code;
		result->resultStorage = resultStorage;
		result->parent = this;
		result->vm = vm;
		
		// Stuff arguments, stored in our 'args' stack,
		// into local variables corrersponding to parameter names.
		// As a special case, skip over the first parameter if it is named 'self'
		// and we were invoked with dot syntax.
		long selfParam = (gotSelf and func->parameters.Count() > 0 and func->parameters[0].name == "self" ? 1 : 0);
		for (long i = 0; i < argCount; i++) {
			// Careful -- when we pop them off, they're in reverse order.
			Value argument = args.Pop();
			long paramNum = argCount - 1 - i + selfParam;
			if (paramNum >= func->parameters.Count()) {
				throw TooManyArgumentsException();
			}
			result->SetVar(func->parameters[paramNum].name, argument);
		}
		// And fill in the rest with default values
		for (long paramNum = argCount+selfParam; paramNum < func->parameters.Count(); paramNum++) {
			result->SetVar(func->parameters[paramNum].name, func->parameters[paramNum].defaultValue);
		}
		
		return result;
	}
	
//	Machine::Machine() : stack(16), storeImplicit(false) {
//		Context *globalContext = new Context;
//		stack.Add(new Context);
//
//	}
	
	Machine::Machine(Context *root, TextOutputMethod output) : stack(16), storeImplicit(false), standardOutput(output), startTime(0) {
		// Note: this constructor adopts the given context, and destroys it later.
		root->vm = this;
		stack.Add(root);
	}
	
	Machine::~Machine() {
		for (long i = stack.Count() - 1; i >= 0; i--) {
			delete stack[i];
		}
		stack.Clear();
	}
	
	void Machine::Step() {
		if (stack.Count() == 0) return;		// not even a global context
		
		if (startTime == 0) startTime = CurrentWallClockTime();
		
		Context* context = stack.Last();
		while (context->Done()) {
			if (stack.Count() == 1) return;		// all done (can't pop the global context)
			PopContext();
			context = stack.Last();
		}
		
		TACLine& line = context->code[context->lineNum++];
		try {
			DoOneLine(line, context);
		} catch (MiniscriptException* mse) {
			mse->location = line.location;
			throw mse;
		}
	}
	
	void Machine::Stop() {
		while (stack.Count() > 1) delete stack.Pop();
		stack[0]->JumpToEnd();
	}
	
	void Machine::DoOneLine(TACLine& line, Context *context) {
		if (line.op == TACLine::Op::PushParam) {
			Value val = line.rhsA.IsNull() ? line.rhsA : line.rhsA.Val(context);
			context->PushParamArgument(val);
		} else if (line.op == TACLine::Op::CallFunctionA) {
			// Resolve rhsA.  If it's a function, invoke it; otherwise,
			// just store it directly.
			ValueDict valueFoundIn;
			Value funcVal = line.rhsA.Val(context, &valueFoundIn);		// resolves the whole dot chain, if any
			if (funcVal.type == ValueType::Function) {
				Value self;
				// bind "super" to the parent of the map the function was found in
				Value super = valueFoundIn.Lookup(Value::magicIsA, Value::null);
				if (line.rhsA.type == ValueType::SeqElem) {
					// bind "self" to the object used to invoke the call,
					// except when invoking via "super"
					Value seq = ((SeqElemStorage*)(line.rhsA.data.ref))->sequence;
					if (seq.type == ValueType::Var && seq.ToString() == "super") self = context->GetVar("self");
					else self = seq.Val(context);
				}
				long argCount = line.rhsB.IntValue();
				FunctionStorage *fs = (FunctionStorage*)(funcVal.data.ref);
				Context* nextContext = context->NextCallContext(fs, argCount, not self.IsNull(), line.lhs);
				nextContext->outerVars = fs->outerVars;
				if (!valueFoundIn.empty()) nextContext->SetVar("super", super);
				if (not self.IsNull()) nextContext->SetVar("self", self);
				stack.Add(nextContext);
			} else {
				// The user is attempting to call something that's not a function.
				// We'll allow that, but any number of parameters is too many.  [#35]
				// (No need to pop them, as the exception will pop the whole call stack anyway.)
				long argCount = line.rhsB.IntValue();
				if (argCount > 0) throw new TooManyArgumentsException();
				context->StoreValue(line.lhs, funcVal);
			}
		} else if (line.op == TACLine::Op::ReturnA) {
			Value val = line.Evaluate(context);
			context->StoreValue(line.lhs, val);
			PopContext();
		} else if (line.op == TACLine::Op::AssignImplicit) {
			Value val = line.Evaluate(context);
			if (storeImplicit) {
				context->StoreValue(Value::implicitResult, val);
				context->implicitResultCounter++;
			}
		} else {
			Value val = line.Evaluate(context);
			context->StoreValue(line.lhs, val);
		}
	}

	void Machine::PopContext() {
		// Our top context is done; pop it off, and copy the return value in temp 0.
		if (stack.Count() == 1) return;	// down to just the global stack (which we keep)
		Context* context = stack.Pop();
		Value result = context->GetTemp(0, Value::null);
		Value storage = context->resultStorage;
		delete context;
		context = stack.Last();
		context->StoreValue(storage, result);
	}

	String Machine::FindShortName(const Value& val) {
		String nullStr;
		if (stack.Count() < 1) return nullStr;
		Context *globalContext = stack[0];
		if (globalContext == NULL) return nullStr;
		for (ValueDictIterator kv = globalContext->variables.GetIterator(); !kv.Done(); kv.Next()) {
			if (kv.Value() == val && kv.Key() != val) return kv.Key().ToString();
		}
		return nullStr;
	}
	
	double Machine::CurrentWallClockTime() {
		#if _WIN32 || _WIN64
			return GetTickCount() * 0.001;
		#else
			struct timeval timecheck;
			gettimeofday(&timecheck, NULL);
			return (long)timecheck.tv_sec * 1.0 + (long)timecheck.tv_usec / 1000000.0;
		#endif
	}
}
