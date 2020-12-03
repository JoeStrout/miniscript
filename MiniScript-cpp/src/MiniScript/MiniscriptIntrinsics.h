//
//  MiniscriptIntrinsics.h
//  MiniScript
//
//	This file defines the Intrinsic class, which represents a built-in function
//	available to MiniScript code.  All intrinsics are held in static storage, so
//	this class includes static functions such as GetByName to look up
//	already-defined intrinsics.  See Chapter 2 of the MiniScript Integration
//	Guide for details on adding your own intrinsics.
//
//	This file also contains the Intrinsics static class, where all of the standard
//	intrinsics are defined.  This is initialized automatically, so normally you
//	don’t need to worry about it, though it is a good place to look for examples
//	of how to write intrinsic functions.
//
//	Note that you should put any intrinsics you add in a separate file; leave the
//	MiniScript source files untouched, so you can easily replace them when updates
//	become available.
//
//
//  Created by Joe Strout on 6/8/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MINISCRIPTINTRINSICS_H
#define MINISCRIPTINTRINSICS_H

#include "MiniscriptTypes.h"

namespace MiniScript {

	class Context;

	// Host app information.  If you fill these in, they will be presented to
	// the user via the `version` intrinsic.
	extern String hostName;
	extern String hostInfo;
	extern double hostVersion;
	
	class Intrinsics {
	public:
		static void InitIfNeeded();
		
		// Helper method to compile a call to Slice.
		static void CompileSlice(List<TACLine> code, Value list, Value fromIdx, Value toIdx, int resultTempNum);
		
		static Value FunctionType();
		static Value ListType();
		static Value MapType();
		static Value NumberType();
		static Value StringType();
	private:
		static bool initialized;
	};
	
	class IntrinsicResultStorage : public RefCountedStorage {
	public:
		bool done;			// true if our work is complete; false if we need to Continue
		Value result;		// final result if done; in-progress data if not done
		
	private:
		IntrinsicResultStorage() : done(true) {}
		virtual ~IntrinsicResultStorage() {}
		friend class IntrinsicResult;
	};
	
	class IntrinsicResult {
	public:
		IntrinsicResult() : rs(nullptr) {}
		IntrinsicResult(Value value, bool done=true) : rs(nullptr) {
			ensureStorage();
			rs->result = value;
			rs->done = done;
		}
		IntrinsicResult(const IntrinsicResult& other) {	((IntrinsicResult&)other).ensureStorage(); rs = other.rs; retain(); }
		IntrinsicResult& operator= (const IntrinsicResult& other) {	((IntrinsicResult&)other).ensureStorage(); other.rs->refCount++; release(); rs = other.rs; return *this; }

		~IntrinsicResult() { release(); }
		
		bool Done() { return not rs or rs->done; }
		Value Result() { return rs ? rs->result : Value::null; }

		static IntrinsicResult Null;		// represents a completed, null result
		static IntrinsicResult EmptyString;	// represents "" (empty string) result
		
	private:
		IntrinsicResult(IntrinsicResultStorage* storage) : rs(storage) {}  // (assumes we grab an existing reference)
		void forget() { rs = nullptr; }
		
		void retain() { if (rs) rs->refCount++; }
		void release() { if (rs and --(rs->refCount) == 0) { delete rs; rs = nullptr; } }
		void ensureStorage() { if (!rs) rs = new IntrinsicResultStorage(); }
		IntrinsicResultStorage *rs;
	};

	class Intrinsic {
	public:
		// name of this intrinsic (should be a valid MiniScript identifier)
		String name;

		// actual C++ code invoked by the intrinsic
		IntrinsicResult (*code)(Context *context, IntrinsicResult partialResult);
		
		// a numeric ID (used internally -- don't worry about this)
		long id() { return numericID; }
		
		void AddParam(String name, Value defaultValue) { function->parameters.Add(FuncParam(name, defaultValue)); }
		void AddParam(String name, double defaultValue);
		void AddParam(String name) { AddParam(name, Value::null); }

		/// GetFunc is used internally by the compiler to get the MiniScript function
		/// that makes an intrinsic call.
		Value GetFunc();

		// Look up an Intrinsic by its internal numeric ID.
		static Intrinsic *GetByID(long id) { return all[id]; }
		
		// Look up an Intrinsic by its name.
		static Intrinsic *GetByName(String name) {
			Intrinsics::InitIfNeeded();
			Intrinsic* result = nullptr;
			nameMap.Get(name, &result);
			return result;
		}
		
		/// Factory method to create a new Intrinsic, filling out its name as given,
		/// and other internal properties as needed.  You'll still need to add any
		/// parameters, and define the code it runs.
		static Intrinsic* Create(String name);
		
		// Internally-used function to execute an intrinsic (by ID) given a context
		// and a partial result.
		static IntrinsicResult Execute(long id, Context *context, IntrinsicResult partialResult);
	
	private:
		Intrinsic() {}		// don't use this; use Create factory method instead.

		FunctionStorage* function;
		Value valFunction;		// (cached wrapper for function)
		long numericID;			// also its index in the 'all' list

		static List<Intrinsic*> all;
		static Dictionary<String, Intrinsic*, hashString> nameMap;
	};

	void InitRand(unsigned int seed);
}



#endif /* MINISCRIPTINTRINSICS_H */
