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
	
	r.resize 7
	r.setUtf8 0, "hello world"
	qa.assertEqual r.utf8(0), "hello w"
	
end function
