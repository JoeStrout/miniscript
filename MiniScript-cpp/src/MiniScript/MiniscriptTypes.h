//
//  MiniscriptTypes.hpp
//  MiniScript
//
//  Created by Joe Strout on 5/31/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MINISCRIPTTYPES_H
#define MINISCRIPTTYPES_H

#include "String.h"
#include "List.h"
#include "Dictionary.h"

namespace MiniScript {
	
	extern const String VERSION;
	
	class FuncParam;
	class TACLine;
	class Value;
	class Context;
	class Machine;
	
	unsigned int HashValue(const Value& v);
	
	typedef List<Value> ValueList;
	typedef ListStorage<Value> ValueListStorage;
	typedef DictionaryStorage<Value, Value> ValueDictStorage;
	typedef DictIterator<Value, Value> ValueDictIterator;
	typedef Dictionary<Value, Value, HashValue> ValueDict;

	/// <summary>
	/// FunctionStorage: our internal representation of a MiniScript function.  This includes
	/// its parameters and its compiled code.  (It does not include a name -- functions don't
	/// actually HAVE names; instead there are named variables whose value may happen to be
	/// a function.)
	/// </summary>
	class FunctionStorage : public RefCountedStorage {
	public:
		// Function parameters
		List<FuncParam> parameters;
		
		// Function code (compiled down to TAC form)
		List<TACLine> code;
		
		// Local variables where the function was defined {#8}
		ValueDict outerVars;
		
		FunctionStorage *BindAndCopy(ValueDict contextVariables);
	};

	class SeqElemStorage;

	enum class ValueType {
		Null,
		Number,
		Temp,
		// Ref-counted types:
		String,
		List,
		Map,
		Function,
		Var,
		SeqElem,
		Handle		// (any opaque RefCountedData subclass needed by the host app)
	};
	
	String ToString(ValueType type);

	class Value {
	public:
		static long maxStringSize;
		static long maxListSize;
		
		ValueType type;
		bool noInvoke;
		union {
			double number;
			RefCountedStorage *ref;
			int tempNum;
		} data;
		
		// constructors from base types
		Value() : type(ValueType::Null), noInvoke(false) {}
		Value(double number) : type(ValueType::Number), noInvoke(false) { data.number = number; }
		Value(const char *s) : type(ValueType::String), noInvoke(false) { String temp(s); data.ref = temp.ss; temp.forget(); }
		Value(const String& s) : type(ValueType::String), noInvoke(false) { data.ref = (s.ss ? s.ss : emptyString.data.ref);	retain(); }
		Value(const ValueList& l) : type(ValueType::List), noInvoke(false) { ((ValueList&)l).ensureStorage(); data.ref = l.ls; retain(); }
		Value(const ValueDict& d) : type(ValueType::Map), noInvoke(false) { ((ValueDict&)d).ensureStorage(); data.ref = d.ds; retain(); }
		Value(FunctionStorage *s) : type(ValueType::Function), noInvoke(false) { data.ref = s; }
		Value(SeqElemStorage *s);

		// some factory functions to make things clearer
		static Value Temp(const int tempNum) { return Value(tempNum, ValueType::Temp); }
		static Value Var(const String& ident) { return Value(ident, ValueType::Var); }
		static Value SeqElem(const Value& seq, const Value& idx);
		static Value NewHandle(RefCountedStorage* data) { Value v; v.type = ValueType::Handle; v.data.ref = data; return v; }
		static Value Truth(bool b) { return b ? one : zero; }
		static Value Truth(double b);

		static Value GetKeyValuePair(Value map, long index);
		
		// copy-ctor, assignment-op, destructor
		Value(const Value &other) : type(other.type), noInvoke(other.noInvoke) {
			data = other.data;
			if (usesRef()) retain();
		}
		Value& operator= (const Value& other) {
			if (other.usesRef() and other.data.ref) other.data.ref->retain();
			if (usesRef()) release();
			type = other.type;
			noInvoke = other.noInvoke;
			data = other.data;
			return *this;
		}
		inline ~Value() { if (usesRef()) release(); }

