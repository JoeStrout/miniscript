// Matrix Utilities.
// A matrix is a 2D array of numbers.  These have lots of uses in computer graphics.
// NOTE: in keeping with standard programming convention, but NOT standard math
// convention, we use 0-base indexing.  So the top-left element is 0,0.

import "listUtil"
import "mathUtil"

// Base class setup.
// Note that we stuff Matrix into the global namespace, since this will
// almost always be wanted; this module does nothing BUT define this class
// and some unit tests.
globals.Matrix = {}

// m.rows and m.columns define the size of the matrix.  These should never
// change once the matrix is initialized.
Matrix.rows = 0			// how many rows we have
Matrix.columns = 0		// how many columns we have

// m.elem: actual list-of-lists which holds the numbers in the matrix.
// Access as m.elem[row][column].  It's OK to change these numbers
// (but not the size of the lists).
Matrix.elem = null

Matrix.rowRange = function
	// We cache the range() result, overriding this method on the instance
	// with the resulting value.  This is valid because matrices should
	// never change their size.
	self.rowRange = range(0, self.rows-1)
	return self.rowRange
end function
Matrix.colRange = function
	self.colRange = range(0, self.columns-1)
	return self.colRange
end function


//----------------------------------------------------------------------
// Class methods: call these directly on Matrix, usually to get back a new
// Matrix instance.
//----------------------------------------------------------------------

// Matrix.fromList: create a new Matrix from a 1D or 2D list.
// (If given a 1D list, it returns a 1 x len matrix, i.e., a row vector.)
Matrix.fromList = function(sourceList)
	m = new Matrix
	if sourceList.len == 0 then return m
	if sourceList[0] isa list then
		m.rows = sourceList.len
		m.columns = sourceList[0].len
		m.elem = sourceList.deepCopy
	else
		m.rows = 1
		m.columns = sourceList.len
		m.elem = [sourceList[:]]
	end if
	return m
end function

// Matrix.ofSize: create a new Matrix with the given dimensions 
// and all elements set to the given initial value.
Matrix.ofSize = function(rows, columns, initialValue=0)
	m = new Matrix
	m.rows = rows
	m.columns = columns
	m.elem = list.init2d(rows, columns, @initialValue)
	return m
end function

// Matrix.identity: return a square matrix with ones on the main diagonal,
// and zeros everywhere else.  Multiplying another matrix by the identity
// matrix gives a result identical to the other matrix.
Matrix.identity = function(size)
	m = Matrix.ofSize(size, size)
	for i in m.rowRange
		m.elem[i][i] = 1
	end for
	return m
end function

//----------------------------------------------------------------------
// Instance methods: call these on a Matrix instance.
//----------------------------------------------------------------------

// m.row: return the given row of matrix m as a single-row matrix.
Matrix.row = function(zeroBasedRowNum)
	return Matrix.fromList(self.elem[zeroBasedRowNum])
end function

// m.column: return the given column of matrix m as a single-column matrix.
Matrix.column = function(zeroBasedColNum)
	m = Matrix.ofSize(self.rows, 1)
	for r in self.rowRange
		m.elem[r][0] = self.elem[r][zeroBasedColNum]
	end for
	return m
end function

// m.columnAsRow: return the given column of matrix m, but transposed
// so that it is returned as a single-row matrix.
Matrix.columnAsRow = function(zeroBasedColNum)
	m = Matrix.ofSize(1, self.rows)
	for r in self.rowRange
		m.elem[0][r] = self.elem[r][zeroBasedColNum]
	end for
	return m
end function

// m.transpose: return a matrix with the rows and columns reversed
// (so rows become columns, and vice versa).
Matrix.transpose = function
	m = Matrix.ofSize(self.columns, self.rows)
	for r in self.rowRange
		for c in self.colRange
			m.elem[c][r] = self.elem[r][c]
		end for
	end for
	return m
end function

// m.clone: return an exact copy of this matrix.
Matrix.clone = function
	m = new Matrix
	m.rows = self.rows
	m.columns = self.columns
	m.elem = [0] * self.rows
	for r in self.rowRange
		m.elem[r] = self.elem[r][:]
	end for
	return m
end function

// m.add: add a number to all elements of a matrix (in place),
// OR do element-wise addition with another matrix (or 2d list) of the same size.
Matrix.add = function(addend)
	if addend isa Matrix then addend = addend.elem
	if addend isa list then
		for r in self.rowRange
			selfRow = self.elem[r]
			otherRow = addend[r]
			for c in self.colRange
				selfRow[c] = selfRow[c] + otherRow[c]
			end for
		end for
	else
		for row in self.elem
			for c in self.colRange
				row[c] = row[c] + addend
			end for
		end for
	end if
end function

// m.plus: add a number to all elements of a matrix, OR do element-wise addition
// with another matrix or 2d list of the same size, returning a new matrix
// (leaving this one untouched).
Matrix.plus = function(addend)
	result = self.clone
	result.add addend
	return result
end function

// m.elemMultiplyBy: multiply all elements of a matrix by a number (in place),
// OR do element-wise multiplication with another matrix (or 2d list) of the same size.
Matrix.elemMultiplyBy = function(factor)
	if factor isa Matrix then factor = factor.elem
	if factor isa list then
		for r in self.rowRange
			selfRow = self.elem[r]
			otherRow = factor[r]
			for c in self.colRange
				selfRow[c] = selfRow[c] * otherRow[c]
			end for
		end for
	else
		for row in self.elem
			for c in self.colRange
				row[c] = row[c] * factor
			end for
		end for
	end if
end function

// m.elemTimes: multiply all elements of a matrix by a number, OR do element-wise
// addition with another matrix or 2d list of the same size, returning a new
// matrix (leaving this one untouched).
Matrix.elemTimes = function(factor)
	result = self.clone
	result.elemMultiplyBy factor
	return result
