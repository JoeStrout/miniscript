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

_hexVal = function(s)
	result = 0
	for c in s.upper
		digitVal = "0123456789ABCDEF".indexOf(c)
		result = result * 16 + digitVal
	end for
	return result
end function

_closestAnsiPaletteIndex = function(r, g, b)
	if r == g == b then
		// for grayscale colors, use the grayscale palette, from 232-255
		if r < 8 then return 232
		if r > 248 then return 255
		return 232 + round(((r - 8) / 247) * 24)
	end if
	// for anything else, use the 6x6x6 RGB color cube, indices 16-231
	return 16 + (36 * round(r/255*5)) + (6 * round(g / 255 * 5)) + round(b / 255 * 5)
end function

_getColorIndex = function(c)
	if c isa number then
		return c
	else if c isa list then
		return _closestAnsiPaletteIndex(c[0], c[1], c[2])
	else if c isa string then
		if c[0] == "#" then c = c[1:]
		r = _hexVal(c[0:2])
		g = _hexVal(c[2:4])
		b = _hexVal(c[4:6])
		return _closestAnsiPaletteIndex(r, g, b)
	end if
	return 0
end function

// textColor: sets the foreground color of the text.  Specify a single
// number (index into the 256-color palette), or an [r,g,b] (each 255) list,
// or a RGB color string like "#FF8800".  (In the latter two cases, we will 
// automatically find as close match as we can in the color palette.)
// Restore normal colors by printing vt.normal.
textColor = function(c)
	return esc + "[38;5;" + _getColorIndex(c) + "m"
end function

// backColor: sets the background color of the text.  Specify a single
// number (index into the 256-color palette), or an [r,g,b] (each 255) list,
// or a RGB color string like "#FF8800".  (In the latter two cases, we will 
// automatically find as close match as we can in the color palette.)
// Restore normal colors by printing vt.normal.
backColor = function(c)
	return esc + "[48;5;" + _getColorIndex(c) + "m"
end function

// Map of color names (to palete index) for use with textColor and backColor.
// But note that you are not limited to these colors!  See notes above.
color = {}
color.aqua = 51
color.black = 232
color.blue = 21
color.brown = 137
color.fuchsia = 201
color.gray = 244
color.green = 34
color.lime = 46
color.maroon = 124
color.navy = 19
color.olive = 142
color.orange = 214
color.pink = 217
color.purple = 127
color.red = 196
color.silver = 250
color.teal = 37
color.white = 255
color.yellow = 226

// cursor movement
cursor = {}
cursor.up = function(n=1); return esc + "[" + n + "A"; end function
cursor.down = function(n=1); return esc + "[" + n + "B"; end function
cursor.left = function(n=1); return esc + "[" + n + "D"; end function
cursor.right = function(n=1); return esc + "[" + n + "C"; end function
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
// the "top half" mode and again with the "bottom half".
// (Not supported by xterm.js, alas.)
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

	print; print
	print underline + "Text Sizes" + normal
	print bigTextTop + "bigTextTop and bigTextBottom 12345"
	print bigTextBottom + "bigTextTop and bigTextBottom 67890"
	print wide + "wide (but normal height)"
	print normalSize + "normalSize"

	print; print
	print cursor.save

	colorNames = color.indexes
	colorNames.sort
	y = 2; x = 56
	for i in colorNames.indexes
		c = colorNames[i]
		idx = color[c]
		if idx > 180 and idx != 232 then idx2 = 232 else idx2 = 255
		print cursor.goto(y, x) + backColor(idx) + textColor(idx2), ""
		print ("   " + idx)[-3:], ""
		if idx != 232 then idx2 = 232
		print textColor(idx) + backColor(idx2) + " " + c
		y += 1
		if y > 11 then; y = 2; x += 13; end if
	end for

	print bold + cursor.goto(7,45) + "*"
	for ang in range(0,360*5,10)
		row = round(7 + 4*sin(ang*pi/180))
		col = round(45 + 8*cos(ang*pi/180))
		print cursor.goto(row,col) + bold + "*" + normal + cursor.restore
		wait 0.02
		print cursor.goto(row,col) + dim + "." + normal + cursor.restore
	end for
	
	print; print
end function


// If this script is run as the main program (rather than an import
// module), auto-run the demo.
if locals == globals then demo