		// conversions
		String ToString(Machine *vm=NULL);
		String CodeForm(Machine *vm, int recursionLimit=-1);
		long IntValue();
		bool BoolValue();
		double DoubleValue() const { return type == ValueType::Number ? data.number : 0; }
		
		// Looking up the inner value, *without* conversion.
		// Note that these do NOT return a temp string/list/dict; they return
		// an ordinary, fully-fledged object you can keep around as long as you like.
		String GetString() const { Assert(type == ValueType::String or type == ValueType::Var);
			StringStorage *ss = (StringStorage*)(data.ref);
			if (!data.ref) return String();
			ss->retain();
			return String(ss, false); }
		ValueList GetList() const { Assert(type == ValueType::List); ValueList l((ValueListStorage*)(data.ref), false); return l; }
		ValueDict GetDict() { Assert(type == ValueType::Map); if (not data.ref) data.ref = new ValueDictStorage(); ValueDict d((ValueDictStorage*)(data.ref)); d.retain(); return d; }

		// evaluation
		bool IsNull() const {
			return type == ValueType::Null /* || (usesRef() && data.ref == nullptr) */;
		}

		Value Val(Context *context, ValueDict *outFoundInMap=NULL) const;
		
		/// Evaluate each of our contained elements, and if any of those is a variable
		/// or temp, then resolve them now.  CAUTION: do not mutate the original list
		/// or map!  We may need it in its original form on future iterations.
		Value FullEval(Context *context);
		
		/// Create a copy of this value, evaluating sub-values as we go.
		/// This is used when a list or map literal appears in the source, to
		/// ensure that each time that code executes, we get a new, distinct
		/// mutable object, rather than the same object referenced each time.
		Value EvalCopy(Context *context);
		
		/// <summary>
		/// Can we set elements within this value?  (I.e., is it a list or map?)
		/// </summary>
		/// <returns>true if SetElem can work; false if it does nothing</returns>
		bool CanSetElem() { return type == ValueType::List or type == ValueType::Map; }
		
		/// <summary>
		/// Set an element associated with the given index within this Value.
		/// </summary>
		/// <param name="index">index/key for the value to set</param>
		/// <param name="value">value to set</param>
		void SetElem(Value index, Value value);

		// Look up the given identifier in the given sequence, walking the
		// type chain until we either find it, or fail.
		static Value Resolve(Value sequence, String identifier, Context *context, ValueDict *outFoundInMap);

		/// <summary>
		/// Look up a value in this dictionary, walking the __isa chain to find
		/// it in a parent object if necessary.
		/// </summary>
		/// <param name="key">key to search for</param>
		/// <returns>value associated with that key, or null if not found</returns>
		Value Lookup(Value key) {
			Value result = null;
			Value& obj = *this;
			while (obj.type == ValueType::Map) {
				ValueDict d = obj.GetDict();
				if (d.Get(key, &result)) return result;
				if (!d.Get(Value::magicIsA, &obj)) break;
			}
			return null;
		}

		/// <summary>
		/// Determine whether this value is the given type (or some subclass)
		/// in the context of the given virtual machine.
		/// </summary>
		bool IsA(Value type, Machine *vm);
		
		// handy statics (DO NOT MUTATE THESE!)
		static Value zero;			// 0
		static Value one;			// 1
		static Value emptyString;	// ""
		static Value magicIsA;		// "__isa"
		static Value null;			// null
		static Value keyString;		// "key"
		static Value valueString;	// "value"
		static Value implicitResult;	// variable "_"

		inline bool operator==(const Value& rhs) const;
		inline bool operator!=(const Value& rhs) const { return !(*this == rhs); }
		unsigned int Hash() const;
		static double Equality(const Value& lhs, const Value& rhs, int recursionDepth=16);
		
