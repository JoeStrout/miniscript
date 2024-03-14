print "I was imported"
import "ImportTest\test\nested"
print nested.val
print "I was imported END"
import "ImportTest\importreturns"
print importreturns.val