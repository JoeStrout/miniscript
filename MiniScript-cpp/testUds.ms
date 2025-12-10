import "qa"

SOCKET = "uds.sock"

_testServer = function(server)
	qa.assertEqual server.isOpen, true
	
	conn = server.accept(60)
	qa.assertEqual conn != null, true
	
	msg = conn.receive
	qa.assertEqual msg.utf8, "foo"
	
	n = conn.send("bar")
	qa.assertEqual n, 3
	
	msg = conn.receive(1)
	qa.assertEqual msg.utf8, "b"
	msg = conn.receive(1)
	qa.assertEqual msg.utf8, "a"
	msg = conn.receive(10, 0)
	qa.assertEqual msg.utf8, "mf"
	
	n = conn.send("hello", 1)
	qa.assertEqual n, 4
	
	msg = conn.receive
	qa.assertEqual msg.utf8, "bye"
end function

_testClient = function(client)
	qa.assertEqual client.isOpen, true
	
	n = client.send("foo")
	qa.assertEqual n, 3
	
	msg = client.receive
	qa.assertEqual msg.utf8, "bar"
	
	r = new RawData
	r.resize 4
	r.setUtf8 0, "bamf"
	n = client.send(r)
	qa.assertEqual n, 4
	
	msg = client.receive
	qa.assertEqual msg.utf8, "ello"
	
	r = new RawData
	r.resize 7
	r.setUtf8 0, "goodbye"
	n = client.send(r, 4)
	qa.assertEqual n, 3
end function

testUds = function
	client = uds.connect(SOCKET, 0)
	if client == null then
		server = uds.createServer(SOCKET)
		qa.assertEqual server != null, true
		
		//
		print " *** Server awaits at " + SOCKET
		print " ***"
		print " *** Please, launch in parallel another instance of `testUds.ms`..."
		print " ***"
		//
		
		_testServer server
		
		//
		print " *** OK"
		//
		
	else
		
		//
		print " *** Client connected"
		//
		
		_testClient client
		
		//
		print " *** OK"
		//
		
	end if
end function

if refEquals(locals, globals) then testUds