	private:
		// private constructors used by factory functions
		Value(const int tempNum, ValueType type) : type(type), noInvoke(false) { data.tempNum = tempNum; }	// (type should be ValueType::Temp)
		Value(const String& s, ValueType type) : type(type), noInvoke(false) { data.ref = s.ss; retain(); }

		// reference handling (for types where that applies)
		bool usesRef() const { return type >= ValueType::String; }
		void retain() { if (data.ref) data.ref->retain(); }
		void release() { if (data.ref) { data.ref->release(); data.ref = nullptr; } }

		// equality helpers
		static bool Equal(StringStorage *lhs, StringStorage *rhs);
		static bool Equal(ListStorage<Value> *lhs, ListStorage<Value> *rhs);
		static bool Equal(DictionaryStorage<Value, Value> *lhs, DictionaryStorage<Value, Value> *rhs);
		static bool Equal(SeqElemStorage *lhs, SeqElemStorage *rhs);
	};
	
	class FuncParam {
	public:
		FuncParam() {}
		
		String name;
		Value defaultValue;
		
		FuncParam(String name, Value defaultValue) : name(name), defaultValue(defaultValue) {}
	};
	
	
	inline bool Value::operator==(const Value& rhs) const {
		if (type != rhs.type) return false;
		switch (type) {
			case ValueType::Null:
				return true;		// null values are always equal
				
			case ValueType::Number:
				return data.number == rhs.data.number;
				
			case ValueType::String:
			case ValueType::Var:
			{
				if (data.ref == rhs.data.ref) return true;
				if (!data.ref || !rhs.data.ref) return false;
				return Equal((StringStorage*)data.ref, (StringStorage*)rhs.data.ref);
			}
			case ValueType::List:
			{
				if (data.ref == rhs.data.ref) return true;
				if (!data.ref || !rhs.data.ref) return false;
				return Equal((ListStorage<Value>*)data.ref, (ListStorage<Value>*)rhs.data.ref);
			}
			case ValueType::Map:
			{
				if (data.ref == rhs.data.ref) return true;
				if (!data.ref || !rhs.data.ref) return false;
				return Equal((DictionaryStorage<Value, Value>*)data.ref, (DictionaryStorage<Value, Value>*)rhs.data.ref);
			}
			case ValueType::Function:
				// Two functions are equal only if they refer to the exact same function
				return (data.ref == rhs.data.ref);

			case ValueType::Temp:
				return (data.tempNum == rhs.data.tempNum);
				
			case ValueType::SeqElem:
				if (data.ref == rhs.data.ref) return true;
				if (!data.ref || !rhs.data.ref) return false;
				return Equal((SeqElemStorage*)data.ref, (SeqElemStorage*)rhs.data.ref);
		}
		return false;
	}
	
	/// <summary>
	/// Basically this just makes a Value number out of a double,
	/// BUT it is optimized for the case where the given value
	///	is either 0 or 1 (as is usually the case with truth tests).
	/// </summary>
	inline Value Value::Truth(double truthValue) {
		if (truthValue == 0.0) return Value::zero;
		if (truthValue == 1.0) return Value::one;
		return truthValue;
	}


	/// SeqElemStorage: internal representation of a sequence element reference.
	/// This simply boxes a sequence value and an index value.
	class SeqElemStorage : public RefCountedStorage {
	public:
		Value sequence;
		Value index;

		SeqElemStorage(Value seq, Value idx) : sequence(seq), index(idx) {}
	};

	inline Value::Value(SeqElemStorage *s) : type(ValueType::SeqElem), noInvoke(false) {
		data.ref = s;
	}

	inline Value Value::SeqElem(const Value& seq, const Value& idx) {
		return Value(new SeqElemStorage(seq, idx));
	}

	/// TextOutputMethod: function pointer that receives text to be output to the user
	/// (or whatever the host environment wants to do with it).
	typedef void (*TextOutputMethod)(String text);

}

#endif /* MINISCRIPTTYPES_H */
