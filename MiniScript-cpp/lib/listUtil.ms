// List Utilities.
//	This module adds some additional methods to the `list` type.
//	It includes some math functions which work on a 1D list of numbers.
//	(If you need to work with 2D arrays of numbers, you should probably
//	consider matrixUtil instead.)

// contains: return true if this list contains the given element.
list.contains = function(item)
	return self.indexOf(item) != null
end function

// split: similar to string.split, this splits a list into sub-lists
// by where the given delimiter element is found.
list.split = function(delimiter, maxCount=null)
	if maxCount == 1 then return [self]
	result = []
	p0 = 0
	while p0 < self.len
		p1 = self.indexOf(delimiter, p0-1)
		if p1 == null then
			result.push self[p0:]
			return result
		end if
		result.push self[p0:p1]
		p0 = p1 + 1
		if maxCount != null and result.len+1 == maxCount then
			result.push self[p0:]
			return result
		end if
	end while
end function

// deepcopy: clones this list, and if it contains sub-lists,
// clones those as well.
list.deepCopy = function
	result = self[:]
	for i in result.indexes
		if result[i] isa list then result[i] = result[i].deepCopy
	end for
	return result
end function

// add: add a number or string to all elements of a list (in place),
// OR do element-wise addition with another list of the same size.
//
// Example 1:
//		x = [1, 2, 3]
//		x.add 5   // x is now [6, 7, 8]
// Example 2:
//		x = [1, 2, 3]
//		x.add [10,100,10]	// x is now [11, 102, 13]
list.add = function(addend)
	if addend isa list then
		maxi = self.len - 1
		if addend.len - 1 < maxi then maxi = addend.len - 1
		for i in range(0,maxi)
			self[i] = self[i] + addend[i]
		end for
	else if addend isa string or addend isa number then
		for i in self.indexes
			self[i] = self[i] + addend
		end for
	else
		print "ERROR: list.add requires a number, string, or list."
		print "(Did you mean to use list.push instead?)"
		exit
	end if
end function

// plus: add a number to all elements of a list, OR do element-wise addition
// with another list of the same size, returning a new list (leaving this
// one untouched).
list.plus = function(addend)
	result = self[:]
	result.add addend
	return result
end function

// multiplyBy: multiply all elements of a list (in place) by a scalar factor,
// OR do element-wise multiplication with another list of the same size.
//
// Example 1:
//		x = [1, 2, 3]
//		x.multiplyBy 5   // x is now [5, 10, 15]
// Example 2:
//		x = [1, 2, 3]
//		x.multiplyBy [10,100,10]	// x is now [10, 200, 30]
list.multiplyBy = function(factor)
	if factor isa list then
		maxi = self.len - 1
		if factor.len - 1 < maxi then maxi = factor.len - 1
		for i in range(0,maxi)
			self[i] = self[i] * factor[i]
		end for
	else
		for i in self.indexes
			self[i] = self[i] * factor
		end for
	end if
end function

// times: multiply all elements of a list by a scalar factor, OR do
// element-wise multiplication with another list of the same size, returning
// a new list (leaving this one untouched).
//
// Example 1:
//		x = [1, 2, 3]
//		x.times 5   // returns[5, 10, 15]
// Example 2:
//		x = [1, 2, 3]
//		x.times [10,100,10]	// returns [10, 200, 30]
list.times = function(factor)
	result = self[:]
	result.multiplyBy factor
	return result
end function

// dot: return a scalar that is the sum of all the pairwise products
// between this list and another list of the same size.
list.dot = function(other)
	if other.len != self.len then return null
	sum = 0
	for i in self.indexes
		sum = sum + self[i] * other[i]
	end for
	return sum
end function

// reverse: reverse the order of elements of a list (in place).
// Example:
//		x = [1, 2, 3]
//		x.reverse		// x is now [3, 2, 1]
list.reverse = function
	if not self then return self
	for i in range(0, self.len/2-1)
	    negi = -1 - i
		temp = self[i]
		self[i] = self[negi]
		self[negi] = temp
	end for
end function

// reversed: return a new list that's a reversed version of this one
// Example:
//		x = [1, 2, 3]
//		x.reversed		// returns [3, 2, 1]
list.reversed = function
	result = self[:]
	result.reverse
	return result
end function

// apply: apply a function to every element of a list (in place).
// Remember to use @ to reference the function to apply!
// Example:
//		x = [1.1, 1.9, 3.45]
//		x.apply @round  // x is now [1, 2, 3]
list.apply = function(func)
	for i in self.indexes
		self[i] = func(self[i])
	end for
