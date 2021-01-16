// Quick test file used during development.
print "test.ms loaded.  Hello, world!"
print "Working directory: " + file.curdir

// Create a file to read.
file.writeLines "fileToRead.txt", ["Hey", "Ho", "Hoo-ah"]

// Now, we're going to try opening a file for reading and
// opening a file for writing at the same time.
reader = file.open("fileToRead.txt", "r")
writer = file.open("fileToWrite.txt", "w")

// And let's just copy the lines from one to the other, with modification.
while true
	line = reader.readLine
	if line == null then break
	print "Copying: " + line + " -> " + line.upper
	writer.writeLine line.upper
end while

print "All done!"
