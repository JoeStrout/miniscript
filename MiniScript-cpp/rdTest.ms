import "qa"

testRawData = function
	
	r = new RawData
	qa.assertEqual r.len, 0
	
	r.resize
	qa.assertEqual r.len, 32
	
	r.resize 10
	qa.assertEqual r.len, 10
	
	// ERROR
	//r.resize -1
	
	r.setByte 3, 100
	qa.assertEqual r.byte(3), 100
	qa.assertEqual r.byte(-7), 100
	qa.assertEqual r.sbyte(3), 100
	qa.assertEqual r.sbyte(-7), 100
	
	r.setSbyte -6, -100
	qa.assertEqual r.byte(4), 156
	qa.assertEqual r.byte(-6), 156
	qa.assertEqual r.sbyte(4), -100
	qa.assertEqual r.sbyte(-6), -100
	
	// ERROR
	//r.setByte 20, 100
	//r.setByte -20, 100
	
	r.setUshort 2, 50000
	qa.assertEqual r.ushort(2), 50000
	qa.assertEqual r.short(2), -15536
	
	r.setShort -8, -15536
	qa.assertEqual r.ushort(2), 50000
	qa.assertEqual r.short(2), -15536
	
	r.setUint 1, 3000000000
	qa.assertEqual r.uint(1), 3000000000
	qa.assertEqual r.int(1), -1294967296
	
	r.setInt -9, -1294967296
	qa.assertEqual r.uint(1), 3000000000
	qa.assertEqual r.int(1), -1294967296
	
	// ERROR
	//r.setInt 9, 100
	
	r.setFloat 6, -7000.25
	qa.assertEqual r.float(6), -7000.25
	
	r.setDouble 0, -7000.25
	qa.assertEqual r.double(-10), -7000.25
	
	r.setUtf8 0, "hello"
	r.setByte 5, 0
	qa.assertEqual r.utf8(0, 5), "hello"
	qa.assertEqual r.utf8(-10, 5), "hello"
	qa.assertEqual r.utf8(1, 3), "ell"
	qa.assertEqual r.utf8(1, 100), null
	
	r.resize 7
	r.setUtf8 0, "hello world"
	qa.assertEqual r.utf8(0), "hello w"
	
	// _sub()
	qa.assertEqual r._sub.utf8, "hello w"
	qa.assertEqual r._sub(0).utf8, "hello w"
	qa.assertEqual r._sub(2).utf8, "llo w"
	qa.assertEqual r._sub(2, 3).utf8, "llo"
	qa.assertEqual r._sub(2, 100), null
	qa.assertEqual r._sub(-5, 3).utf8, "llo"
	
	// _setSub()
	r2 = new RawData
	r2.resize 2
	r2.setUtf8 0, "xx"
	r._setSub 0, r2
	qa.assertEqual r.utf8, "xxllo w"
	r._setSub 3, r2
	qa.assertEqual r.utf8, "xxlxx w"
	r._setSub 6, r2
	qa.assertEqual r.utf8, "xxlxx x"
	
	// _concat()
	qa.assertEqual RawData._concat([]).len, 0
	r1 = new RawData
	r1.resize 4
	r1.setUtf8 0, "hell"
	r2 = new RawData
	r2.resize 4
	r2.setUtf8 0, "o wo"
	r3 = new RawData
	r3.resize 3
	r3.setUtf8 0, "rld"
	qa.assertEqual RawData._concat([r1, r2, r3]).utf8, "hello world"
end function

if refEquals(locals, globals) then testRawData
