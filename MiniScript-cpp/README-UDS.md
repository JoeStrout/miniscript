# Unix domain sockets

[Unix domain sockets](https://en.wikipedia.org/wiki/Unix_domain_socket) (or inter-process communication sockets) is a mean to exchange data between two MiniScript programs running on the same machine or between a MiniScript program and some other program that also supports unix domain sockets (reverse proxies, databases etc).

The MiniScript shell (interpreter) provides an intrinsic `uds` module with functions and classes that help writing both client and server side of UDS communication.

Steps to write a client:

* Call `uds.connect()` -- it returns a connection object (`uds.Connection`).
* Call connection's `send()` and `receive()` methods to exchange data with the server.

Steps to write a server:

* Call `uds.createServer()` -- it returns a server object (`uds.Server`).
* Call server's `accept()` method -- it returns a connection object when a new client connects (`uds.Connection`).
* Call connection's `send()` and `receive()` methods to exchange data with the client.

Both `connect()` and `createServer()` accept a path to a socket-file used as the connection point. The server will create it if it doesn't exist.

Methods `uds.connect()`, `connection.receive()` and `server.accept()` have an optional `timeout` parameter (in seconds). They return as soon as the result is ready or as the timeout expires (in which case they return `null`). If the timeout is zero, they return immediately in a non-blocking fashion. If the timeout is negative, these methods will block till the result is ready. The default value is 30 seconds.


## Example

Simple echo server:

```c
srv = uds.createServer("echo.sock")
while true
	c = srv.accept(-1)
	msg = c.receive
	print "received " + msg.utf8
	c.send msg
	c.close
end while
```

Client:

```c
c = uds.connect("echo.sock")
c.send "foo"
msg = c.receive
print msg.utf8  // prints: foo
c.close
```


## API - Creating client and server

#### Function uds.connect()

`uds.connect(sockPath, timeout=30) -> uds.Connection or null`

Creates a connection to a server on the client side. (In C++ it calls *socket()* and *connect()*.)

The `sockPath` parameter is a path to a socket file (connection point).

The `timeout` parameter tells how many seconds to wait for the server to accept us (`0` -- return immediately, `-1` -- wait indefinitely).

If successful, a connection object is returned (an instance of `uds.Connection`).

Otherwise, the result is `null`.


#### Function uds.createServer()

`uds.createServer(sockPath, backlog=20) -> uds.Server or null`

Creates a server object. (In C++ it calls *socket()*, *bind()* and *listen()*.)

The `sockPath` parameter is a path to a socket file (connection point).

If the socket file doesn't exist, this function creates it.

If the socket file exists, this function deletes it and creates a new one.

The `backlog` parameter (the maximal number of queued connections) will be passed to C++'s *listen()*.

If successful, a server object is returned (an instance of `uds.Server`).

Otherwise, the result is `null`.


## API - Connection

#### Function connection.isOpen()

`connection.isOpen() -> true or false`

Returns whether the underlying socket is still open.

#### Function connection.close()

`connection.close()`

Closes the underlying socket. (In C++ it calls *close()* in unixes or *closesocket()* in Windows.)

#### Function connection.send()

`connection.send(rawData, offset=0) -> nBytes`

Sends a message over a socket. (In C++ it calls *send()*.)

The `rawData` parameter can either be a `RawData` object or a string.

The optional `offset` parameter tells from which byte in `rawData` to start.

It returns the number of successfully sent bytes or `-1` (basically, the result of C++'s *send()*).

#### Function connection.receive()

`connection.receive(bytes=-1, timeout=30) -> RawData or null`

Receives bytes from a socket. (In C++ it calls *recv()*.)

The `bytes` parameter is the amount of bytes to read from a socket. If `-1`, it returns all available bytes.

The `timeout` parameter tells how many seconds to wait for the bytes to arrive (`0` -- return immediately, `-1` -- wait indefinitely).

If there are bytes, it returns a `RawData` object.

Otherwise, the result is `null`.


## API - Server

#### Function server.isOpen()

`server.isOpen() -> true or false`

Returns whether the underlying socket is still open.

#### Function server.accept()

`server.accept(timeout=30) -> Connection`

Accepts a request for an incoming connection. (In C++ it calls *accept()*.)

The `timeout` parameter tells how many seconds to wait for a connection request (`0` -- return immediately, `-1` -- wait indefinitely).

If successful, a connection object is returned (an instance of `uds.Connection`).

Otherwise, the result is `null`.


## Test

To test `uds` launch two virtual terminals and run in each of them `miniscript testUds.ms`.
