// Video Terminal (VTxx) Utilities
//
// This module defines some special strings which, when printed on a VT100
// or compatible terminal window, cause special things to happen: clear
// the screen, move the cursor, turn bold text on/off, etc.

esc = char(27)

// character styles
bold = esc + "[1m"
dim = esc + "[2m"
underline = esc + "[4m"  
blink = esc + "[5m"
inverse = esc + "[7m"
invisible = esc + "[8m"
normal = esc + "[m"		// (turns off all styles)

// cursor movement
cursor = {}
cursor.up = function(n=1); return esc + "[" + n + "A"; end function
cursor.down = function(n=1); return esc + "[" + n + "B"; end function
cursor.left = function(n=1); return esc + "[" + n + "C"; end function
cursor.right = function(n=1); return esc + "[" + n + "D"; end function
cursor.nextLine = esc + "E"
cursor.home = esc + "[f"		// (moves cursor to upper left)
cursor.save = esc + "7"
cursor.restore = esc + "8"
// cursor.goto: row is measured from the top, col is from the left,
// both starting at 1.
cursor.goto = function(row,col); return esc + "[" + row + ";" + col + "H"; end function

// control of tab stops
tab = {}
tab.set = esc + "H"			// set a tab stop at the current column
tab.clear = esc + "[g"		// clear a tab stop at the current column
tab.clearAll = esc + "[3g"	// clear all tab stops

// big text!  Requires printing your message twice, once with
// the "top half" mode and again with the "bottom half"
bigTextTop = esc + "#3"
bigTextBottom = esc + "#4"
// ...or you can print double-wide, normal-height text:
wide = esc + "#6"
// turn off any of these size changes with:
normalSize = esc + "#5"

// clearing part or all of the screen
clear = esc + "[2J"
clearLineToRight = esc + "[K"
clearLineToLeft = esc + "[1K"
clearLine = esc + "[2K"
clearBelow = esc + "[J"
clearAbove = esc + "[1J"
reset = esc + "c"  // (clear, plus also resets all state)

// LEDs, if your terminal has these (most don't anymore)
ledOn = function(n=1)
	return esc + "[" + n + "q"
end function
ledsOff = ledOn(0)

demo = function
	print reset + clear + cursor.home
	print " " * 20 + bold + underline + "vt module demo" + normal
	
	print
	print underline + "Character Styles" + normal
	print "vt.bold:      " + bold + "boldface text" + normal
	print "vt.dim:       " + dim + "dim text" + normal
	print "vt.underline: " + underline + "underlined text" + normal
	print "vt.blink:     " + blink + "blinking text" + normal
	print "vt.inverse:   " + inverse + "inverse text" + normal
	print "vt.invisible: " + invisible + "invisible text" + normal
	print "vt.normal:    undoes any of the above"

	print
	print underline + "Text Sizes" + normal
	print bigTextTop + "bigTextTop and bigTextBottom 12345"
	print bigTextBottom + "bigTextTop and bigTextBottom 67890"
	print wide + "wide (but normal height)"
	print normalSize + "normalSize"

	print cursor.save
	print bold + cursor.goto(7,45) + "*"
	for ang in range(0,360*5,10)
		row = round(7 + 4*sin(ang*pi/180))
		col = round(45 + 8*cos(ang*pi/180))
		print cursor.goto(row,col) + bold + "*" + normal + cursor.restore
		wait 0.02
		print cursor.goto(row,col) + dim + "." + normal + cursor.restore
	end for
	
end function


// If this script is run as the main program (rather than an import
// module), auto-run the demo.
if locals == globals then demo