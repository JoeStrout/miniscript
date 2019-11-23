/*	MiniscriptIntrinsics.cs

This file defines the Intrinsic class, which represents a built-in function
available to MiniScript code.  All intrinsics are held in static storage, so
this class includes static functions such as GetByName to look up 
already-defined intrinsics.  See Chapter 2 of the MiniScript Integration
Guide for details on adding your own intrinsics.

This file also contains the Intrinsics static class, where all of the standard
intrinsics are defined.  This is initialized automatically, so normally you
don’t need to worry about it, though it is a good place to look for examples
of how to write intrinsic functions.

Note that you should put any intrinsics you add in a separate file; leave the
MiniScript source files untouched, so you can easily replace them when updates
become available.
*/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Globalization;

namespace Miniscript {
	/// <summary>
	/// IntrinsicCode is a delegate to the actual C# code invoked by an intrinsic method.
	/// </summary>
	/// <param name="context">TAC.Context in which the intrinsic was invoked</param>
	/// <param name="partialResult">partial result from a previous invocation, if any</param>
	/// <returns>result of the computation: whether it's complete, a partial result if not, and a Value if so</returns>
	public delegate Intrinsic.Result IntrinsicCode(TAC.Context context, Intrinsic.Result partialResult);
	
	/// <summary>
	/// Information about the app hosting MiniScript.  Set this in your main program.
	/// This is provided to the user via the `version` intrinsic.
	/// </summary>
	public static class HostInfo {
		public static string name;		// name of the host program
		public static string info;		// URL or other short info about the host
		public static double version;	// host program version number
	}
		
	/// <summary>
	/// Intrinsic: represents an intrinsic function available to MiniScript code.
	/// </summary>
	public class Intrinsic {
		// name of this intrinsic (should be a valid MiniScript identifier)
		public string name;
		
		// actual C# code invoked by the intrinsic
		public IntrinsicCode code;
		
		// a numeric ID (used internally -- don't worry about this)
		public int id { get { return numericID; } }

		// static map from Values to short names, used when displaying lists/maps;
		// feel free to add to this any values (especially lists/maps) provided
		// by your own intrinsics.
		public static Dictionary<Value, string> shortNames = new Dictionary<Value, string>();

		private Function function;
		private ValFunction valFunction;	// (cached wrapper for function)
		int numericID;		// also its index in the 'all' list

		static List<Intrinsic> all = new List<Intrinsic>() { null };
		static Dictionary<string, Intrinsic> nameMap = new Dictionary<string, Intrinsic>();
		
		/// <summary>
		/// Factory method to create a new Intrinsic, filling out its name as given,
		/// and other internal properties as needed.  You'll still need to add any
		/// parameters, and define the code it runs.
		/// </summary>
		/// <param name="name">intrinsic name</param>
		/// <returns>freshly minted (but empty) static Intrinsic</returns>
		public static Intrinsic Create(string name) {
			Intrinsic result = new Intrinsic();
			result.name = name;
			result.numericID = all.Count;
			result.function = new Function(null);
			result.valFunction = new ValFunction(result.function);
			all.Add(result);
			nameMap[name] = result;
			return result;
		}
		
		/// <summary>
		/// Look up an Intrinsic by its internal numeric ID.
		/// </summary>
		public static Intrinsic GetByID(int id) {
			return all[id];
		}
		
		/// <summary>
		/// Look up an Intrinsic by its name.
		/// </summary>
		public static Intrinsic GetByName(string name) {
			Intrinsics.InitIfNeeded();
			Intrinsic result = null;
			if (nameMap.TryGetValue(name, out result)) return result;
			return null;
		}
		
		/// <summary>
		/// Add a parameter to this Intrinsic, optionally with a default value
		/// to be used if the user doesn't supply one.  You must add parameters
		/// in the same order in which arguments must be supplied.
		/// </summary>
		/// <param name="name">parameter name</param>
		/// <param name="defaultValue">default value, if any</param>
		public void AddParam(string name, Value defaultValue=null) {
			function.parameters.Add(new Function.Param(name, defaultValue));
		}
		
		/// <summary>
		/// Add a parameter with a numeric default value.  (See comments on
		/// the first version of AddParam above.)
		/// </summary>
		/// <param name="name">parameter name</param>
		/// <param name="defaultValue">default value for this parameter</param>
		public void AddParam(string name, double defaultValue) {
			Value defVal;
			if (defaultValue == 0) defVal = ValNumber.zero;
			else if (defaultValue == 1) defVal = ValNumber.one;
			else defVal = TAC.Num(defaultValue);
			function.parameters.Add(new Function.Param(name, defVal));
		}

		/// <summary>
		/// Add a parameter with a string default value.  (See comments on
		/// the first version of AddParam above.)
		/// </summary>
		/// <param name="name">parameter name</param>
		/// <param name="defaultValue">default value for this parameter</param>
		public void AddParam(string name, string defaultValue) {
			Value defVal;
			if (string.IsNullOrEmpty(defaultValue)) defVal = ValString.empty;
			else if (defaultValue == "__isa") defVal = ValString.magicIsA;
			else if (defaultValue == "self") defVal = _self;
			else defVal = new ValString(defaultValue);
			function.parameters.Add(new Function.Param(name, defVal));
		}
		ValString _self = new ValString("self");
		
