//
//  MiniscriptIntrinsics.cpp
//  MiniScript
//
//  Created by Joe Strout on 6/8/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#define _USE_MATH_DEFINES
#include "MiniscriptIntrinsics.h"
#include "MiniscriptTAC.h"
#include "UnicodeUtil.h"
#include "SplitJoin.h"
#include <math.h>
#include <ctime>
#include <algorithm>

namespace MiniScript {

	String hostName = "";
	String hostInfo = "";
	double hostVersion = 0;
	
	static Value _functionType;
	static Value _listType;
	static Value _mapType;
	static Value _numberType;
	static Value _stringType;
	
	List<Intrinsic*> Intrinsic::all;
	Dictionary<String, Intrinsic*, hashString> Intrinsic::nameMap;
	IntrinsicResult IntrinsicResult::Null;	// represents a completed, null result
	IntrinsicResult IntrinsicResult::EmptyString(Value("")); // represents an empty string result

	bool Intrinsics::initialized = false;
	
	
	static bool randInitialized = false;

	static inline void InitRand() {
		if (!randInitialized) {
			srand((unsigned int)time(NULL));
			for (int i=0; i<10; i++) rand();
			randInitialized = true;
		}
	}

	void InitRand(unsigned int seed) {
		srand(seed);
		randInitialized = true;
	}

