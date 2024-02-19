// GRFON (General Recursive Format Object Notation)
//
// GRFON is a simpler, gentler file format designed to be
// especially human-editable.
// https://app.assembla.com/wiki/show/grfon

// Flag to control whether strings "true" and "false" should be
// returned as 1 or 0, or left as strings.
interpretTrueAndFalse = false

// Similar flag for string "null".
interpretNull = true

// parse: convert a GRFON string into a MiniScript value (which could
//	include a list or map of other values).  This is the main entry point
//	for reading GRFON data and converting it to native form.
//	Example: parse("42")		// returns 42
parse = function(grfonString)
	//if grfonString isa RawData then grfonString = grfonString.utf8
	p = new Parser
	return p.parse(grfonString)
end function


// Escape any special characters in the given string by preceeding
// them with backslashes.  In addition: a newline becomes \n; a
// carriage return becomes \r; and a tab becomes \t.
escape = function(s)
	for i in _escapeIndexes
		s = s.replace(_escapeFrom[i], _escapeTo[i])
	end for
	s = s.replace("//", "\/\/")
	return s
end function

// unescape: replace backslash sequences in the given string.
//	Example: unescape("\t")		// returns char(9)
unescape = function(s)
	result = []
	i = 0
	maxi = s.len
	while i < maxi
		di = 1
		if s[i] == "\" then
			di = 2
			c = s[i+1]
			if c == "b" then
				result.push char(8)
			else if c == "t" then
				result.push char(9)
			else if c == "n" then
				result.push char(10)
			else if c == "f" then
				result.push char(12)
			else if c == "r" then
				result.push char(13)
			else if c == "u" then
				// Unicode code point (must always be 4 digits)
				hex = s[i+2:i+6]
				result.push char(hexToInt(hex))
				di = 6
			else
				result.push c
			end if
		else
			result.push s[i]
		end if
		i = i + di
	end while
	return result.join("")
end function

toGRFON = function(value, compact=false, indent=0, topLevel=true)
	if @value isa funcRef then return "<function>"
	if value == null then return "null"
	if value isa number then return str(value)
	if value isa string then return escape(value)
	if value isa list then return _listToGRFON(value, compact, indent, topLevel)
	if value isa map then return _mapToGRFON(value, compact, indent, topLevel)
end function

//----------------------------------------------------------------------
// Stuff below is internal implementation; 
// most users don't need to poke around here.

cr = char(13)
lf = char(10)

// Parsing GRFON

Parser = {}
Parser.source = ""
Parser._sourceLen = 0
Parser._p = 0		// index of next character to consume in source

Parser.init = function(source)
	self.source = source
	self._sourceLen = source.len
end function

Parser.parse = function(source=null)
	if source != null then self.init source
	self._p = 0
	return self._parseElement
end function

whitespace = " " + char(9) + cr + lf
Parser._skipWhitespace = function
	while self._p < self._sourceLen
		if self.source[self._p : self._p + 2] == "//" then
			self._skipToEOL
		else if whitespace.indexOf(self.source[self._p]) != null then
			self._p = self._p + 1
		else
			break
		end if
	end while
end function

Parser._skipToEOL = function
	while self._p < self._sourceLen
		c = self.source[self._p]
		self._p = self._p + 1
		if c == cr or c == lf then break
	end while
end function

