import "qa"

testDaylight = function
	
	// This test always only tests a half of functionality depending on whether the host system has "daylight savings" on or off.
	
	d = "2000-01-01 11:00:00"
	qa.assertEqual _dateStr(d, "%H"), "11"
	
	d = "2000-07-01 11:00:00"
	qa.assertEqual _dateStr(d, "%H"), "11"
end function

if refEquals(locals, globals) then testDaylight