	static IntrinsicResult intrinsic_abs(Context *context, IntrinsicResult partialResult) {
		Value x = context->GetVar("x");
		return IntrinsicResult(fabs(x.DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_acos(Context *context, IntrinsicResult partialResult) {
		Value x = context->GetVar("x");
		return IntrinsicResult(acos(x.DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_asin(Context *context, IntrinsicResult partialResult) {
		Value x = context->GetVar("x");
		return IntrinsicResult(asin(x.DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_atan(Context *context, IntrinsicResult partialResult) {
		double y = context->GetVar("y").DoubleValue();
		double x = context->GetVar("x").DoubleValue();
		if (x == 1.0) return IntrinsicResult(atan(y));
		return IntrinsicResult(atan2(y, x));
	}
	
	static IntrinsicResult intrinsic_bitAnd(Context *context, IntrinsicResult partialResult) {
		Value i = context->GetVar("i");
		Value j = context->GetVar("j");
		return IntrinsicResult(i.IntValue() & j.IntValue());
	}
	
	static IntrinsicResult intrinsic_bitOr(Context *context, IntrinsicResult partialResult) {
		Value i = context->GetVar("i");
		Value j = context->GetVar("j");
		return IntrinsicResult(i.IntValue() | j.IntValue());
	}
	
	static IntrinsicResult intrinsic_bitXor(Context *context, IntrinsicResult partialResult) {
		Value i = context->GetVar("i");
		Value j = context->GetVar("j");
		return IntrinsicResult(i.IntValue() ^ j.IntValue());
	}

	static IntrinsicResult intrinsic_char(Context *context, IntrinsicResult partialResult) {
		long codePoint = context->GetVar("codePoint").IntValue();
		char buf[5];
		long len = UTF8Encode((unsigned long)codePoint, (unsigned char*)buf);
		String s(buf, (size_t)len);
		return IntrinsicResult(s);
	}

	static IntrinsicResult intrinsic_ceil(Context *context, IntrinsicResult partialResult) {
		Value x = context->GetVar("x");
		return IntrinsicResult(ceil(x.DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_code(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		long codepoint = 0;
		if (not self.IsNull()) codepoint = UTF8Decode((unsigned char*)(self.ToString().c_str()));
		return IntrinsicResult(codepoint);
	}
	
	static IntrinsicResult intrinsic_cos(Context *context, IntrinsicResult partialResult) {
		Value radians = context->GetVar("radians");
		return IntrinsicResult(cos(radians.DoubleValue()));
	}

	static IntrinsicResult intrinsic_floor(Context *context, IntrinsicResult partialResult) {
		Value x = context->GetVar("x");
		return IntrinsicResult(floor(x.DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_function(Context *context, IntrinsicResult partialResult) {
		if (context->vm->functionType.IsNull()) {
			context->vm->functionType = Intrinsics::FunctionType().EvalCopy(context->vm->GetGlobalContext());
		}
		return IntrinsicResult(context->vm->functionType);
	};

	static IntrinsicResult intrinsic_hash(Context *context, IntrinsicResult partialResult) {
		Value obj = context->GetVar("obj");
		return IntrinsicResult(obj.Hash());
	}
	
	static IntrinsicResult intrinsic_hasIndex(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		Value index = context->GetVar("index");
		if (self.type == ValueType::List) {
			if (index.type != ValueType::Number) return IntrinsicResult(Value::zero);		// #3
			ValueList list = self.GetList();
			long i = index.IntValue();
			return IntrinsicResult(Value::Truth(i >= -list.Count() and i < list.Count()));
		} else if (self.type == ValueType::String) {
			String str = self.GetString();
			long i = index.IntValue();
			return IntrinsicResult(Value::Truth(i >= -str.Length() and i < str.Length()));
		} else if (self.type == ValueType::Map) {
			ValueDict map = self.GetDict();
			return IntrinsicResult(Value::Truth(map.ContainsKey(index)));
		}
		return IntrinsicResult::Null;
	}

	static IntrinsicResult intrinsic_indexes(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		if (self.type == ValueType::Map) {
			ValueDict map = self.GetDict();
			return IntrinsicResult(map.Keys());
		} else if (self.type == ValueType::List) {
			ValueList list = self.GetList();
			long count = list.Count();
			ValueList indexes(count);
			for (long i=0; i<count; i++) indexes.Add(i);
			return IntrinsicResult(indexes);
		} else if (self.type == ValueType::String) {
			String str = self.GetString();
			long count = str.Length();
			ValueList indexes(count);
			for (long i=0; i<count; i++) indexes.Add(i);
			return IntrinsicResult(indexes);
		}
		return IntrinsicResult::Null;
	}

	static IntrinsicResult intrinsic_indexOf(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		Value value = context->GetVar("value");
		Value after = context->GetVar("after");
		if (self.type == ValueType::List) {
			ValueList list = self.GetList();
			long count = list.Count();
			long afterIdx = -1;
			if (!after.IsNull()) afterIdx = after.IntValue();
			if (afterIdx < -1) afterIdx += count;
			if (afterIdx < -1 || afterIdx > count-1) return IntrinsicResult::Null;
			for (long i=afterIdx+1; i<count; i++) {
				if (Value::Equality(list[i], value) == 1) return IntrinsicResult(i);
			}
		} else if (self.type == ValueType::String) {
			String str = self.GetString();
			String s = value.ToString();
			long afterIdx = -1;
			if (!after.IsNull()) afterIdx = after.IntValue();
			if (afterIdx < -1) afterIdx += str.Length();
			long idx = str.IndexOf(s, afterIdx+1);
			if (idx >= 0) return IntrinsicResult(idx);
		} else if (self.type == ValueType::Map) {
			ValueDict dict = self.GetDict();
			bool sawAfter = after.IsNull();
			for (ValueDictIterator kv = dict.GetIterator(); !kv.Done(); kv.Next()) {
				if (!sawAfter) {
					if (Value::Equality(kv.Key(), after) == 1) sawAfter = true;
				} else {
					if (Value::Equality(kv.Value(), value) == 1) return IntrinsicResult(kv.Key());
    			}
			}
		}
		return IntrinsicResult::Null;
	}

	static IntrinsicResult intrinsic_insert(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		Value index = context->GetVar("index");
		Value value = context->GetVar("value");
		if (index.IsNull()) throw RuntimeException("insert: index argument required");
		if (index.type != ValueType::Number) throw new RuntimeException("insert: number required for index argument");
		long idx = index.IntValue();
		if (self.type == ValueType::List) {
			ValueList list = self.GetList();
			long count = list.Count();
			if (idx < 0) idx += count + 1;	// +1 because we are inserting AND counting from the end.
			CheckRange(idx, 0, count);		// and allowing all the way up to .Count here, because insert.
			list.Insert(value, idx);
			return IntrinsicResult(self);
		} else if (self.type == ValueType::String) {
			String s = self.ToString();
			if (idx < 0) idx += s.Length() + 1;
			CheckRange(idx, 0, s.Length());
			s = s.Substring(0, idx) + value.ToString() + s.Substring(idx);
			return IntrinsicResult(s);
		} else {
			throw new RuntimeException("insert called on invalid type");
		}
	};

	static IntrinsicResult intrinsic_join(Context *context, IntrinsicResult partialResult) {
		Value val = context->GetVar("self");
		String delim = context->GetVar("delimiter").ToString();
		if (val.type != ValueType::List) return IntrinsicResult(val);
		ValueList src = val.GetList();
		StringList list(src.Count());
		for (int i=0; i<src.Count(); i++) {
			list.Add(src[i].ToString());
		}
		String result = Join(delim, list);
		return IntrinsicResult(result);
	}
	
	static IntrinsicResult intrinsic_len(Context *context, IntrinsicResult partialResult) {
		Value val = context->GetVar("self");
		if (val.type == ValueType::List) {
			ValueList list = val.GetList();
			return IntrinsicResult(list.Count());
		} else if (val.type == ValueType::String) {
			String str = val.GetString();
			return IntrinsicResult(str.Length());
		} else if (val.type == ValueType::Map) {
			return IntrinsicResult(val.GetDict().Count());
		}
		return IntrinsicResult::Null;
	}
	
	static IntrinsicResult intrinsic_list(Context *context, IntrinsicResult partialResult) {
		if (context->vm->listType.IsNull()) {
			context->vm->listType = Intrinsics::ListType().EvalCopy(context->vm->GetGlobalContext());
		}
		return IntrinsicResult(context->vm->listType);
	};
	
	
	static IntrinsicResult intrinsic_log(Context *context, IntrinsicResult partialResult) {
		double x = context->GetVar("x").DoubleValue();
		double base = context->GetVar("base").DoubleValue();
		double result;
		if (fabs(base - 2.718282) < 0.000001) result = log(x);
		else result = log(x) / log(base);
		return IntrinsicResult(result);
	}
	
	static IntrinsicResult intrinsic_lower(Context *context, IntrinsicResult partialResult) {
		Value val = context->GetVar("self");
		if (val.type == ValueType::String) {
			String str = val.GetString();
			return IntrinsicResult(str.ToLower());
		}
		return IntrinsicResult(val);
	}

	static IntrinsicResult intrinsic_map(Context *context, IntrinsicResult partialResult) {
		if (context->vm->mapType.IsNull()) {
			context->vm->mapType = Intrinsics::MapType().EvalCopy(context->vm->GetGlobalContext());
		}
		return IntrinsicResult(context->vm->mapType);
	};
	
	
	static IntrinsicResult intrinsic_number(Context *context, IntrinsicResult partialResult) {
		if (context->vm->numberType.IsNull()) {
			context->vm->numberType = Intrinsics::NumberType().EvalCopy(context->vm->GetGlobalContext());
		}
		return IntrinsicResult(context->vm->numberType);
	};
	
	
	static IntrinsicResult intrinsic_pi(Context *context, IntrinsicResult partialResult) {
		return IntrinsicResult(M_PI);
	}

	static IntrinsicResult intrinsic_print(Context *context, IntrinsicResult partialResult) {
		Value s = context->GetVar("s");
		if (!s.IsNull()) (*context->vm->standardOutput)(s.ToString());
		else (*context->vm->standardOutput)("null");
		return IntrinsicResult::Null;
	}
	
	static IntrinsicResult intrinsic_pop(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		if (self.type == ValueType::List) {
			ValueList list = self.GetList();
			long count = list.Count();
			if (count < 1) return IntrinsicResult::Null;
			Value result = list[count-1];
			list.RemoveAt(count-1);
			return IntrinsicResult(result);
		} else if (self.type == ValueType::Map) {
			ValueDict map = self.GetDict();
			if (map.Count() < 1) return IntrinsicResult::Null;
			ValueDictIterator kv = map.GetIterator();
			if (!kv.Done()) {
				Value key = kv.Key();
				map.Remove(key);
				return IntrinsicResult(key);
			}
		}
		return IntrinsicResult::Null;
	}
	
	static IntrinsicResult intrinsic_pull(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		if (self.type == ValueType::List) {
			ValueList list = self.GetList();
			long count = list.Count();
			if (count < 1) return IntrinsicResult::Null;
			Value result = list[0];
			list.RemoveAt(0);
			return IntrinsicResult(result);
		} else if (self.type == ValueType::Map) {
			ValueDict map = self.GetDict();
			if (map.Count() < 1) return IntrinsicResult::Null;
			ValueDictIterator kv = map.GetIterator();
			if (!kv.Done()) {
				Value key = kv.Key();
				map.Remove(key);
				return IntrinsicResult(key);
			}
		}
		return IntrinsicResult::Null;
	}
	
	static IntrinsicResult intrinsic_push(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		Value value = context->GetVar("value");
		if (self.type == ValueType::List) {
			ValueList list = self.GetList();
			list.Add(value);
			return IntrinsicResult(self);
		} else if (self.type == ValueType::Map) {
			ValueDict map = self.GetDict();
			map.SetValue(value, Value::one);
			return IntrinsicResult(self);
		}
		return IntrinsicResult::Null;
	}
	
	static IntrinsicResult intrinsic_range(Context *context, IntrinsicResult partialResult) {
		Value p0 = context->GetVar("from");
		Value p1 = context->GetVar("to");
		Value p2 = context->GetVar("step");
		double fromVal = p0.DoubleValue();
		double toVal = p1.DoubleValue();
		double step = (toVal >= fromVal ? 1 : -1);
		if (p2.type == ValueType::Number) step = p2.DoubleValue();
		if (step == 0) throw RuntimeException("range() error (step==0)");
		int count = (int)((toVal - fromVal) / step) + 1;
		if (count > Value::maxListSize) throw LimitExceededException("list too large");
		try {
			ValueList values(count);
			for (double v = fromVal; step > 0 ? (v <= toVal) : (v >= toVal); v += step) {
				values.Add(v);
			}
			return IntrinsicResult(values);
		} catch (std::bad_alloc e) {
			throw LimitExceededException("range() error");
		}
	}
	
	static IntrinsicResult intrinsic_remove(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		Value k = context->GetVar("k");
		if (self.IsNull()) throw RuntimeException("argument to 'remove' must not be null");
		if (self.type == ValueType::Map) {
			ValueDict selfMap = self.GetDict();
			if (selfMap.ContainsKey(k)) {
				selfMap.Remove(k);
				return IntrinsicResult(Value::one);
			}
			return IntrinsicResult(Value::zero);
		} else if (self.type == ValueType::List) {
			if (k.IsNull()) throw RuntimeException("argument to 'remove' must not be null");
			ValueList selfList = self.GetList();
			long idx = k.IntValue();
			if (idx < 0) idx += selfList.Count();
			CheckRange(idx, 0, selfList.Count()-1);
			selfList.RemoveAt(idx);
			return IntrinsicResult::Null;
		} else if (self.type == ValueType::String) {
			if (k.IsNull()) throw RuntimeException("argument to 'remove' must not be null");
			String selfStr = self.GetString();
			String substr = k.ToString();
			long foundPosB = selfStr.IndexOfB(substr);
			if (foundPosB < 0) return IntrinsicResult(self);
			return IntrinsicResult(selfStr.ReplaceB(foundPosB, substr.LengthB(), String()));
		}
		throw TypeException("Type Error: 'remove' requires map, list, or string");
	}
	
	static IntrinsicResult intrinsic_replace(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		Value oldval = context->GetVar("oldval");
		Value newval = context->GetVar("newval");
		Value maxCountVal = context->GetVar("maxCount");
		if (self.IsNull()) throw RuntimeException("argument to 'replace' must not be null");
		long maxCount = -1;
		if (!maxCountVal.IsNull()) {
			maxCount = maxCountVal.IntValue();
			if (maxCount < 1) return IntrinsicResult(self);
		}
		long count = 0;
		if (self.type == ValueType::Map) {
			ValueDict selfMap = self.GetDict();
			for (ValueDictIterator kv = selfMap.GetIterator(); !kv.Done(); kv.Next()) {
				if (Value::Equality(kv.Value(), oldval) == 1) {
					selfMap.SetValue(kv.Key(), newval);
					count++;
					if (maxCount > 0 and count == maxCount) break;
				}
			}
			return IntrinsicResult(self);
		} else if (self.type == ValueType::List) {
			ValueList selfList = self.GetList();
			long listCount = selfList.Count();
			for (long i=0; i<listCount; i++) {
				if (Value::Equality(selfList[i], oldval) == 1) {
					selfList[i] = newval;
					count++;
					if (maxCount > 0 and count == maxCount) break;
				}
			}
			return IntrinsicResult(self);
		} else if (self.type == ValueType::String) {
			String str = self.ToString();
			String oldstr = oldval.ToString();
			if (oldstr.empty()) throw RuntimeException("replace: oldval argument is empty");
			String newstr = newval.ToString();
			long idx = 0;
			while (true) {
				idx = str.IndexOfB(oldstr, idx);
				if (idx < 0) break;
				str = str.SubstringB(0, idx) + newstr + str.SubstringB(idx + oldstr.LengthB());
				idx += newstr.LengthB();
				count++;
				if (maxCount > 0 && count == maxCount) break;
			}
			return IntrinsicResult(str);
		}
		throw TypeException("Type Error: 'replace' requires map, list, or string");
	}
	
	static IntrinsicResult intrinsic_round(Context *context, IntrinsicResult partialResult) {
		double num = context->GetVar("x").DoubleValue();
		long decimalPlaces = context->GetVar("decimalPlaces").IntValue();
		if (decimalPlaces == 0) return IntrinsicResult(round(num));	// easy case
		double f = pow(10, decimalPlaces);
		return IntrinsicResult(round(num*f) / f);
	};
	
	static IntrinsicResult intrinsic_rnd(Context *context, IntrinsicResult partialResult) {
		Value seed = context->GetVar("seed");
		if (seed.IsNull()) InitRand();
		else InitRand((unsigned int)seed.IntValue());
		double d = (double)rand() / (RAND_MAX + 1.0);
		return IntrinsicResult(d);
	};

	static IntrinsicResult intrinsic_sign(Context *context, IntrinsicResult partialResult) {
		double num = context->GetVar("x").DoubleValue();
		if (num < 0) return IntrinsicResult(-1);
		if (num > 0) return IntrinsicResult(Value::one);
		return IntrinsicResult(Value::zero);
	};

	static IntrinsicResult intrinsic_sin(Context *context, IntrinsicResult partialResult) {
		Value radians = context->GetVar("radians");
		return IntrinsicResult(sin(radians.DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_slice(Context *context, IntrinsicResult partialResult) {
		Value seq = context->GetVar("seq");
		long fromIdx = context->GetVar("from").IntValue();
		Value toVal = context->GetVar("to");
		long toIdx = 0;
		if (not toVal.IsNull()) toIdx = toVal.IntValue();
		if (seq.type == ValueType::List) {
			ValueList list = seq.GetList();
			long count = list.Count();
			if (fromIdx < 0) fromIdx += count;
			if (fromIdx < 0) fromIdx = 0;
			if (toVal.IsNull()) toIdx = count;
			if (toIdx < 0) toIdx += count;
			if (toIdx > count) toIdx = count;
			ValueList slice;
			if (fromIdx < count and toIdx > fromIdx) {
				for (long i = fromIdx; i < toIdx; i++) {
					slice.Add(list[i]);
				}
			}
			return IntrinsicResult(slice);
		} else if (seq.type == ValueType::String) {
			String str = seq.GetString();
			long length = str.Length();
			if (fromIdx < 0) fromIdx += length;
			if (fromIdx < 0) fromIdx = 0;
			if (toVal.IsNull()) toIdx = length;
			else if (toIdx < 0) toIdx += length;
			if (toIdx > length) toIdx = length;
			if (toIdx - fromIdx <= 0) return IntrinsicResult(Value::emptyString);
			return IntrinsicResult(str.Substring(fromIdx, toIdx - fromIdx));
		}
		return IntrinsicResult::Null;
	}
	

	struct KeyedValue {
		Value sortKey;
		Value value;
		int valueIndex;
	};
	
	bool sort_lesser(const Value& a, const Value& b) {
		// Always sort null to the end of the list.
		if (a.type == ValueType::Null) return false;
		if (b.type == ValueType::Null) return true;
		// If either argument is a string, do a string comparison
		if (a.type == ValueType::String) {
			if (b.type == ValueType::String) return a.GetString() < b.GetString();
			else return a.GetString() < const_cast<Value&>(b).ToString();
		} else if (b.type == ValueType::String) return const_cast<Value&>(a).ToString() < b.GetString();
		// If both arguments are numbers, compare numerically.
		if (a.type == ValueType::Number && b.type == ValueType::Number) {
			return a.DoubleValue() < b.DoubleValue();
		}
		// Otherwise, consider all values equal, for sorting purposes.
		return false;
	}

	bool sort_greater(const Value& a, const Value& b) {
		return sort_lesser(b, a);
	}

	bool sort_KeyedValue(const KeyedValue& a, const KeyedValue& b) {
		return sort_lesser(a.sortKey, b.sortKey);
	}

	bool sort_KeyedValueDesc(const KeyedValue& a, const KeyedValue& b) {
		return sort_lesser(b.sortKey, a.sortKey);
	}

	static IntrinsicResult intrinsic_sort(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		if (self.type != ValueType::List) return IntrinsicResult(self);
		ValueList list = self.GetList();
		if (list.Count() < 2) return IntrinsicResult(list);
		
		bool ascending = context->GetVar("ascending").BoolValue();
		
		Value byKey = context->GetVar("byKey");
		if (byKey.IsNull()) {
			// Simple case: sorting values as themselves.
			std::stable_sort(&list[0], &list[0] + list.Count(), ascending ? &sort_lesser : &sort_greater);
			return IntrinsicResult(list);
		}
		// Harder case: sorting values by a given map key or function.
		// Construct an array of ValuePair, sort that, and then convert back into a list of values.
		KeyedValue *arr = new KeyedValue[list.Count()];
		for (int i=0; i<list.Count(); i++) {
			arr[i].value = list[i];
			arr[i].valueIndex = i;
		}
		// The key for each item will be the item itself, unless it is a map, in which
		// case it's the item indexed by the given key.  (Works too for lists if our
		// index is an integer.)
		long byKeyInt = byKey.IntValue();
		for (long i=0; i<list.Count(); i++) {
			Value& item = list[i];
			if (item.type == ValueType::Map) arr[i].sortKey = item.Lookup(byKey);
			else if (item.type == ValueType::List) {
				ValueList itemList = item.GetList();
				if (byKeyInt > -itemList.Count() && byKeyInt < itemList.Count()) arr[i].sortKey = itemList.Item(byKeyInt);
				else arr[i].sortKey = Value::null;
			} else arr[i].sortKey = item;
		}
		// Sort our valueKey array
		std::sort(arr, arr + list.Count(), ascending ? &sort_KeyedValue : &sort_KeyedValueDesc);
		// Build our output (and release the temp array)
		for (int i=0; i<list.Count(); i++) list[i] = arr[i].value;
		delete[] arr;
		return IntrinsicResult(list);
	}
	
	static IntrinsicResult intrinsic_sqrt(Context *context, IntrinsicResult partialResult) {
		return IntrinsicResult(sqrt(context->GetVar("x").DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_str(Context *context, IntrinsicResult partialResult) {
		return IntrinsicResult(context->GetVar("x").ToString());
	}

	static IntrinsicResult intrinsic_string(Context *context, IntrinsicResult partialResult) {
		if (context->vm->stringType.IsNull()) {
			context->vm->stringType = Intrinsics::StringType().EvalCopy(context->vm->GetGlobalContext());
		}
		return IntrinsicResult(context->vm->stringType);
	};
	
	static IntrinsicResult intrinsic_shuffle(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		InitRand();
		if (self.type == ValueType::List) {
			ValueList list = self.GetList();
			// We'll do a Fisher-Yates shuffle, i.e., swap each element
			// with a randomly selected one.
			for (long i=list.Count()-1; i >= 1; i--) {
				int j = rand() % (i+1);
				Value temp = list[j];
				list[j] = list[i];
				list[i] = temp;
			}
		} else if (self.type == ValueType::Map) {
			ValueDict map = self.GetDict();
			// Fisher-Yates again, but this time, what we're swapping
			// is the values associated with the keys, not the keys themselves.
			ValueList keys = map.Keys();
			for (long i=keys.Count()-1; i >= 1; i--) {
				int j = rand() % (i+1);
				Value keyi = keys[i];
				Value keyj = keys[j];
				Value temp = map[keyj];
				map.SetValue(keyj, map[keyi]);
				map.SetValue(keyi, temp);
			}
		}
		return IntrinsicResult::Null;
	}
	
	static IntrinsicResult intrinsic_split(Context *context, IntrinsicResult partialResult) {
		String self = context->GetVar("self").ToString();
		String delim = context->GetVar("delimiter").ToString();
		long maxCount = context->GetVar("maxCount").IntValue();
		ValueList result;
		long posB = 0;
		while (posB < self.LengthB()) {
			long nextPos;
			if (maxCount >= 0 and result.Count() == maxCount - 1) nextPos = self.LengthB();
			else if (delim.empty()) nextPos = posB + 1;
			else nextPos = self.IndexOfB(delim, posB);
			if (nextPos < 0) nextPos = self.LengthB();
			result.Add(self.SubstringB(posB, nextPos - posB));
			posB = nextPos + delim.LengthB();
			if (posB == self.LengthB() && !delim.empty()) result.Add(Value::emptyString);
		}
		return IntrinsicResult(result);
	}
	
	static IntrinsicResult intrinsic_sum(Context *context, IntrinsicResult partialResult) {
		Value val = context->GetVar("self");
		double sum = 0;
		if (val.type == ValueType::List) {
			ValueList list = val.GetList();
			for (long i=list.Count()-1; i>=0; i--) {
				sum += list[i].DoubleValue();
			}
		} else if (val.type == ValueType::Map) {
			ValueDict map = val.GetDict();
			for (ValueDictIterator kv = map.GetIterator(); !kv.Done(); kv.Next()) {
				sum += kv.Value().DoubleValue();
			}
		}
		return IntrinsicResult(sum);
	}

	static IntrinsicResult intrinsic_tan(Context *context, IntrinsicResult partialResult) {
		Value radians = context->GetVar("radians");
		return IntrinsicResult(tan(radians.DoubleValue()));
	}
	
	static IntrinsicResult intrinsic_time(Context *context, IntrinsicResult partialResult) {
		return IntrinsicResult(context->vm->RunTime());
	}

	static IntrinsicResult intrinsic_upper(Context *context, IntrinsicResult partialResult) {
		Value val = context->GetVar("self");
		if (val.type == ValueType::String) {
			String str = val.GetString();
			return IntrinsicResult(str.ToUpper());
		}
		return IntrinsicResult(val);
	}
	
	static IntrinsicResult intrinsic_val(Context *context, IntrinsicResult partialResult) {
		Value val = context->GetVar("self");
		if (val.type == ValueType::Number) return IntrinsicResult(val);
		if (val.type == ValueType::String) return IntrinsicResult(val.GetString().DoubleValue());
		return IntrinsicResult::Null;
	}
	
	static IntrinsicResult intrinsic_values(Context *context, IntrinsicResult partialResult) {
		Value self = context->GetVar("self");
		if (self.type == ValueType::Map) {
			ValueDict map = self.GetDict();
			return IntrinsicResult(map.Values());
		} else if (self.type == ValueType::String) {
			String str = self.GetString();
			ValueList values;
			if (str.empty()) return IntrinsicResult(values);
			const char *c = str.c_str();
			const char *endc = c + str.LengthB();
			while (c < endc) {
				const char *nextc = c;
				AdvanceUTF8((unsigned char**)&nextc, (unsigned char*)endc, 1);
				values.Add(String(c, nextc - c));
				c = nextc;
			}
			return IntrinsicResult(values);
		}
		return IntrinsicResult(self);
	}

	static IntrinsicResult intrinsic_version(Context *context, IntrinsicResult partialResult) {
		if (context->vm->versionMap.IsNull()) {
			ValueDict d;
			d.SetValue("miniscript", VERSION);
			
			// Convert from e.g. "Jan 14 2012" to sortable (SQL) format, "2012-01-14".
			String mmm_dd_yyyy(__DATE__);
			String dd = mmm_dd_yyyy.SubstringB(4, 2).Replace(" ", "0");
			String yyyy = mmm_dd_yyyy.SubstringB(7, 4);
			String mmm = mmm_dd_yyyy.Substring(0, 3);
			String mm;
			if (mmm == "Jan") mm = "01";
			else if (mmm == "Feb") mm = "02";
			else if (mmm == "Mar") mm = "03";
			else if (mmm == "Apr") mm = "04";
			else if (mmm == "May") mm = "05";
			else if (mmm == "Jun") mm = "06";
			else if (mmm == "Jul") mm = "07";
			else if (mmm == "Aug") mm = "08";
			else if (mmm == "Sep") mm = "09";
			else if (mmm == "Oct") mm = "10";
			else if (mmm == "Nov") mm = "11";
			else if (mmm == "Dec") mm = "12";
			else mm = mmm;	// (should never happen)
			d.SetValue("buildDate", yyyy + "-" + mm + "-" + dd);

			d.SetValue("host", hostVersion);
			d.SetValue("hostName", hostName);
			d.SetValue("hostInfo", hostInfo);
			context->vm->versionMap = Value(d);
		}
		return IntrinsicResult(context->vm->versionMap);
	}
	
	static IntrinsicResult intrinsic_wait(Context *context, IntrinsicResult partialResult) {
		double now = context->vm->RunTime();
		if (partialResult.Done()) {
			// Just starting our wait; calculate end time and return as partial result
			double interval = context->GetVar("seconds").DoubleValue();
			return IntrinsicResult(Value(now + interval), false);
		} else {
			// Continue until current time exceeds the time in the partial result
			if (now > partialResult.Result().DoubleValue()) return IntrinsicResult::Null;
			return partialResult;
		}
	}

	static IntrinsicResult intrinsic_yield(Context *context, IntrinsicResult partialResult) {
		context->vm->yielding = true;
		return IntrinsicResult::Null;
	}

	//------------------------------------------------------------------------------------------
	
	IntrinsicResult Intrinsic::Execute(long id, Context *context, IntrinsicResult partialResult) {
		Intrinsic* item = GetByID(id);
		return item->code(context, partialResult);
	}

	/// <summary>
	/// Factory method to create a new Intrinsic, filling out its name as given,
	/// and other internal properties as needed.  You'll still need to add any
	/// parameters, and define the code it runs.
	/// </summary>
	/// <param name="name">intrinsic name</param>
	/// <returns>freshly minted (but empty) static Intrinsic</returns>
	Intrinsic* Intrinsic::Create(String name) {
		Intrinsic* result = new Intrinsic();
		result->name = name;
		result->numericID = all.Count();
		result->function = new FunctionStorage();
		result->valFunction = Value(result->function);
		all.Add(result);
		if (!name.empty()) nameMap.SetValue(name, result);
		return result;
	}
	
	void Intrinsic::AddParam(String name, double defaultValue) {
		if (defaultValue == 0) AddParam(name, Value::zero);
		else if (defaultValue == 1) AddParam(name, Value::one);
		else AddParam(name, Value(defaultValue));
	}
	
	/// GetFunc is used internally by the compiler to get the MiniScript function
	/// that makes an intrinsic call.
	Value Intrinsic::GetFunc() {
		if (function->code.Count() == 0) {
			// Our little wrapper function is a single opcode: CallIntrinsicA.
			// It really exists only to provide a local variable context for the parameters.
			function->code.Add(TACLine(Value::Temp(0), TACLine::Op::CallIntrinsicA, Value(numericID)));
		}
		return valFunction;
	}

	void Intrinsics::InitIfNeeded() {
		if (initialized) return;		// our work is already done; bail out
		initialized = true;
		Intrinsic *f;
		
		f = Intrinsic::Create("abs");
		f->AddParam("x", 0);
		f->code = &intrinsic_abs;
		
		f = Intrinsic::Create("acos");
		f->AddParam("x", 0);
		f->code = &intrinsic_acos;
		
		f = Intrinsic::Create("asin");
		f->AddParam("x", 0);
		f->code = &intrinsic_asin;
		
		f = Intrinsic::Create("atan");
		f->AddParam("y", 0);
		f->AddParam("x", 1);
		f->code = &intrinsic_atan;
		
		f = Intrinsic::Create("bitAnd");
		f->AddParam("i", 0);
		f->AddParam("j", 0);
		f->code = &intrinsic_bitAnd;
		
		f = Intrinsic::Create("bitOr");
		f->AddParam("i", 0);
		f->AddParam("j", 0);
		f->code = &intrinsic_bitOr;
		
		f = Intrinsic::Create("bitXor");
		f->AddParam("i", 0);
		f->AddParam("j", 0);
		f->code = &intrinsic_bitXor;
		
		f = Intrinsic::Create("char");
		f->AddParam("codePoint", 65);
		f->code = &intrinsic_char;
		
		f = Intrinsic::Create("ceil");
		f->AddParam("x", 0);
		f->code = &intrinsic_ceil;
		
		f = Intrinsic::Create("code");
		f->AddParam("self");
		f->code = &intrinsic_code;
		
		f = Intrinsic::Create("cos");
		f->AddParam("radians", 0);
		f->code = &intrinsic_cos;
		
		f = Intrinsic::Create("floor");
		f->AddParam("x", 0);
		f->code = &intrinsic_floor;
		
		f = Intrinsic::Create("funcRef");
		f->code = &intrinsic_function;
		
		f = Intrinsic::Create("hash");
		f->AddParam("obj");
		f->code = &intrinsic_hash;
		
		f = Intrinsic::Create("hasIndex");
		f->AddParam("self");
		f->AddParam("index");
		f->code = &intrinsic_hasIndex;
		
		f = Intrinsic::Create("indexes");
		f->AddParam("self");
		f->code = &intrinsic_indexes;
		
		f = Intrinsic::Create("indexOf");
		f->AddParam("self");
		f->AddParam("value");
		f->AddParam("after", Value::null);
		f->code = &intrinsic_indexOf;
		
		f = Intrinsic::Create("insert");
		f->AddParam("self");
		f->AddParam("index");
		f->AddParam("value");
		f->code = &intrinsic_insert;
		
		f = Intrinsic::Create("join");
		f->AddParam("self");
		f->AddParam("delimiter", " ");
		f->code = &intrinsic_join;
		
		f = Intrinsic::Create("len");
		f->AddParam("self");
		f->code = &intrinsic_len;
		
		f = Intrinsic::Create("list");
		f->code = &intrinsic_list;

		f = Intrinsic::Create("log");
		f->AddParam("x");
		f->AddParam("base", 10);
		f->code = &intrinsic_log;
		
		f = Intrinsic::Create("lower");
		f->AddParam("self");
		f->code = &intrinsic_lower;
		
		f = Intrinsic::Create("map");
		f->code = &intrinsic_map;
		
		f = Intrinsic::Create("number");
		f->code = &intrinsic_number;
		
		f = Intrinsic::Create("pi");
		f->code = &intrinsic_pi;
		
		f = Intrinsic::Create("print");
		f->AddParam("s", Value::emptyString);
		f->code = &intrinsic_print;
		
		f = Intrinsic::Create("pop");
		f->AddParam("self");
		f->code = &intrinsic_pop;
		
		f = Intrinsic::Create("pull");
		f->AddParam("self");
		f->code = &intrinsic_pull;
		
		f = Intrinsic::Create("push");
		f->AddParam("self");
		f->AddParam("value");
		f->code = &intrinsic_push;
		
		f = Intrinsic::Create("range");
		f->AddParam("from", 0);
		f->AddParam("to", 0);
		f->AddParam("step");
		f->code = &intrinsic_range;
		
		f = Intrinsic::Create("remove");
		f->AddParam("self");
		f->AddParam("k");
		f->code = &intrinsic_remove;
		
		f = Intrinsic::Create("replace");
		f->AddParam("self");
		f->AddParam("oldval");
		f->AddParam("newval");
		f->AddParam("maxCount");
		f->code = &intrinsic_replace;
		
		f = Intrinsic::Create("round");
		f->AddParam("x", 0);
		f->AddParam("decimalPlaces", 0);
		f->code = &intrinsic_round;
		
		f = Intrinsic::Create("rnd");
		f->AddParam("seed");
		f->code = &intrinsic_rnd;

		f = Intrinsic::Create("sign");
		f->AddParam("x", 0);
		f->code = &intrinsic_sign;

		f = Intrinsic::Create("sin");
		f->AddParam("radians", 0);
		f->code = &intrinsic_sin;

		f = Intrinsic::Create("slice");
		f->AddParam("seq");
		f->AddParam("from", 0);
		f->AddParam("to");
		f->code = &intrinsic_slice;

		f = Intrinsic::Create("sort");
		f->AddParam("self", 0);
		f->AddParam("byKey");
		f->AddParam("ascending", 1);
		f->code = &intrinsic_sort;
		
		f = Intrinsic::Create("split");
		f->AddParam("self");
		f->AddParam("delimiter", " ");
		f->AddParam("maxCount", -1);
		f->code = &intrinsic_split;

		f = Intrinsic::Create("sqrt");
		f->AddParam("x", 0);
		f->code = &intrinsic_sqrt;

		f = Intrinsic::Create("str");
		f->AddParam("x", 0);
		f->code = &intrinsic_str;

		f = Intrinsic::Create("string");
		f->code = &intrinsic_string;
		
		f = Intrinsic::Create("shuffle");
		f->AddParam("self");
		f->code = &intrinsic_shuffle;

		f = Intrinsic::Create("sum");
		f->AddParam("self");
		f->code = &intrinsic_sum;

		f = Intrinsic::Create("tan");
		f->AddParam("radians", 0);
		f->code = &intrinsic_tan;

		f = Intrinsic::Create("time");
		f->code = &intrinsic_time;

		f = Intrinsic::Create("upper");
		f->AddParam("self");
		f->code = &intrinsic_upper;
		
		f = Intrinsic::Create("val");
		f->AddParam("self", 0);
		f->code = &intrinsic_val;

		f = Intrinsic::Create("values");
		f->AddParam("self");
		f->code = &intrinsic_values;
		
		f = Intrinsic::Create("version");
		f->code = &intrinsic_version;
		
		f = Intrinsic::Create("wait");
		f->AddParam("seconds", 1);
		f->code = &intrinsic_wait;
		
		f = Intrinsic::Create("yield");
		f->code = &intrinsic_yield;
		
	}
	
	// Helper method to compile a call to Slice (when invoked directly via slice syntax).
	void Intrinsics::CompileSlice(List<TACLine> code, Value list, Value fromIdx, Value toIdx, int resultTempNum) {
		code.Add(TACLine(TACLine::Op::PushParam, list));
		code.Add(TACLine(TACLine::Op::PushParam, fromIdx.IsNull() ? Value::zero : fromIdx));
		code.Add(TACLine(TACLine::Op::PushParam, toIdx));// toIdx == null ? TAC.Num(0) : toIdx));
		Value func = Intrinsic::GetByName("slice")->GetFunc();
		code.Add(TACLine(Value::Temp(resultTempNum), TACLine::Op::CallFunctionA, func, 3));
	}

	
	Value Intrinsics::FunctionType() {
		if (_functionType.IsNull()) {
			ValueDict d;
			_functionType = d;
		}
		return _functionType;
	}

	Value Intrinsics::ListType() {
		if (_listType.IsNull()) {
			ValueDict d;
			d.SetValue("hasIndex", Intrinsic::GetByName("hasIndex")->GetFunc());
			d.SetValue("indexes", Intrinsic::GetByName("indexes")->GetFunc());
			d.SetValue("indexOf",  Intrinsic::GetByName("indexOf")->GetFunc());
			d.SetValue("insert",  Intrinsic::GetByName("insert")->GetFunc());
			d.SetValue("join",  Intrinsic::GetByName("join")->GetFunc());
			d.SetValue("len",  Intrinsic::GetByName("len")->GetFunc());
			d.SetValue("pop",  Intrinsic::GetByName("pop")->GetFunc());
			d.SetValue("pull",  Intrinsic::GetByName("pull")->GetFunc());
			d.SetValue("push",  Intrinsic::GetByName("push")->GetFunc());
			d.SetValue("shuffle",  Intrinsic::GetByName("shuffle")->GetFunc());
			d.SetValue("sort",  Intrinsic::GetByName("sort")->GetFunc());
			d.SetValue("sum",  Intrinsic::GetByName("sum")->GetFunc());
			d.SetValue("remove",  Intrinsic::GetByName("remove")->GetFunc());
			d.SetValue("replace",  Intrinsic::GetByName("replace")->GetFunc());
			d.SetValue("values",  Intrinsic::GetByName("values")->GetFunc());
			_listType = d;
		}
		return _listType;
	}

	Value Intrinsics::MapType() {
		if (_mapType.IsNull()) {
			ValueDict d;
			d.SetValue("hasIndex",  Intrinsic::GetByName("hasIndex")->GetFunc());
			d.SetValue("indexes",  Intrinsic::GetByName("indexes")->GetFunc());
			d.SetValue("indexOf",  Intrinsic::GetByName("indexOf")->GetFunc());
			d.SetValue("len",  Intrinsic::GetByName("len")->GetFunc());
			d.SetValue("pop",  Intrinsic::GetByName("pop")->GetFunc());
			d.SetValue("push",  Intrinsic::GetByName("push")->GetFunc());
			d.SetValue("shuffle",  Intrinsic::GetByName("shuffle")->GetFunc());
			d.SetValue("sum",  Intrinsic::GetByName("sum")->GetFunc());
			d.SetValue("remove",  Intrinsic::GetByName("remove")->GetFunc());
			d.SetValue("replace",  Intrinsic::GetByName("replace")->GetFunc());
			d.SetValue("values",  Intrinsic::GetByName("values")->GetFunc());
			_mapType = d;
		}
		return _mapType;
	}
	
	Value Intrinsics::NumberType() {
		if (_numberType.IsNull()) {
			ValueDict d;
			_numberType = d;
		}
		return _numberType;
	}
	
	Value Intrinsics::StringType() {
		if (_stringType.IsNull()) {
			ValueDict d;
			d.SetValue("hasIndex",  Intrinsic::GetByName("hasIndex")->GetFunc());
			d.SetValue("indexes",  Intrinsic::GetByName("indexes")->GetFunc());
			d.SetValue("indexOf",  Intrinsic::GetByName("indexOf")->GetFunc());
			d.SetValue("insert",  Intrinsic::GetByName("insert")->GetFunc());
			d.SetValue("code",  Intrinsic::GetByName("code")->GetFunc());
			d.SetValue("len",  Intrinsic::GetByName("len")->GetFunc());
			d.SetValue("lower",  Intrinsic::GetByName("lower")->GetFunc());
			d.SetValue("val",  Intrinsic::GetByName("val")->GetFunc());
			d.SetValue("remove",  Intrinsic::GetByName("remove")->GetFunc());
			d.SetValue("replace",  Intrinsic::GetByName("replace")->GetFunc());
			d.SetValue("split",  Intrinsic::GetByName("split")->GetFunc());
			d.SetValue("upper",  Intrinsic::GetByName("upper")->GetFunc());
			d.SetValue("values",  Intrinsic::GetByName("values")->GetFunc());
			_stringType = Value(d);
		}
		return _stringType;
	}

}
