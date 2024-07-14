import "qa"

testDateTimeDateStr = function
	qa.assertEqual _dateStr(1e20), ""
end function

if refEquals(locals, globals) then testDateTimeDateStr