		/// <summary>
		/// GetFunc is used internally by the compiler to get the MiniScript function
		/// that makes an intrinsic call.
		/// </summary>
		public ValFunction GetFunc() {
			if (function.code == null) {
				// Our little wrapper function is a single opcode: CallIntrinsicA.
				// It really exists only to provide a local variable context for the parameters.
				function.code = new List<TAC.Line>();
				function.code.Add(new TAC.Line(TAC.LTemp(0), TAC.Line.Op.CallIntrinsicA, TAC.Num(numericID)));
			}
			return valFunction;
		}
		
		/// <summary>
		/// Internally-used function to execute an intrinsic (by ID) given a
		/// context and a partial result.
		/// </summary>
		public static Result Execute(int id, TAC.Context context, Result partialResult) {
			Intrinsic item = GetByID(id);
			return item.code(context, partialResult);
		}
		
		/// <summary>
		/// Result represents the result of an intrinsic call.  An intrinsic will either
		/// be done with its work, or not yet done (e.g. because it's waiting for something).
		/// If it's done, set done=true, and store the result Value in result.
		/// If it's not done, set done=false, and store any partial result in result (and 
		/// then your intrinsic will get invoked with this Result passed in as partialResult).
		/// </summary>
		public class Result {
			public bool done;		// true if our work is complete; false if we need to Continue
			public Value result;	// final result if done; in-progress data if not done
			
			/// <summary>
			/// Result constructor taking a Value, and an optional done flag.
			/// </summary>
			/// <param name="result">result or partial result of the call</param>
			/// <param name="done">whether our work is done (optional, defaults to true)</param>
			public Result(Value result, bool done=true) {
				this.done = done;
				this.result = result;
			}

			/// <summary>
			/// Result constructor for a simple numeric result.
			/// </summary>
			public Result(double resultNum) {
				this.done = true;
				this.result = new ValNumber(resultNum);
			}

			/// <summary>
			/// Result constructor for a simple string result.
			/// </summary>
			public Result(string resultStr) {
				this.done = true;
				if (string.IsNullOrEmpty(resultStr)) this.result = ValString.empty;
				else this.result = new ValString(resultStr);
			}
			
			/// <summary>
			/// Result.Null: static Result representing null (no value).
			/// </summary>
			public static Result Null { get { return _null; } }
			static Result _null = new Result(null, true);
			
			/// <summary>
			/// Result.EmptyString: static Result representing "" (empty string).
			/// </summary>
			public static Result EmptyString { get { return _emptyString; } }
			static Result _emptyString = new Result(ValString.empty);
			
			/// <summary>
			/// Result.True: static Result representing true (1.0).
			/// </summary>
			public static Result True { get { return _true; } }
			static Result _true = new Result(ValNumber.one, true);
			
			/// <summary>
			/// Result.True: static Result representing false (0.0).
			/// </summary>
			public static Result False { get { return _false; } }
			static Result _false = new Result(ValNumber.zero, true);
			
			/// <summary>
			/// Result.Waiting: static Result representing a need to wait,
			/// with no in-progress value.
			/// </summary>
			public static Result Waiting { get { return _waiting; } }
			static Result _waiting = new Result(null, false);
		}
	}
	
	/// <summary>
	/// Intrinsics: a static class containing all of the standard MiniScript
	/// built-in intrinsics.  You shouldn't muck with these, but feel free
	/// to browse them for lots of examples of how to write your own intrinics.
	/// </summary>
	public static class Intrinsics {

		static bool initialized;
	
		private struct KeyedValue {
			public Value sortKey;
			public Value value;
			//public long valueIndex;
		}
		
	
		/// <summary>
		/// InitIfNeeded: called automatically during script setup to make sure
		/// that all our standard intrinsics are defined.  Note how we use a
		/// private bool flag to ensure that we don't create our intrinsics more
		/// than once, no matter how many times this method is called.
		/// </summary>
		public static void InitIfNeeded() {
			if (initialized) return;	// our work is already done; bail out.
			initialized = true;
			Intrinsic f;

			// abs(x)
			f = Intrinsic.Create("abs");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Abs(context.GetVar("x").DoubleValue()));
			};