Parser._parseElement = function(asValue=false)
	result = null
	while true
		self._skipWhitespace
		if self._p >= self._sourceLen then return result
		if self.source[self._p] == "}" then 
			// done with current collection
			self._p = self._p + 1
			return result
		end if
		if self.source[self._p] == "{" then
			// nested collection
			self._p = self._p + 1
			tok = self._parseElement
			if tok == null then tok = {}  // (interpret {} as an empty map)
			if self._p < self._sourceLen and self.source[self._p] == "{" then
				self._p = self._p + 1
			end if
		else
			tok = self._parseValue
		end if
		if asValue then return tok
		if tok == null then return result
		next = ""
		self._skipWhitespace
		if self._p < self._sourceLen then next = self.source[self._p]
		if next == ":" then			// key:value pair
			self._p = self._p + 1		// skip colon
			tok2 = self._parseElement(true)
			if result == null then
				result = {}
			else if result isa list then
				result = {"_": result}
			end if
			result[tok] = tok2
			self._skipWhitespace
			if self._p < self._sourceLen and self.source[self._p] == ";" then
				self._p = self._p + 1
			end if
		else if next == ";" then	// new collection element
			if result == null then
				result = [tok]
			else if result isa list then
				result.push tok
			else
				result["_"].push tok
			end if
			self._p = self._p + 1		// skip semicolon
		else
			if result == null then
				result = tok
			else if result isa list then
				result.push tok
			else if result isa map then
				if result.hasIndex("_") then
					result["_"].push tok
				else
					result["_"] = [tok]
				end if
			else
				result = [result, tok]
			end if
			if next == "" then return result
		end if
	end while
end function

Parser._parseValue = function
	self._skipWhitespace
	if self._p >= self._sourceLen then return null
	c = self.source[self._p]
	s = self._parseString
	if interpretTrueAndFalse then
		if s == "true" then return true
		if s == "false" then return false
	end if
	if interpretNull and s == "null" then return null
	numVal = val(s)
	if s == "0" or numVal != 0 then return numVal
	return s
end function

// Get a string literal from the source.  Stop at the delimiter
// so the caller can see if it is a ":" or "}" or whatever.
_delims = ":;}" + cr + lf
Parser._parseString = function
	startPos = self._p
	anyEscape = false
	while self._p < self._sourceLen
		c = self.source[self._p]
		if _delims.indexOf(c) != null then break
		if c == "/" and self._p+1 < self._sourceLen and self.source[self._p+1] == "/" then break
		self._p = self._p + 1
		if c == "\" then
			anyEscape = true
			self._p = self._p + 1
		end if
	end while
	result = self.source[startPos : self._p]
	if anyEscape then result = unescape(result)
	return result
end function


// Generating GRFON

_listToGRFON = function(lst, compact, indent, topLevel=false)
	if compact then
		ws = "; "
	else
		ws = (_eol + "  "*(indent+1)*(not topLevel))
	end if
	parts = []
	if not topLevel then
		parts.push "{"
		if not compact then parts.push ws
	end if
	nextIndent = indent + (not topLevel)
	first = true
	for item in lst
		if first then first = false	else parts.push ws
		parts.push toGRFON(item, compact, nextIndent, false)
	end for
	if not topLevel then
		if not compact then parts.push _eol + "  " * indent
		parts.push "}"
	end if
	return join(parts, "")
end function

_mapToGRFON = function(m, compact, indent, topLevel=false)
	if compact then
		ws = "; "
	else
		ws = (_eol + "  "*(indent+1)*(not topLevel))
	end if
	parts = []
	if not topLevel then
		parts.push "{"
		if not compact then parts.push ws
	end if
	nextIndent = indent + (not topLevel)
	first = true
	for kv in m
		if first then first = false	else parts.push ws
		parts.push toGRFON(kv.key, true, nextIndent, false) +
		": " + toGRFON(kv["value"], compact, nextIndent, false)
	end for
	if not topLevel then
		if not compact then parts.push _eol + "  " * indent
		parts.push "}"		
	end if
	return join(parts, "")
end function

