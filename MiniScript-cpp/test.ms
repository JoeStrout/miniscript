score = 0
addToScore = function(d=1)
	score = score + d
	print "local: " + score
end function
addToScore
print "global:" + score
