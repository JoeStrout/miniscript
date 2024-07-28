import "qa"

testDir = "tests/"

testFileName = function
	p = "a/b/"
	n = file.name(p)
	qa.assertEqual n, "b"
	qa.assertEqual p, "a/b/"
end function

if refEquals(locals, globals) then testFileName
