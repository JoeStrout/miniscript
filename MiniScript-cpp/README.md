# MiniScript -- C++ Source

This folder contains the source code of the C++ implementation of the [MiniScript scripting language](http://miniscript.org), including the command-line host program.

## Building for macOS

Open the Xcode project (MiniScript.xcodeproj).  Build.

(Note that if you run within XCode, it will work, but every input keystroke in the console will be doubled, because the readline library doesn't quite work properly with Xcode's console.  It works fine in a real Terminal window though.)

You can also follow the Linux procedure, if you prefer command-line tools.

## Building for Linux

Prerequisites: You will need make, gcc, and g++ installed.

Then use `make` in the directory containing this README to build the miniscript executable.  The executable, `miniscript` should appear in the same directory.

Other make options you can use:

- `make install`: ensures the executable has the proper `x` bits set, then installs a symbolic link to it in /usr/local/bin.  Note that if you move the executable (or the directory it's in) somewhere else on your file system, this symbolic link will no longer work.  You can fix it manually, or by running `make install` again from the new location.

- `make clean`: deletes all the object (.o) files from the directory.  You shouldn't need this unless something goes wrong during the normal `make` process.

- `make uninstall`: deletes the executable and the symbolic link in /usr/local/bin.

## Building for Windows

Install the [Visual Studio Command-Line Tools](https://docs.microsoft.com/en-us/cpp/build/walkthrough-compiling-a-native-cpp-program-on-the-command-line?view=vs-2019), and then you can build by `cd`ing into the `src` directory, and using this command:

`cl /EHsc /wd4068 *.cpp MiniScript/*.cpp /Feminiscript.exe`

The output will be called `miniscript.exe` and located in the same directory.

## About the lib folder

The **lib** folder contains importable scripts (modules) for use with the [import](https://miniscript.org/wiki/Import) command.  MiniScript will look for these in any directory in the `MS_IMPORT_PATH` environment variable.  If undefined, a value of `$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib` will be used — that is, it will look for import modules first next to the current script, and then in a `lib` folder next to the executable.
