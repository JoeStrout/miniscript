import "qa"

testDir = "tests/"

testFileReadLines = function
	fn = file.child(testDir, "_cr.txt")
	f = file.open(fn, "w")
	f.write("a" + char(13) + "b" + char(13) + "c")
	f.close
	qa.assertEqual file.readLines("tests/_cr.txt"), ["a", "b", "c"]
	
	fn = file.child(testDir, "_lf.txt")
	f = file.open(fn, "w")
	f.write("a" + char(10) + "b" + char(10) + "c")
	f.close
	qa.assertEqual file.readLines("tests/_lf.txt"), ["a", "b", "c"]
	
	fn = file.child(testDir, "_crlf.txt")
	f = file.open(fn, "w")
	f.write("a" + char(13) + char(10) + "b" + char(13) + char(10) + "c")
	f.close
	qa.assertEqual file.readLines("tests/_crlf.txt"), ["a", "b", "c"]
end function

if refEquals(locals, globals) then testFileReadLines
