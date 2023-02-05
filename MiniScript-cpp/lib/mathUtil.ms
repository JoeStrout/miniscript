// Math Utilities.

// NOTE: Some of these functions are just wrappers for hidden intrinsic
// methods (methods that start with an underscore).  You should call
// those via mathUtil anyway, rather than calling them directly.  The
// hidden intrinsics might change, but we'll update this mathUtil module
// so your code doesn't break.

// e: a mathematical constant with some interesting properties.  See:
// https://en.wikipedia.org/wiki/E_(mathematical_constant)
e = 2.7182818284

// ln: natural logarithm (i.e., log base e)
ln = function(x)
	return log(x, 2.7182818284)
end function

// radToDeg: convert radians to degrees
radToDeg = function(radians)
	return radians * 180 / pi
end function

// degToRad: convert degrees to radians
degToRad = function(degrees)
	return degrees * pi / 180
end function

// moveTowards: shift a value towards another value, but no more than a 
// certain amount.  This version works on a number and returns a number.
moveTowards = function(num, targetNum, maxChange=1)
	if abs(targetNum - num) <= maxChange then return targetNum
	if targetNum > num then return num + maxChange
	return num - maxChange
end function

// moveTowardsXY: shift a map containing "x" and "y" values towards a 
// target map that contains the same, but moving no more than the given
// distance.  This is handy, for example, to make a sprite move towards
// another sprite or the mouse.  Note that this version changes the map
// passed in the first parameter, and returns `true` if any change was
// made, or `false` if the mover was already at the target.
moveTowardsXY = function(mover, target, maxDist=1)
	dx = target.x - mover.x
	dy = target.y - mover.y
	if dx == 0 and dy == 0 then return false  // already there
	dist = sqrt(dx^2 + dy^2)
	if dist < maxDist then
		mover.x = target.x
		mover.y = target.y
	else
		f = maxDist / dist
		mover.x = mover.x + dx * f
		mover.y = mover.y + dy * f
	end if
	return true   // moved, at least a little
end function

// distance: Calculate the distance between two points.  Each point may be
// either a map containing "x" and "y" (such as mouse or a Sprite), or it
// can be a 2-element [x,y] list.
distance = function(p1, p2)
	if p1 isa map then
		x1 = p1.x
		y1 = p1.y
	else
		x1 = p1[0]
		y1 = p1[1]
	end if
	if p2 isa map then
		x2 = p2.x
		y2 = p2.y
	else
		x2 = p2[0]
		y2 = p2[1]
	end if	
	return sqrt((x1-x2)^2 + (y1-y2)^2)
end function

// lerp: Interpolate between two values, that is, return a value that is
// some portion (t) of the way from a to b.
lerp = function(a, b, t)
	return a + (b-a) * t
end function

// lerp2d: Interpolate between two points, that is, return a point that is
// some portion (t) of the way from p1 to p2.  p1 and p2 may be either a map
// containing "x" and "y", or it may be an [x,y] list, but the result will
// always be in the form of an [x,y] list.
lerp2d = function(p1, p2, t)
	if p1 isa map then
		x1 = p1.x
		y1 = p1.y
	else
		x1 = p1[0]
		y1 = p1[1]
	end if
	if p2 isa map then
		x2 = p2.x
		y2 = p2.y
	else
		x2 = p2[0]
		y2 = p2[1]
	end if	
	return [x1 + (x2-x1) * t, y1 + (y2-y1) * t]
end function

// randNormal: return a normally-destributed (i.e. Gaussian) number
// with the given mean and standard deviation.
//  mean: average value of the returned numbers
//  stddev: standard deviation of the returned numbers
randNormal = function(mean=0, stddev=1)
    return mean + sqrt(-2 * log(rnd,2.7182818284)) * cos(2*pi*rnd) * stddev
end function

// randRange: return a uniformly distributed random number between the
// given minimum and maximum values (including min, but not including max).
randRange = function(min, max)
	return min + (max - min) * rnd
end function

// dice: return the sum of rolling some number of dice.
// For example, dice(3,6) simulates rolling three 6-sided dice.
dice = function(numberOfDice=1, sides=6)
	sum = 0
	for i in range(1, numberOfDice)
		sum = sum + ceil(rnd * sides)
	end for
	return sum
end function

// clamp: limits the given value to a specified range.
clamp = function(x, minval=0, maxval=1)
	if x < minval then return minval
	if x > maxval then return maxval
	return x
end function

// numToStr: converts a number to a string, with a specified precision
// (number of digits past the decimal place).  Trailing zeros will be
// added as needed to get the decimal point at the right place.
numToStr = function(n, precision=null)
	if precision == null then return str(n)
	s = str(round(n, precision))
	if precision > 0 then
		dotPos = s.indexOf(".")
		if dotPos == null then
			s = s + "." + "0" * precision
		else if dotPos > s.len - precision - 1 then
			s = s + "0" * (dotPos - s.len + precision + 1)
		end if
	end if
	return s
end function

runUnitTests = function
	print "Unit testing: mathUtil"
	
	errorCount = 0
	assertEqual = function(actual, expected, note)
		if actual != expected then
			print "Unit test failure (" + note + "): expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function
	
	assertEqual moveTowards(100, 25, 10), 90, "moveTowards"
	mover = {"x":50, "y":25}
	target = {"x":60, "y":25}
	assertEqual moveTowardsXY(mover, target), true
	assertEqual mover.x, 51, "moveTowardsXY"
	assertEqual mover.y, 25, "moveTowardsXY"
	target = {"x":51, "y":30}
	assertEqual moveTowardsXY(mover, target, 10), true
	assertEqual mover.x, 51, "moveTowardsXY"
	assertEqual mover.y, 30, "moveTowardsXY"
	assertEqual moveTowardsXY(mover, target, 10), false
	
	assertEqual numToStr(pi, 2), "3.14"
	assertEqual numToStr(1.23, 3), "1.230"
	assertEqual numToStr(1.23), "1.23"
	assertEqual numToStr(12345.67, -2), "12300"
	assertEqual numToStr(2, 3), "2.000"

	if errorCount == 0 then
		print "All tests passed.  Woot!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if
end function

if globals == locals then runUnitTests