_escapeFrom = ["\", """", ":", ";", char(8), char(9), char(10), char(12), char(13)]
_escapeTo = ["\\", "\""", "\:", "\;", "\b","\t","\n","\f","\r"]
_escapeIndexes = _escapeFrom.indexes
_eol = char(10)

//----------------------------------------------------------------------
// Unit tests (run when you load & run this script directly).
runUnitTests = function
	print "Unit testing: grfon"
	
	errorCount = 0
	assertEqual = function(actual, expected)
		if actual != expected then
			print "Unit test failure: expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function
	assertEqualEither = function(actual, expected, expected2)
		if actual != expected and actual != expected2 then
			print "Unit test failure: expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function
	
	assertEqual escape("foo"+char(9)+"bar"), "foo\tbar"
	assertEqual unescape("foo\tbar"), "foo"+char(9)+"bar"
	assertEqual escape("http://foo"), "http\:\/\/foo"
	assertEqual unescape("http\:\/\/foo"), "http://foo"
	
	// Note: the order of key/value pairs in a dictionary is undefined.
	// So, it's a bit hard to unit test, as the order in which they appear
	// in GRFON isn't defined either.
	s = toGRFON([1, 2, "foo"], true)
	assertEqual s, "1; 2; foo";
	s = toGRFON([1, 2, "foo"])
	assertEqual escape(s), "1\n2\nfoo"
	d = {1:"one", 2:"two", "three":"san"}
	s = toGRFON(d, true)
	//assertEqual s, "1: one; 2: two; three: san"
	assertEqual s.indexOf("1: one") != null, true
	assertEqual s.indexOf("2: two") != null, true
	assertEqual s.indexOf("three: san") != null, true
	assertEqual s.split(";").len, 3
	s = toGRFON(d)
	//assertEqual s, unescape("1: one\n2: two\nthree: san")
	assertEqual s.indexOf("1: one") != null, true
	assertEqual s.indexOf("2: two") != null, true
	assertEqual s.indexOf("three: san") != null, true
	assertEqual s.split(unescape("\n")).len, 3
	d = [0, [1], [1,2]]
	s = toGRFON(d)
	assertEqual s, unescape("0\n{\n  1\n}\n{\n  1\n  2\n}")
	d = {"foo":[1, 2, 3], "bar":"baz"}
	s = toGRFON(d, true)
	//assertEqual s, "foo: {1; 2; 3}; bar: baz"
	assertEqual s.indexOf("foo: {1; 2; 3}") != null, true
	assertEqual s.indexOf("bar: baz") != null, true
	assertEqual s.split(";").len, 4
	
	p = new Parser
	p.init("   true")
	assertEqual p.parse, "true"
	
	assertEqual parse("abc"), "abc"
	assertEqual parse(char(13) + "42"), 42
	assertEqual parse("-123.45"), -123.45
	assertEqual parse(".5"), 0.5
	assertEqual parse("12 // foo"), 12
	assertEqual parse("// foo" + cr + "34"), 34
	assertEqual parse("1; 2; foo"), [1, 2, "foo"]
	assertEqual parse("1:one; 2:two"), {1:"one", 2:"two"}
	assertEqual parse("\tHello, \""Bob\""."), char(9) + "Hello, ""Bob""."
	
	assertEqual parse("foo:{1; 2; 3}; bar:baz"), {"foo":[1, 2, 3], "bar":"baz"}
	
	// An empty {} could be interpreted as a list or map, and the choice
	// is rather arbitrary.  But we choose to have it return a map
	assertEqual parse("{}"), {}
	assertEqual parse("1; 2; {}; 4"), [1, 2, {}, 4]
	
	// Fixed bug with 3 lines of comments throwing Runtime Error
	assertEqual parse("//" + cr + "foo:42"), {"foo": 42}
	assertEqual parse("//" + cr + "//" + cr + "foo:42"), {"foo": 42}
	assertEqual parse("//" + cr + "//" + cr + "//" + cr + "foo:42"), {"foo": 42}
	
	// Fixing bug with two line breaks after a comment making the "{" character a part of the key
	assertEqual parse("//" + cr + cr + "{foo:42}"), {"foo": 42}
	
	if errorCount == 0 then
		print "All tests passed.  Fus Ro Dah!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if
	
end function

if locals == globals then runUnitTests