end function

// m.times: multiply all elements of a matrix by a number (in place),
// OR do proper matrix multiplication with another matrix of a compatible size
// (i.e., the number of rows in m2 must equal the number of columns in m).
Matrix.times = function(m2)
	if m2 isa Matrix then m2 = m2.elem
	if m2 isa list then
		// matrix multiplication
		result = Matrix.ofSize(self.rows, m2[0].len)
		for r in result.rowRange
			resultRow = result.elem[r]
			for c in result.colRange
				sum = 0
				for i in self.colRange
					sum = sum + self.elem[r][i] * m2[i][c]
				end for
				resultRow[c] = sum
			end for
		end for
	else
		// simple multiplication by a scalar
		result = self.clone
		result.elemMultiplyBy m2
	end if
	return result
end function

// m.swapRows: swap two rows in this matrix (in place).
Matrix.swapRows = function(rowA, rowB)
	temp = self.elem[rowA]
	self.elem[rowA] = self.elem[rowB]
	self.elem[rowB] = temp
end function

// m.swapColumns: swap two columns in this matrix (in place).
Matrix.swapColumns = function(colA, colB)
	for r in self.rowRange
		row = self.elem[r]
		temp = row[colA]
		row[colA] = row[colB]
		row[colB] = temp
	end for
end function

// m.sameSize(m2): return whether matrix m is the same size as m2.
Matrix.sameSize = function(m2)
	return self.rows == m2.rows and self.columns == m2.columns
end function

// m.equals(m2): return whether all the values in the two matrices are equal.
// Or, instead of a matrix, you can supply a 2d list, and it will compare 
// the values against that.
Matrix.equals = function(m2)
	if m2 isa list then m2elems = m2 else m2elems = m2.elem
	return self.elem == m2elems
end function

// Matrix.print: prints matrix data, with some formatting options.
//	fieldWidth: minimum number of characters for each element
//	precision: if non-null, round each element to this many digits
//	columnSep: extra string printed between elements within a row
//	rowSep: extra string printed between rows; defaults to text.delimiter
Matrix.print = function(fieldWidth=10, precision=null, columnSep="", rowSep=null)
	oldDelim = text.delimiter; text.delimiter = ""
	if rowSep == null then rowSep = oldDelim
	for row in self.elem
		firstInRow = true
		for elem in row
			s = mathUtil.numToStr(elem, precision)
			if s.len < fieldWidth then s = " "*(fieldWidth-s.len) + s
			if not firstInRow then print columnSep
			print s
			firstInRow = false
		end for
		print rowSep
	end for
	text.delimiter = oldDelim
end function


runUnitTests = function
	print "Unit testing: matrixUtil"
	
	errorCount = 0
	assertEqual = function(actual, expected, note)
		if expected isa list and actual isa Matrix then actual = actual.elem
		if actual != expected then
			print "Unit test failure (" + note + "): expected " + expected + ", got " + actual + " at " + stackTrace[1]
			outer.errorCount = errorCount + 1
		end if
	end function
	
	assert = function(shouldBeTrue, note)
		if shouldBeTrue then return
		print "Unit test failure (" + note + ")"
	end function
	
	m = Matrix.ofSize(2,3, 42)
	assertEqual m, [[42,42,42], [42,42,42]]
	
	m = Matrix.identity(3)
	assertEqual m, [[1,0,0], [0,1,0], [0,0,1]]
	
	m = Matrix.fromList([1,2,3])
	assertEqual m.rows, 1
	assertEqual m.columns, 3
	assertEqual m, [[1,2,3]]
		
	m = Matrix.fromList([[1,2,3], [4,5,6]])
	assertEqual m.row(0), [[1,2,3]]
	assertEqual m.row(1), [[4,5,6]]
	assertEqual m.column(0), [[1], [4]]
	assertEqual m.column(2), [[3], [6]]
	assertEqual m.columnAsRow(2), [[3, 6]]
	assert m.equals(Matrix.fromList([[1,2,3], [4,5,6]]))
	assert m.equals([[1,2,3], [4,5,6]])
	
	m2 = m.clone
	assertEqual m.elem, m2.elem
	assert m.equals(m2)
	m2.elem[1][2] = 60
	assertEqual m, [[1,2,3], [4,5,6]]
	assertEqual m2, [[1,2,3], [4,5,60]]	

	mT = m.transpose
	assertEqual m, [[1,2,3], [4,5,6]]
	assertEqual mT, [[1,4], [2,5], [3,6]]
	assert not m.sameSize(mT), "m.sameSize(mT)"
	
	m2 = m.plus(10)
	assertEqual m, [[1,2,3], [4,5,6]]
	assertEqual m2, [[11,12,13], [14,15,16]]	

	m2 = m.plus([[10,0,0], [0,0,100]])
	assertEqual m, [[1,2,3], [4,5,6]]
	assertEqual m2, [[11,2,3], [4,5,106]]

	m2 = m.times(10)
	assertEqual m, [[1,2,3], [4,5,6]]
	assertEqual m2, [[10,20,30], [40,50,60]]
	
	m2 = m.clone
	m2.swapRows 0, 1
	assertEqual m2, [[4,5,6], [1,2,3]]
	m2 = m.clone
	m2.swapColumns 1, 2
	assertEqual m2, [[1,3,2], [4,6,5]]
	
	m2 = Matrix.fromList([[7,8], [9,10], [11,12]])
	product = m.times(m2)
	assertEqual product, [[58,64], [139,154]]
	
	if errorCount == 0 then
		print "All tests passed.  Matrices rock!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if
end function

if globals == locals then runUnitTests
