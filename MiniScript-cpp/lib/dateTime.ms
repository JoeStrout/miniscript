// Date/Time support module.
//
// Mini Micro supports dates/times in two different formats: as a string
// (by default in SQL format), and as a number of seconds since a reference
// date (Jan 01, 2000).  The numeric format is useful for doing date/time
// calculations, like finding the amount of time between two dates, or adding
// some amount of time to a starting date/time.

// nowVal: get the current date/time, as a number.
nowVal = function
	return _dateVal
end function

// now: return the current date/time as a string
now = function(format="yyyy-MM-dd HH:mm:ss")
	return _dateStr(_dateVal, format)
end function

// str: convert a date (string or number) to a string with the given format.
// If format is not specified, uses standard SQL date/time format.
// For details on the format specifiers you can use, see:
// https://docs.microsoft.com/en-us/dotnet/standard/base-types/custom-date-and-time-format-strings
str = function(date, format="yyyy-MM-dd HH:mm:ss")
	return _dateStr(date, format)
end function

// val: convert a date string to a number.
// If no date string is given, returns the current time (same as nowVal).
val = function(dateStr)
	return _dateVal(dateStr)
end function

// year: get the year of the given date, as a number.
year = function(date)
	return _dateStr(date, "yyyy").val
end function

// month: get the month of the given date, as a number from 1 to 12.
month = function(date)
	return _dateStr(date, "%M").val
end function

// day: get the day of the given date, as a number from 1 to 31.
day = function(date)
	return _dateStr(date, "%d").val
end function

// hour: get the hour of the given date/time, as a number from 0 to 23.
hour = function(date)
	return _dateStr(date, "%H").val
end function

// minute: get the minute of the given date/time, as a number from 0 to 59.
minute = function(date)
	return _dateStr(date, "%m").val
end function

// second: get the second of the given date, as a number from 0 to 59.
second = function(date)
	return _dateStr(date, "%s").val
end function

// Weekday name abbreviations, in the order returned by the weekday function.
// DO NOT CHANGE THESE, or the weekday function will no longer work.
weekdayNames = "Sun Mon Tue Wed Thu Fri Sat".split

// weekday: get the day of the week for the given date, from 0 (Sunday) to 6 (Saturday).
weekday = function(date)
	return weekdayNames.indexOf(_dateStr(date, "ddd"))
end function

runUnitTests = function
	print "It is now: " + now
	print "Unit testing: dateTime"
	
	errorCount = 0
	assertEqual = function(actual, expected, note)
		if actual != expected then
			print "Unit test failure (" + note + "): expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function
	
	dstr = "1971-10-29 01:23:45"
	dval = val(dstr)
	assertEqual dval, -889137375
	assertEqual year(dval), 1971, "year (from val)"
	assertEqual month(dval), 10, "month (from val)"
	assertEqual day(dval), 29, "day (from val)"
	assertEqual hour(dval), 1, "hour (from val)"
	assertEqual minute(dval), 23, "minute (from val)"
	assertEqual second(dval), 45, "second (from val)"
	assertEqual weekday(dval), 5, "weekday (from val)"  // (Friday)

	assertEqual year(dstr), 1971, "year (from str)"
	assertEqual month(dstr), 10, "month (from str)"
	assertEqual day(dstr), 29, "day (from str)"
	assertEqual hour(dstr), 1, "hour (from str)"
	assertEqual minute(dstr), 23, "minute (from str)"
	assertEqual second(dstr), 45, "second (from str)"
	assertEqual weekday(dstr), 5, "weekday (from str)"  // (Friday)
	
	nextDay = str(dval + 24*60*60)
	assertEqual nextDay, "1971-10-30 01:23:45"
	
	if errorCount == 0 then
		print "All tests passed.  Happy day!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if
end function


if locals == globals then
	runUnitTests
	// Normally, dateTime would not be run in the global space, so its
	// special str and val methods would not hide the intrinsics... but
	// since it was, let's remove those so as to not cause grief.
	dateTime = {"str":@str, "val":@val}
	globals.remove "str"
	globals.remove "val"
end if
	