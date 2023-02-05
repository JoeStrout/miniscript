// Map Utilities.
//	This module adds some additional methods to the `map` type.

// get: look up the value for an index in this map, but if the given
// index is not found, return a default value instead.  Works with
// inherited values, too.
map.get = function(key, defaultValue=null)
	m = self
	while m
		if m.hasIndex(key) then return m[key]
		if not m.hasIndex("__isa") then break
		m = m.__isa
	end while
	return defaultValue
end function

// hasValue: return true if this map contains the given value as
// a value (compare with built-in function .hasIndex, which checks
// whether this map has a given value as a key).  By default this
// does not walk the inheritance chain (just like .hasIndex), but
// if you pass `true` as the second parameter, then it does.
map.hasValue = function(v, includeInherited=false)
	if self.indexOf(v) != null then return true
	if includeInherited then
		m = self
		while m
			if not m.hasIndex("__isa") then break
			m = m.__isa
			if m.indexOf(v) != null then return true
		end while
	end if
	return false
end function

// sortedIndexes: get the indexes of this map, but in sorted order
// (rather than undefined order as .indexes gives you).
map.sortedIndexes = function
	result = self.indexes
	result.sort
	return result
end function

// inverse: return a map that has the keys and values swapped (keys
// of this map become values of the result map, and vice versa).
map.inverse = function
	result = {}
	for kv in self
		result[kv.value] = kv.key
	end for
	return result
end function

// filterIndexes: remove any indexes for which the given function
// does not return true.
map.filterIndexes = function(func)
	for k in self.indexes
		if not func(k) then self.remove k
	end for
end function

// filterValues: remove any key/value pairs for which the 
// function, applied to the value, does not return true.
map.filterValues = function(func)
	for k in self.indexes
		if not func(self[k]) then self.remove k
	end for
end function

// applyToValues: apply the given function to all values,
// in place.
map.applyToValues = function(func)
	for kv in self
		self[kv.key] = func(kv.value)
	end for
end function

// pairs: get all key/value pairs as a list (in arbitrary order)
// of little maps with "key" and "value", just like what you get
// when you iterate over a map with a `for` loop.
map.pairs = function
	result = []
	for kv in self
		result.push kv
	end for
	return result
end function

// swap: swap the values of two keys.  Note that this may be
// applied to locals (or globals), enabling you to swap the values
// of two variables.
map.swap = function(key1, key2)
	temp = self[key1]
	self[key1] = self[key2]
	self[key2] = temp
end function

// mapUtils.fromTo: generate a map from one sequence (list or string)
// to another.  The two sequences must be the same length.
fromTo = function(fromSeq, toSeq)
	if fromSeq.len != toSeq.len then
		print "mapUtils.fromTo: sequence lengths do not match (" + fromSeq.len + " and " + toSeq.len + ")"
		return null
	end if
	result = {}
	for i in fromSeq.indexes
		result[fromSeq[i]] = toSeq[i]
	end for
	return result
end function

runUnitTests = function
	print "Unit testing: mapUtil"
	
	errorCount = 0
	assertEqual = function(actual, expected, note)
		if actual != expected then
			print "Unit test failure (" + testing + "): expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function

	d = {"one":"ichi", "two":"ni", "three":"san", "four":"shi", "five":"go"}

	testing = "get"
	assertEqual d.get("one", 1), "ichi"
	assertEqual d.get("ten", 10), 10
	assertEqual d.get("twenty"), null
	
	testing = "hasValue"
	assertEqual d.hasValue("ni"), true
	assertEqual d.hasValue("foo"), false
	
	testing = "sortedIndexes"
	assertEqual d.sortedIndexes, ["five", "four", "one", "three", "two"] // (alphabetical)
	
	testing = "fromTo"
	assertEqual fromTo("abc", "xyz"), {"a":"x", "b":"y", "c":"z"}
	
	testing = "inverse"
	assertEqual d.inverse, {"ichi":"one", "ni":"two", "san":"three", "shi":"four", "go":"five"}

	testing = "filterIndexes"
	d.filterIndexes function(k)
		return k.len < 4		// only true for "one" and "two"
	end function
	assertEqual d.sortedIndexes, ["one", "two"]

	testing = "swap"
	d = {"one":"ichi", "two":"ni", "three":"san", "four":"shi", "five":"go"}
	d.swap "one", "two"
	assertEqual d.one, "ni"
	assertEqual d.two, "ichi"

	testing = "filterValues"
	d = {"one":"ichi", "two":"ni", "three":"san", "four":"shi", "five":"go"}
	d.filterValues function(v)
		return v.len < 3		// only true for "ni" and "go"
	end function
	assertEqual d.sortedIndexes, ["five", "two"]	// (keys for "go" and "ni")
	
	testing = "applyToValues"
	d = {"one":"ichi", "two":"ni"}
	d.applyToValues @upper
	assertEqual d.one, "ICHI"
	assertEqual d.two, "NI"
	d = {"one":"1", "two":"2"}
	d.applyToValues @val
	assertEqual d.one, 1
	assertEqual d.two, 2	
	
	if errorCount == 0 then
		print "All tests passed.  Boo-yeah!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if

end function

if globals == locals then runUnitTests
