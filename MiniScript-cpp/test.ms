// Quick test file used during development.
print "starting test"
startTime = time

fib = function(n)
    if (n < 2)  then
       return n
    else
       return (fib(n-2) + fib(n-1))
end function

//N = 34
N = 30
print fib(N)
endTime = time
print "Time: " + (endTime - startTime)
