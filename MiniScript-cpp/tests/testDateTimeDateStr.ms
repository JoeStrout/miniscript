import "qa"

testDateTimeDateStr = function
	qa.assertEqual _dateStr(0), "2000-01-01 00:00:00"
	qa.assertEqual _dateStr(1e20), ""
end function

if refEquals(locals, globals) then testDateTimeDateStr
