# MiniScript -- C++ Source

This folder contains the source code of the C++ implementation of the [MiniScript scripting language](http://miniscript.org), including the command-line host program.

## Building for macOS

Open the Xcode project (MiniScript.xcodeproj).  Build.

(Note that if you run within XCode, it will work, but every input keystroke in the console will be doubled, because the readline library doesn't quite work properly with Xcode's console.  It works fine in a real Terminal window though.)

You can probably also follow the Linux procedure, if you prefer command-line tools.


## Building for Linux

Prerequisites: You will need make, gcc, and g++ installed.

Then use `make` to build the miniscript executable.  You can also `make install` to install it in /usr/local/bin, or `make clean` to clean up.  


## Building for Windows

Install the [Visual Studio Command-Line Tools](https://docs.microsoft.com/en-us/cpp/build/walkthrough-compiling-a-native-cpp-program-on-the-command-line?view=vs-2019), and then you can build by `cd`ing into the `src` directory, and using this command:

`cl /EHsc /wd4068 *.cpp MiniScript/*.cpp /Feminiscript.exe`

The output will be called `miniscript.exe` and located in the same directory.