end function

// apply1: same as apply, but takes 1 extra argument.
list.apply1 = function(func, arg1)
	for i in self.indexes
		self[i] = func(self[i], arg1)
	end for
end function

// mean: return the average (sum divided by number of values)
list.mean = function
	return self.sum / self.len
end function

// counts: return a map that maps each unique element of this list,
// to the number of times that value occurs.
list.counts = function
	result = {}
	for item in self
		if result.hasIndex(item) then
			result[item] = result[item] + 1
		else
			result[item] = 1
		end if
	end for
	return result
end function

// distinct: return this list, with any duplicate entries removed.
// (Note that order of the result is not guaranteed.)
list.distinct = function
	return self.counts.indexes
end function

// product: return the result of multiplying up all values in the list.
list.product = function
	first = true
	for item in self
		if first then
			result = item
			first = false
		else
			result = result * item
		end if
	end for
	return result
end function

// min: return the minimum value of all elements in the list.
list.min = function
	first = true
	for item in self
		if first then
			result = item
			first = false
		else if item < result then
			result = item
		end if
	end for
	return result
end function

// max: return the maximum value of all elements in the list.
list.max = function
	first = true
	for item in self
		if first then
			result = item
			first = false
		else if item > result then
			result = item
		end if
	end for
	return result
end function

// reduce: apply a function that takes two arguments as follows:
//	first, apply it to the first two elements of self; then take
//	the result, and pass that in with the next element; and 
//	continue passing the result of each call along with the next
//	element until we have gone through the whole list.  Depending
//	on the function you provide, this could calculate a sum, or
//	a min or max, or lots of other possibilities.
list.reduce = function(func)
	if self.len == 0 then return func(null, null)
	if self.len == 1 then return func(self[0], null)
	result = func(self[0], self[1])
	i = 2
	while i < self.len
		result = func(result, self[i])
		i = i + 1
	end while
	return result
end function

// filter: remove all elements where a given function is not true (in place).
//	Or instead of a function, you may give the name of a map key, which must
//	evaluate to true for every element we keep (to filter by an object property).
list.filter = function(func)
	i = self.len - 1
	if @func isa string then
		while i >= 0
			f = self[i][func]	// (may be a simple type; this will still work)
			if not f then self.remove i
			i = i - 1
		end while		
	else
		while i >= 0
			if not func(self[i]) then self.remove i
			i = i - 1
		end while
	end if
end function

// filtered: return a new list containing only the elements of self where
//	the given function is true.  As with filter, you may also specify the
//	name of a map key to look up in each element of the self.
list.filtered = function(func)
	result = []
	if @func isa string then
		for elem in self
			f = elem[func]	// (may be a simple type; this will still work)
			if f then result.push elem
		end for
	else
		for elem in self
			if func(elem) then result.push elem
		end for
	end if
	return result
end function

// valuesOf: operates on a list of maps (or other indexable type), and pulls
// out the value of the given index for every element in the list, returning
// theses as a new list in the same order.
list.valuesOf = function(indexToExtract)
	result = []
	for elem in self
		result.push elem[indexToExtract]
	end for
	return result
end function

// removeVal: remove the first (or all) occurrences of a given value.
list.removeVal = function(val, removeAll=false)
	i = 0
	while true
		i = self.indexOf(val, i-1)
		if i == null then return
		self.remove i
		if not removeAll then return
	end while
end function

// any: return a random element of the list
list.any = function
	if self.len < 1 then return null
	return self[self.len * rnd]
end function

// swap: exchange two values in place, by their indexes
list.swap = function(index1, index2)
	temp = self[index1]
	self[index1] = self[index2]
	self[index2] = temp
end function

// init: build an array initialized with an initial value.
// Note that initialValue may be a function passed in with @
// (for example, @rnd), in which case it will be invoked for
// each element as they are created.
list.init = function(size, initialValue)
	if @initialValue isa funcRef then
		result = []
		for i in range(1, size)
			result.push initialValue
		end for
		return result
	else
		return [initialValue] * size
	end if
end function

// init2d: build a 2D array, composed as a list of lists.
// (Note that the math functions here in listUtil probably do not do what
// you would want for a 2D array of numbers; if you want to do matrix math,
// use matrixUtil instead.)
list.init2d = function(rows, columns, initialValue)
	result = []
	for a in range(1, rows)
		result.push list.init(columns, @initialValue)
	end for
	return result
