// This is a little module to read Tab-Separated Value (TSV) files,
// such as can be exported (for example) from Google Sheets.
//
// The data is returned in one of two ways, depending on whether the
// first column contains unique identifiers.  You select which way 
// you want via the `asRowList` paremeter.
//
// If asRowList=false:
// The file should be organized by row and column, with row headers
// (if any) in column 1 and column headers in row 1.  The return value 
// will be a map of maps, indexed first by row header and second by
// column header.
//
// Example: if your data file looks like this:
//            color    points    calories
//    apple   red      100       234.5
//    banana  yellow   200       180.75
//    cherry  red      500       85.235
//
// ...and you used this module to read this file into a variable
// called data, then you could read data.apple.points to find
// the value 100, or equivalently, data["apple"]["points"].
//
// The map for each row will also have a _lineNum entry which is
// the 1-based line number that data came from.
//
// If asRowList=true (the default):
// When you don't have unique row identifiers in column 1,
// you can get the result as a list of maps.
// Example: if your data file looks like this:
//    color    points    calories
//    red      100       234.5
//    yellow   200       180.75
//    red      500       85.235
//
// ...and you used this module to read this file into a variable
// called data, then you could read data[0].points to find
// the value 100, or equivalently, data[0]["points"].  (Note that
// the list index starts at 0, though _lineNum will still be the
// 1-based number in the file, which will be 2 for the first entry
// since line 0 is the headers.)

// tsv.parseLines: parse TSV data that's already in a list of lines.
// If asRowList == true:
//	The result will be a list of maps.
// If asRowList == false:
//	The result will be a map of maps, indexed by the value in the first
//	column (which must be unique, or else you will lose some rows due
//	to reuse of that first column value as a key).
parseLines = function(lines, asRowList=true)
	TAB = char(9)
	CR = char(13)
	// First line should be column names.
	colNames = lines[0].split(TAB)
	if asRowList then
		result = []
		dataCols = range(0, colNames.len-1)
	else
		result = {}
		dataCols = range(1, colNames.len-1)
	end if
	// Subsequent lines are data, with row names at index 0.
	for lineNum in range(1, lines.len-1)
		line = lines[lineNum]
		if not line then continue
		if line[-1] == CR then line = line[:-1]  // (fix Windows line endings)
		fields = line.split(TAB)
		rowMap = {"_lineNum": lineNum + 1}
		if asRowList then
			result.push rowMap
		else
			if fields[0] == "" then continue
			result[fields[0]] = rowMap
		end if
		for col in dataCols
			if col >= fields.len then value = null else value = fields[col]
			if value == "0" or val(value) != 0 then value = val(value)
			rowMap[colNames[col]] = value
		end for
	end for
	return result
end function

// tsv.parseLines: parse TSV data in a file at the given path.
// If asRowList == true:
//	The result will be a list of maps.
// If asRowList == false:
//	The result will be a map of maps, indexed by the value in the first
//	column (which must be unique, or else you will lose some rows due
//	to reuse of that first column value as a key).
parseFile = function(path, asRowList=true)
	lines = file.readLines(path)
	if lines == null then
		print "tsv.parseFile: unable to read " + path
		return null
	end if
	return parseLines(lines, asRowList)
end function

// tsv.parse: parse TSV data where lines are separated by CR, LF, or CRLF.
parse = function(text, asRowList=true)
	CR = char(13)
	LF = char(10)
	lines = []
	if text.indexOf(CR+LF) != null then
		lines = text.split(CR+LF)
	else if text.indexOf(CR) != null then
		lines = text.split(CR)
	else
		lines = text.split(LF)
	end if
	return parseLines(lines, asRowList)
end function

	
//----------------------------------------------------------------------
// Unit tests (run when you load & run this script directly).
runUnitTests = function
	print "Unit testing: tsv"
	
	errorCount = 0
	assertEqual = function(actual, expected)
		if actual != expected then
			print "Unit test failure: expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function
	
	TAB = char(9)
	
	// First let's test the case where each row has a unique identifier
	// in column 0, so we want it as a map of maps.
	sampleData = [
		"name" + TAB + "color" + TAB + "points" + TAB + "calories",
		"apple" + TAB + "red" + TAB + 100 + TAB + 234.5,
		"banana" + TAB + "yellow" + TAB + 200 + TAB + 180.75,
		"cherry" + TAB + "red" + TAB + 500 + TAB + 85.235]
	data = parseLines(sampleData, false)
	assertEqual data.len, 3
	assertEqual data.apple.color, "red"
	assertEqual data.banana.color, "yellow"
	assertEqual data.cherry.points, 500
	assertEqual data.cherry._lineNum, 4
	
	data = parse(sampleData.join(char(13)+char(10)), false)
	assertEqual data.apple.points, 100
	
	data = parse(sampleData.join(char(13)), false)
	assertEqual data.apple.points, 100
	
	data = parse(sampleData.join(char(10)), false)
	assertEqual data.apple.points, 100
	
	// Now let's test the same data, but not assuming column 0 is
	// unique, so we want it as just a list of maps.
	data = parseLines(sampleData, true)
	assertEqual data.len, 3
	assertEqual data[0].name, "apple"
	assertEqual data[0].color, "red"
	assertEqual data[1].color, "yellow"
	assertEqual data[2].points, 500
	assertEqual data[2]._lineNum, 4	
	
	if errorCount == 0 then
		print "All tests passed.  Woo!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if

end function

if locals == globals then runUnitTests