			// acos(x)
			f = Intrinsic.Create("acos");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Acos(context.GetVar("x").DoubleValue()));
			};

			// asin(x)
			f = Intrinsic.Create("asin");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Asin(context.GetVar("x").DoubleValue()));
			};

			// atan(y, x=1)
			f = Intrinsic.Create("atan");
			f.AddParam("y", 0);
			f.AddParam("x", 1);
			f.code = (context, partialResult) => {
				double y = context.GetVar("y").DoubleValue();
				double x = context.GetVar("x").DoubleValue();
				if (x == 1.0) return new Intrinsic.Result(Math.Atan(y));
				return new Intrinsic.Result(Math.Atan2(y, x));
			};
			
			// char(i)
			f = Intrinsic.Create("char");
			f.AddParam("codePoint", 65);
			f.code = (context, partialResult) => {
				int codepoint = context.GetVar("codePoint").IntValue();
				string s = char.ConvertFromUtf32(codepoint);
				return new Intrinsic.Result(s);
			};
			
			// ceil(x)
			f = Intrinsic.Create("ceil");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Ceiling(context.GetVar("x").DoubleValue()));
			};
			
			// code(s)
			f = Intrinsic.Create("code");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				int codepoint = 0;
				if (self != null) codepoint = char.ConvertToUtf32(self.ToString(), 0);
				return new Intrinsic.Result(codepoint);
			};
						
			// cos(radians)
			f = Intrinsic.Create("cos");
			f.AddParam("radians", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Cos(context.GetVar("radians").DoubleValue()));
			};

			// floor(x)
			f = Intrinsic.Create("floor");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Floor(context.GetVar("x").DoubleValue()));
			};

			// funcRef
			f = Intrinsic.Create("funcRef");
			f.code = (context, partialResult) => {
				if (context.vm.functionType == null) {
					context.vm.functionType = FunctionType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.functionType);
			};
			
			// hash
			f = Intrinsic.Create("hash");
			f.AddParam("obj");
			f.code = (context, partialResult) => {
				Value val = context.GetVar("obj");
				return new Intrinsic.Result(val.Hash());
			};

			// hasIndex
			f = Intrinsic.Create("hasIndex");
			f.AddParam("self");
			f.AddParam("index");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				Value index = context.GetVar("index");
				if (self is ValList) {
					if (!(index is ValNumber)) return Intrinsic.Result.False;	// #3
					List<Value> list = ((ValList)self).values;
					int i = index.IntValue();
					return new Intrinsic.Result(ValNumber.Truth(i >= -list.Count && i < list.Count));
				} else if (self is ValString) {
					string str = ((ValString)self).value;
					int i = index.IntValue();
					return new Intrinsic.Result(ValNumber.Truth(i >= -str.Length && i < str.Length));
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					return new Intrinsic.Result(ValNumber.Truth(map.ContainsKey(index)));
				}
				return Intrinsic.Result.Null;
			};
			
			// indexes
			//	Returns the keys of a dictionary, or the indexes for a string or list.
			f = Intrinsic.Create("indexes");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				if (self is ValMap) {
					ValMap map = (ValMap)self;
					List<Value> keys = new List<Value>(map.map.Keys);
					return new Intrinsic.Result(new ValList(keys));
				} else if (self is ValString) {
					string str = ((ValString)self).value;
					List<Value> indexes = new List<Value>(str.Length);
					for (int i = 0; i < str.Length; i++) {
						indexes.Add(TAC.Num(i));
					}
					return new Intrinsic.Result(new ValList(indexes));
				} else if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					List<Value> indexes = new List<Value>(list.Count);
					for (int i = 0; i < list.Count; i++) {
						indexes.Add(TAC.Num(i));
					}
					return new Intrinsic.Result(new ValList(indexes));
				}
				return Intrinsic.Result.Null;
			};
			
			// indexOf
			//	Returns index or key of the given value, or if not found, returns null.
			f = Intrinsic.Create("indexOf");
			f.AddParam("self");
			f.AddParam("value");
			f.AddParam("after");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				Value value = context.GetVar("value");
				Value after = context.GetVar("after");
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					int idx;
					if (after == null) idx = list.FindIndex(x => 
						x == null ? value == null : x.Equality(value) == 1);
					else {
						int afterIdx = after.IntValue();
						if (afterIdx < -1) afterIdx += list.Count;
						if (afterIdx < -1 || afterIdx >= list.Count-1) return Intrinsic.Result.Null;
						idx = list.FindIndex(afterIdx + 1, x => 
							x == null ? value == null : x.Equality(value) == 1);
					}
					if (idx >= 0) return new Intrinsic.Result(idx);
				} else if (self is ValString) {
					string str = ((ValString)self).value;
					string s = value.ToString();
					int idx;
					if (after == null) idx = str.IndexOf(s);
					else {
						int afterIdx = after.IntValue();
						if (afterIdx < -1) afterIdx += str.Length;
						if (afterIdx < -1 || afterIdx >= str.Length-1) return Intrinsic.Result.Null;
						idx = str.IndexOf(s, afterIdx + 1);
					}
					if (idx >= 0) return new Intrinsic.Result(idx);
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					bool sawAfter = (after == null);
					foreach (Value k in map.map.Keys) {
						if (!sawAfter) {
							if (k.Equality(after) == 1) sawAfter = true;
						} else {
							if (map.map[k].Equality(value) == 1) return new Intrinsic.Result(k);
						}
					}
				}
				return Intrinsic.Result.Null;
			};

			// insert
			f = Intrinsic.Create("insert");
			f.AddParam("self");
			f.AddParam("index");
			f.AddParam("value");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				Value index = context.GetVar("index");
				Value value = context.GetVar("value");
				if (index == null) throw new RuntimeException("insert: index argument required");
				if (!(index is ValNumber)) throw new RuntimeException("insert: number required for index argument");
				int idx = index.IntValue();
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					if (idx < 0) idx += list.Count + 1;	// +1 because we are inserting AND counting from the end.
					Check.Range(idx, 0, list.Count);	// and allowing all the way up to .Count here, because insert.
					list.Insert(idx, value);
					return new Intrinsic.Result(self);
				} else if (self is ValString) {
					string s = self.ToString();
					if (idx < 0) idx += s.Length + 1;
					Check.Range(idx, 0, s.Length);
					s = s.Substring(0, idx) + value.ToString() + s.Substring(idx);
					return new Intrinsic.Result(s);
				} else {
					throw new RuntimeException("insert called on invalid type");
				}
			};


			// self.join
			f = Intrinsic.Create("join");
			f.AddParam("self");
			f.AddParam("delimiter", " ");
			f.code = (context, partialResult) => {
				Value val = context.GetVar("self");
				string delim = context.GetVar("delimiter").ToString();
				if (!(val is ValList)) return new Intrinsic.Result(val);
				ValList src = (val as ValList);
				List<string> list = new List<string>(src.values.Count);
				for (int i=0; i<src.values.Count; i++) {
					if (src.values[i] == null) list.Add(null);
					else list.Add(src.values[i].ToString());
				}
				string result = string.Join(delim, list.ToArray());
				return new Intrinsic.Result(result);
			};
			
			// self.len
			f = Intrinsic.Create("len");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.GetVar("self");
				if (val is ValList) {
					List<Value> list = ((ValList)val).values;
					return new Intrinsic.Result(list.Count);
				} else if (val is ValString) {
					string str = ((ValString)val).value;
					return new Intrinsic.Result(str.Length);
				} else if (val is ValMap) {
					return new Intrinsic.Result(((ValMap)val).Count);
				}
				return Intrinsic.Result.Null;
			};
			
			// list type
			f = Intrinsic.Create("list");
			f.code = (context, partialResult) => {
				if (context.vm.listType == null) {
					context.vm.listType = ListType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.listType);
			};
			
			// log(x, base)
			f = Intrinsic.Create("log");
			f.AddParam("x", 0);
			f.AddParam("base", 10);
			f.code = (context, partialResult) => {
				double x = context.GetVar("x").DoubleValue();
				double b = context.GetVar("base").DoubleValue();
				double result;
				if (Math.Abs(b - 2.718282) < 0.000001) result = Math.Log(x);
				else result = Math.Log(x) / Math.Log(b);
				return new Intrinsic.Result(result);
			};
			
			// s.lower
			f = Intrinsic.Create("lower");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.GetVar("self");
				if (val is ValString) {
					string str = ((ValString)val).value;
					return new Intrinsic.Result(str.ToLower());
				}
				return new Intrinsic.Result(val);
			};

			// map type
			f = Intrinsic.Create("map");
			f.code = (context, partialResult) => {
				if (context.vm.mapType == null) {
					context.vm.mapType = MapType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.mapType);
			};
			
			// number type
			f = Intrinsic.Create("number");
			f.code = (context, partialResult) => {
				if (context.vm.numberType == null) {
					context.vm.numberType = NumberType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.numberType);
			};
			
			// pi
			f = Intrinsic.Create("pi");
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.PI);
			};

			// print(s)
			f = Intrinsic.Create("print");
			f.AddParam("s", ValString.empty);
			f.code = (context, partialResult) => {
				Value s = context.GetVar("s");
				if (s != null) context.vm.standardOutput(s.ToString());
				else context.vm.standardOutput("null");
				return Intrinsic.Result.Null;
			};
				
			// self.pop(x)
			//	removes and returns the last item in a list (or arbitrary key of a map)
			f = Intrinsic.Create("pop");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					if (list.Count < 1) return Intrinsic.Result.Null;
					Value result = list[list.Count-1];
					list.RemoveAt(list.Count-1);
					return new Intrinsic.Result(result);
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					if (map.map.Count < 1) return Intrinsic.Result.Null;
					Value result = map.map.Keys.First();
					map.map.Remove(result);
					return new Intrinsic.Result(result);
				}
				return Intrinsic.Result.Null;
			};

			// self.pull(x)
			//	removes and returns the first item in a list (or arbitrary key of a map)
			f = Intrinsic.Create("pull");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					if (list.Count < 1) return Intrinsic.Result.Null;
					Value result = list[0];
					list.RemoveAt(0);
					return new Intrinsic.Result(result);
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					if (map.map.Count < 1) return Intrinsic.Result.Null;
					Value result = map.map.Keys.First();
					map.map.Remove(result);
					return new Intrinsic.Result(result);
				}
				return Intrinsic.Result.Null;
			};

			// self.push(x)
			//	appends an item to a list (or inserts in a map); returns self
			f = Intrinsic.Create("push");
			f.AddParam("self");
			f.AddParam("value");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				Value value = context.GetVar("value");
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					list.Add(value);
					return new Intrinsic.Result(self);
				} else if (self is ValMap) {
					ValMap map = (ValMap)self;
					map.map[value] = ValNumber.one;
					return new Intrinsic.Result(self);
				}
				return Intrinsic.Result.Null;
			};

			// range(from, to, step)
			f = Intrinsic.Create("range");
			f.AddParam("from", 0);
			f.AddParam("to", 0);
			f.AddParam("step");
			f.code = (context, partialResult) => {
				Value p0 = context.GetVar("from");
				Value p1 = context.GetVar("to");
				Value p2 = context.GetVar("step");
				double fromVal = p0.DoubleValue();
				double toVal = p1.DoubleValue();
				double step = (toVal >= fromVal ? 1 : -1);
				if (p2 is ValNumber) step = (p2 as ValNumber).value;
				if (step == 0) throw new RuntimeException("range() error (step==0)");
				List<Value> values = new List<Value>();
				int count = (int)((toVal - fromVal) / step) + 1;
				if (count > ValList.maxSize) throw new RuntimeException("list too large");
				try {
					values = new List<Value>(count);
					for (double v = fromVal; step > 0 ? (v <= toVal) : (v >= toVal); v += step) {
						values.Add(TAC.Num(v));
					}
				} catch (SystemException e) {
					// uh-oh... probably out-of-memory exception; clean up and bail out
					values = null;
					throw(new LimitExceededException("range() error", e));
				}
				return new Intrinsic.Result(new ValList(values));
			};

			// remove(self, key or index or substring)
			// 		list: mutated in place, returns null, error if index out of range
			//		map: mutated in place; returns 1 if key found, 0 otherwise
			//		string: returns new string with first occurrence of k removed
			f = Intrinsic.Create("remove");
			f.AddParam("self");
			f.AddParam("k");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				Value k = context.GetVar("k");
				if (self == null || k == null) throw new RuntimeException("argument to 'remove' must not be null");
				if (self is ValMap) {
					ValMap selfMap = (ValMap)self;
					if (selfMap.map.ContainsKey(k)) {
						selfMap.map.Remove(k);
						return new Intrinsic.Result(ValNumber.one);
					}
					return new Intrinsic.Result(ValNumber.zero);
				} else if (self is ValList) {
					ValList selfList = (ValList)self;
					int idx = k.IntValue();
					if (idx < 0) idx += selfList.values.Count;
					Check.Range(idx, 0, selfList.values.Count-1);
					selfList.values.RemoveAt(idx);
					return Intrinsic.Result.Null;
				} else if (self is ValString) {
					ValString selfStr = (ValString)self;
					string substr = k.ToString();
					int foundPos = selfStr.value.IndexOf(substr);
					if (foundPos < 0) return new Intrinsic.Result(self);
					return new Intrinsic.Result(selfStr.value.Remove(foundPos, substr.Length));
				}
				throw new TypeException("Type Error: 'remove' requires map, list, or string");
			};

			// replace(self, value or substring, new value/substring)
			// 		list: mutated in place, returns self
			//		map: mutated in place; returns self
			//		string: returns new string with occurrences of oldval replaced
			f = Intrinsic.Create("replace");
			f.AddParam("self");
			f.AddParam("oldval");
			f.AddParam("newval");
			f.AddParam("maxCount");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				if (self == null) throw new RuntimeException("argument to 'replace' must not be null");
				Value oldval = context.GetVar("oldval");
				Value newval = context.GetVar("newval");
				Value maxCountVal = context.GetVar("maxCount");
				int maxCount = -1;
				if (maxCountVal != null) {
					maxCount = maxCountVal.IntValue();
					if (maxCount < 1) return new Intrinsic.Result(self);
				}
				int count = 0;
				if (self is ValMap) {
					ValMap selfMap = (ValMap)self;
					// C# doesn't allow changing even the values while iterating
					// over the keys.  So gather the keys to change, then change
					// them afterwards.
					List<Value> keysToChange = null;
					foreach (Value k in selfMap.map.Keys) {
						if (selfMap.map[k].Equality(oldval) == 1) {
							if (keysToChange == null) keysToChange = new List<Value>();
							keysToChange.Add(k);
							count++;
							if (maxCount > 0 && count == maxCount) break;
						}
					}
					if (keysToChange != null) foreach (Value k in keysToChange) {
						selfMap.map[k] = newval;
					}
					return new Intrinsic.Result(self);
				} else if (self is ValList) {
					ValList selfList = (ValList)self;
					int idx = -1;
					while (true) {
						idx = selfList.values.FindIndex(idx+1, x => x.Equality(oldval) == 1);
						if (idx < 0) break;
						selfList.values[idx] = newval;
						count++;
						if (maxCount > 0 && count == maxCount) break;
					}
					return new Intrinsic.Result(self);
				} else if (self is ValString) {
					string str = self.ToString();
					string oldstr = oldval.ToString();
					string newstr = newval.ToString();
					int idx = 0;
					while (true) {
						idx = str.IndexOf(oldstr, idx);
						if (idx < 0) break;
						str = str.Substring(0, idx) + newstr + str.Substring(idx + oldstr.Length);
						idx += newstr.Length;
						count++;
						if (maxCount > 0 && count == maxCount) break;
					}
					return new Intrinsic.Result(str);
				}
				throw new TypeException("Type Error: 'replace' requires map, list, or string");
			};

			// round(x, decimalPlaces)
			f = Intrinsic.Create("round");
			f.AddParam("x", 0);
			f.AddParam("decimalPlaces", 0);
			f.code = (context, partialResult) => {
				double num = context.GetVar("x").DoubleValue();
				int decimalPlaces = context.GetVar("decimalPlaces").IntValue();
				return new Intrinsic.Result(Math.Round(num, decimalPlaces));
			};


			// rnd(seed)
			f = Intrinsic.Create("rnd");
			f.AddParam("seed");
			f.code = (context, partialResult) => {
				if (random == null) random = new Random();
				Value seed = context.GetVar("seed");
				if (seed != null) random = new Random(seed.IntValue());
				return new Intrinsic.Result(random.NextDouble());
			};

			// sign(x)
			f = Intrinsic.Create("sign");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Sign(context.GetVar("x").DoubleValue()));
			};

			// sin(radians)
			f = Intrinsic.Create("sin");
			f.AddParam("radians", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Sin(context.GetVar("radians").DoubleValue()));
			};
				
			// slice(seq, from, to)
			f = Intrinsic.Create("slice");
			f.AddParam("seq");
			f.AddParam("from", 0);
			f.AddParam("to");
			f.code = (context, partialResult) => {
				Value seq = context.GetVar("seq");
				int fromIdx = context.GetVar("from").IntValue();
				Value toVal = context.GetVar("to");
				int toIdx = 0;
				if (toVal != null) toIdx = toVal.IntValue();
				if (seq is ValList) {
					List<Value> list = ((ValList)seq).values;
					if (fromIdx < 0) fromIdx += list.Count;
					if (fromIdx < 0) fromIdx = 0;
					if (toVal == null) toIdx = list.Count;
					if (toIdx < 0) toIdx += list.Count;
					if (toIdx > list.Count) toIdx = list.Count;
					ValList slice = new ValList();
					if (fromIdx < list.Count && toIdx > fromIdx) {
						for (int i = fromIdx; i < toIdx; i++) {
							slice.values.Add(list[i]);
						}
					}
					return new Intrinsic.Result(slice);
				} else if (seq is ValString) {
					string str = ((ValString)seq).value;
					if (fromIdx < 0) fromIdx += str.Length;
					if (fromIdx < 0) fromIdx = 0;
					if (toVal == null) toIdx = str.Length;
					if (toIdx < 0) toIdx += str.Length;
					if (toIdx > str.Length) toIdx = str.Length;
					if (toIdx - fromIdx <= 0) return Intrinsic.Result.EmptyString;
					return new Intrinsic.Result(str.Substring(fromIdx, toIdx - fromIdx));
				}
				return Intrinsic.Result.Null;
			};
			
			// list.sort(byKey=null)
			f = Intrinsic.Create("sort");
			f.AddParam("self");
			f.AddParam("byKey");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				ValList list = self as ValList;
				if (list == null || list.values.Count < 2) return new Intrinsic.Result(self);
				
				Value byKey = context.GetVar("byKey");
				if (byKey == null) {
					// Simple case: sort the values as themselves
					list.values = list.values.OrderBy((arg) => arg, ValueSorter.instance).ToList();
				} else {
					// Harder case: sort by a key.
					int count = list.values.Count;
					KeyedValue[] arr = new KeyedValue[count];
					for (int i=0; i<count; i++) {
						arr[i].value = list.values[i];
						//arr[i].valueIndex = i;
					}
					// The key for each item will be the item itself, unless it is a map, in which
					// case it's the item indexed by the given key.  (Works too for lists if our
					// index is an integer.)
					int byKeyInt = byKey.IntValue();
					for (int i=0; i<count; i++) {
						Value item = list.values[i];
						if (item is ValMap) arr[i].sortKey = ((ValMap)item).Lookup(byKey);
						else if (item is ValList) {
							ValList itemList = (ValList)item;
							if (byKeyInt > -itemList.values.Count && byKeyInt < itemList.values.Count) arr[i].sortKey = itemList.values[byKeyInt];
							else arr[i].sortKey = null;
						}
					}
					// Now sort our list of keyed values, by key
					var sortedArr = arr.OrderBy((arg) => arg.sortKey, ValueSorter.instance);
					// And finally, convert that back into our list
					int idx=0;
					foreach (KeyedValue kv in sortedArr) {
						list.values[idx++] = kv.value;
					}
				}
				return new Intrinsic.Result(list);
			};

			// split(self, delimiter, maxCount)
			f = Intrinsic.Create("split");
			f.AddParam("self");
			f.AddParam("delimiter", " ");
			f.AddParam("maxCount", -1);
			f.code = (context, partialResult) => {
				string self = context.GetVar("self").ToString();
				string delim = context.GetVar("delimiter").ToString();
				int maxCount = context.GetVar("maxCount").IntValue();
				ValList result = new ValList();
				int pos = 0;
				while (pos < self.Length) {
					int nextPos;
					if (maxCount >= 0 && result.values.Count == maxCount - 1) nextPos = self.Length;
					else if (delim.Length == 0) nextPos = pos+1;
					else nextPos = self.IndexOf(delim, pos, StringComparison.InvariantCulture);
					if (nextPos < 0) nextPos = self.Length;
					result.values.Add(new ValString(self.Substring(pos, nextPos - pos)));
					pos = nextPos + delim.Length;
					if (pos == self.Length && delim.Length > 0) result.values.Add(ValString.empty);
				}
				return new Intrinsic.Result(result);
			};

			// str(x)
			// sqrt(x)
			f = Intrinsic.Create("sqrt");
			f.AddParam("x", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Sqrt(context.GetVar("x").DoubleValue()));
			};

			// str(x)
			f = Intrinsic.Create("str");
			f.AddParam("x", ValString.empty);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(context.GetVar("x").ToString());
			};

			// string type
			f = Intrinsic.Create("string");
			f.code = (context, partialResult) => {
				if (context.vm.stringType == null) {
					context.vm.stringType = StringType().EvalCopy(context.vm.globalContext);
				}
				return new Intrinsic.Result(context.vm.stringType);
			};

			// shuffle(self)
			f = Intrinsic.Create("shuffle");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value self = context.GetVar("self");
				if (random == null) random = new Random();
				if (self is ValList) {
					List<Value> list = ((ValList)self).values;
					// We'll do a Fisher-Yates shuffle, i.e., swap each element
					// with a randomly selected one.
					for (int i=list.Count-1; i >= 1; i--) {
						int j = random.Next(i+1);
						Value temp = list[j];
						list[j] = list[i];
						list[i] = temp;
					}
				} else if (self is ValMap) {
					Dictionary<Value, Value> map = ((ValMap)self).map;
					// Fisher-Yates again, but this time, what we're swapping
					// is the values associated with the keys, not the keys themselves.
					List<Value> keys = System.Linq.Enumerable.ToList(map.Keys);
					for (int i=keys.Count-1; i >= 1; i--) {
						int j = random.Next(i+1);
						Value keyi = keys[i];
						Value keyj = keys[j];
						Value temp = map[keyj];
						map[keyj] = map[keyi];
						map[keyi] = temp;
					}
				}
				return Intrinsic.Result.Null;
			};

			// sum(self)
			f = Intrinsic.Create("sum");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.GetVar("self");
				double sum = 0;
				if (val is ValList) {
					List<Value> list = ((ValList)val).values;
					foreach (Value v in list) {
						sum += v.DoubleValue();
					}
				} else if (val is ValMap) {
					Dictionary<Value, Value> map = ((ValMap)val).map;
					foreach (Value v in map.Values) {
						sum += v.DoubleValue();
					}
				}
				return new Intrinsic.Result(sum);
			};

			// tan(radians)
			f = Intrinsic.Create("tan");
			f.AddParam("radians", 0);
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(Math.Tan(context.GetVar("radians").DoubleValue()));
			};

			// time
			f = Intrinsic.Create("time");
			f.code = (context, partialResult) => {
				return new Intrinsic.Result(context.vm.runTime);
			};
			
			// s.upper
			f = Intrinsic.Create("upper");
			f.AddParam("self");
			f.code = (context, partialResult) => {
				Value val = context.GetVar("self");
				if (val is ValString) {
					string str = ((ValString)val).value;
					return new Intrinsic.Result(str.ToUpper());
				}
				return new Intrinsic.Result(val);
			};
			
			// val(s)
			f = Intrinsic.Create("val");
			f.AddParam("self", 0);
			f.code = (context, partialResult) => {
				Value val = context.GetVar("self");
				if (val is ValNumber) return new Intrinsic.Result(val);
				if (val is ValString) {
					double value = 0;
					double.TryParse(val.ToString(), NumberStyles.Any, CultureInfo.InvariantCulture, out value);
					return new Intrinsic.Result(value);
				}
				return Intrinsic.Result.Null;
			};

            // values
            //  Returns the values of a dictionary, or the characters of a string.
            //  (Returns any other value as-is.)
            f = Intrinsic.Create("values");
            f.AddParam("self");
            f.code = (context, partialResult) => {
                Value self = context.GetVar("self");
                if (self is ValMap) {
                    ValMap map = (ValMap)self;
                    List<Value> values = new List<Value>(map.map.Values);
                    return new Intrinsic.Result(new ValList(values));
                } else if (self is ValString) {
                    string str = ((ValString)self).value;
                    List<Value> values = new List<Value>(str.Length);
                    for (int i = 0; i < str.Length; i++) {
                        values.Add(TAC.Str(str[i].ToString()));
                    }
                    return new Intrinsic.Result(new ValList(values));
                }
                return new Intrinsic.Result(self);
            };

			// version
			f = Intrinsic.Create("version");
			f.code = (context, partialResult) => {
				if (context.vm.versionMap == null) {
					var d = new ValMap();
					d["miniscript"] = new ValString("1.5");
			
					// Getting the build date is annoyingly hard in C#.
					// This will work if the assembly.cs file uses the version format: 1.0.*
					DateTime buildDate;
					System.Version version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
					buildDate = new DateTime(2000, 1, 1);
					buildDate = buildDate.AddDays(version.Build);
					buildDate = buildDate.AddSeconds(version.Revision * 2);
					d["buildDate"] = new ValString(buildDate.ToString("yyyy-MM-dd"));

					d["host"] = new ValNumber(HostInfo.version);
					d["hostName"] = new ValString(HostInfo.name);
					d["hostInfo"] = new ValString(HostInfo.info);

					context.vm.versionMap = d;
				}
				return new Intrinsic.Result(context.vm.versionMap);
			};

			// wait(seconds)
			f = Intrinsic.Create("wait");
			f.AddParam("seconds", 1);
			f.code = (context, partialResult) => {
				double now = context.vm.runTime;
				if (partialResult == null) {
					// Just starting our wait; calculate end time and return as partial result
					double interval = context.GetVar("seconds").DoubleValue();
					return new Intrinsic.Result(new ValNumber(now + interval), false);
				} else {
					// Continue until current time exceeds the time in the partial result
					if (now > partialResult.result.DoubleValue()) return Intrinsic.Result.Null;
					return partialResult;
				}
			};

			// yield
			f = Intrinsic.Create("yield");
			f.code = (context, partialResult) => {
				context.vm.yielding = true;
				return Intrinsic.Result.Null;
			};

		}

		static Random random;	// TODO: consider storing this on the context, instead of global!


		// Helper method to compile a call to Slice (when invoked directly via slice syntax).
		public static void CompileSlice(List<TAC.Line> code, Value list, Value fromIdx, Value toIdx, int resultTempNum) {
			code.Add(new TAC.Line(null, TAC.Line.Op.PushParam, list));
			code.Add(new TAC.Line(null, TAC.Line.Op.PushParam, fromIdx == null ? TAC.Num(0) : fromIdx));
			code.Add(new TAC.Line(null, TAC.Line.Op.PushParam, toIdx));// toIdx == null ? TAC.Num(0) : toIdx));
			ValFunction func = Intrinsic.GetByName("slice").GetFunc();
			code.Add(new TAC.Line(TAC.LTemp(resultTempNum), TAC.Line.Op.CallFunctionA, func, TAC.Num(3)));
		}
		
		/// <summary>
		/// FunctionType: a static map that represents the Function type.
		/// </summary>
		public static ValMap FunctionType() {
			if (_functionType == null) {
				_functionType = new ValMap();
			}
			return _functionType;
		}
		static ValMap _functionType = null;
		
		/// <summary>
		/// ListType: a static map that represents the List type, and provides
		/// intrinsic methods that can be invoked on it via dot syntax.
		/// </summary>
		public static ValMap ListType() {
			if (_listType == null) {
				_listType = new ValMap();
				_listType["hasIndex"] = Intrinsic.GetByName("hasIndex").GetFunc();
				_listType["indexes"] = Intrinsic.GetByName("indexes").GetFunc();
				_listType["indexOf"] = Intrinsic.GetByName("indexOf").GetFunc();
				_listType["insert"] = Intrinsic.GetByName("insert").GetFunc();
				_listType["join"] = Intrinsic.GetByName("join").GetFunc();
				_listType["len"] = Intrinsic.GetByName("len").GetFunc();
				_listType["pop"] = Intrinsic.GetByName("pop").GetFunc();
				_listType["pull"] = Intrinsic.GetByName("pull").GetFunc();
				_listType["push"] = Intrinsic.GetByName("push").GetFunc();
				_listType["shuffle"] = Intrinsic.GetByName("shuffle").GetFunc();
				_listType["sort"] = Intrinsic.GetByName("sort").GetFunc();
				_listType["sum"] = Intrinsic.GetByName("sum").GetFunc();
                _listType["remove"] = Intrinsic.GetByName("remove").GetFunc();
                _listType["replace"] = Intrinsic.GetByName("replace").GetFunc();
                _listType["values"] = Intrinsic.GetByName("values").GetFunc();
			}
			return _listType;
		}
		static ValMap _listType = null;
		
		/// <summary>
		/// StringType: a static map that represents the String type, and provides
		/// intrinsic methods that can be invoked on it via dot syntax.
		/// </summary>
		public static ValMap StringType() {
			if (_stringType == null) {
				_stringType = new ValMap();
				_stringType["hasIndex"] = Intrinsic.GetByName("hasIndex").GetFunc();
				_stringType["indexes"] = Intrinsic.GetByName("indexes").GetFunc();
				_stringType["indexOf"] = Intrinsic.GetByName("indexOf").GetFunc();
				_stringType["insert"] = Intrinsic.GetByName("insert").GetFunc();
				_stringType["code"] = Intrinsic.GetByName("code").GetFunc();
				_stringType["len"] = Intrinsic.GetByName("len").GetFunc();
				_stringType["lower"] = Intrinsic.GetByName("lower").GetFunc();
				_stringType["val"] = Intrinsic.GetByName("val").GetFunc();
				_stringType["remove"] = Intrinsic.GetByName("remove").GetFunc();
                _stringType["replace"] = Intrinsic.GetByName("replace").GetFunc();
				_stringType["split"] = Intrinsic.GetByName("split").GetFunc();
                _stringType["upper"] = Intrinsic.GetByName("upper").GetFunc();
                _stringType["values"] = Intrinsic.GetByName("values").GetFunc();
			}
			return _stringType;
		}
		static ValMap _stringType = null;
		
		/// <summary>
		/// MapType: a static map that represents the Map type, and provides
		/// intrinsic methods that can be invoked on it via dot syntax.
		/// </summary>
		public static ValMap MapType() {
			if (_mapType == null) {
				_mapType = new ValMap();
				_mapType["hasIndex"] = Intrinsic.GetByName("hasIndex").GetFunc();
				_mapType["indexes"] = Intrinsic.GetByName("indexes").GetFunc();
				_mapType["indexOf"] = Intrinsic.GetByName("indexOf").GetFunc();
				_mapType["len"] = Intrinsic.GetByName("len").GetFunc();
				_mapType["pop"] = Intrinsic.GetByName("pop").GetFunc();
				_mapType["push"] = Intrinsic.GetByName("push").GetFunc();
				_mapType["shuffle"] = Intrinsic.GetByName("shuffle").GetFunc();
				_mapType["sum"] = Intrinsic.GetByName("sum").GetFunc();
                _mapType["remove"] = Intrinsic.GetByName("remove").GetFunc();
                _mapType["replace"] = Intrinsic.GetByName("replace").GetFunc();
                _mapType["values"] = Intrinsic.GetByName("values").GetFunc();
			}
			return _mapType;
		}
		static ValMap _mapType = null;
		
		/// <summary>
		/// NumberType: a static map that represents the Number type.
		/// </summary>
		public static ValMap NumberType() {
			if (_numberType == null) {
				_numberType = new ValMap();
			}
			return _numberType;
		}
		static ValMap _numberType = null;
		
		
	}
}