end function

// init3D: build a 3D array, composed as a list of 2D arrays.
list.init3d = function(sizeA, sizeB, sizeC, initialValue)
	result = []
	for a in range(1, sizeA)
		result.push list.init2d(sizeB, sizeC, @initialValue)
	end for
	return result
end function

runUnitTests = function
	print "Unit testing: listUtil"
	
	errorCount = 0
	assertEqual = function(actual, expected, note)
		if actual != expected then
			print "Unit test failure (" + note + "): expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function

	a = [-1, 42, -1, 53, 87, 345, -1, 100]
	assertEqual a.split(-1), [ [], [42], [53, 87, 345], [100] ]
	assertEqual a.split(-1, 3), [ [], [42], [53, 87, 345, -1, 100] ]

	a = [1,2,3]
	a.multiplyBy 3
	assertEqual a, [3,6,9], "multiplyBy (scalar)"
	a.multiplyBy [10,100,10]
	assertEqual a, [30,600,90], "multiplyBy (list)"
	
	a = [1,2,3]
	assertEqual a.times(3), [3,6,9], "times (scalar)"
	assertEqual a, [1,2,3], "times changed self (which it shouldn't)"
	assertEqual a.times([10,100,10]), [10,200,30], "times (list)"
	assertEqual a, [1,2,3], "times changed self (which it shouldn't)"
	
	a = [1,2,3]
	assertEqual a.reversed, [3,2,1], "reversed"
	assertEqual a, [1,2,3], "times changed self (which it shouldn't)"
	a.reverse
	assertEqual a, [3,2,1], "reverse"
	a = [1,2,3,4]
	a.reverse
	assertEqual a, [4,3,2,1], "reverse"
	
	a = ["zero", "one", "two"]
	a.swap 1, 2
	assertEqual a, ["zero", "two", "one"], "swap"

	a = [1.1, 1.9, 3.45]
	a.apply @round
	assertEqual a, [1, 2, 3], "apply"
	a = [1.1, 1.899, 3.452]
	a.apply1 @round, 2
	assertEqual a, [1.10, 1.90, 3.45], "apply1"

	a = range(1, 10)
	isEven = function(x)
		return x % 2 == 0
	end function
	a.filter @isEven
	assertEqual a, [2, 4, 6, 8, 10], "filter"
	a = range(1, 10)
	b = a.filtered(@isEven)
	assertEqual a, range(1, 10), "filtered"
	assertEqual b, [2, 4, 6, 8, 10], "filtered"

	a = [{"x":42, "keep":true}, {"x":123, "keep":false}, {"x":0, "keep":true}]
	a.filter "keep"
	assertEqual a, [{"x":42, "keep":true}, {"x":0, "keep":true}], "filter by key"
	b = a.filtered("x")
	assertEqual b, [{"x":42, "keep":true}], "filtered by key"

	b = a.valuesOf("x")
	assertEqual b, [42, 0], "valuesOf"

	a = [1,2,6,2,5,7,2,9]
	counts = a.counts
	assertEqual counts[2], 3, "counts[2]"
	assertEqual counts[7], 1, "counts[7]"
	b = a.distinct
	b.sort
	assertEqual b, [1,2,5,6,7,9]
	a.removeVal 2
	assertEqual a, [1,6,2,5,7,2,9], "removeVal (one)"
	a.removeVal 2, true
	assertEqual a, [1,6,5,7,9], "removeVal (all)"
		
	sum = function(a,b)
		return a + b
	end function

	a = [1,2,3,4,5]
	assertEqual a.reduce(@sum), 15, "reduce"
	assertEqual a.sum, 15, "sum" // (actually intrinsic, but let's test anyway)
	assertEqual a.product, 120, "product"
	assertEqual a.min, 1, "min"
	assertEqual a.max, 5, "max"
	assertEqual a.mean, 3, "mean"
	assertEqual a.contains(3), true, "contains"
	assertEqual a.contains(7), false, "contains"
	
	a = [1,2,3]
	b = [4,-5,6]
	assertEqual a.dot(b), 12, "dot"
	assertEqual a.plus(b), [5,-3,9], "plus"
	
	a = list.init(4, "x")
	assertEqual a, ["x", "x", "x", "x"]
	
	a = list.init2d(3, 2, 0)
	assertEqual a, [[0,0], [0,0], [0,0]]
	
	if errorCount == 0 then
		print "All tests passed.  Wahoo!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if
end function

if globals == locals then runUnitTests
