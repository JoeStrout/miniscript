# MiniScript -- C++ Source

This folder contains the source code of the C++ implementation of the [MiniScript scripting language](http://miniscript.org), including the command-line host program.

## Building

MiniScript is built with [CMake](https://cmake.org/). You can generate your desired flavour of build files as usual from either the CMake GUI or using `cmake` on the command line. If you are unfamiliar with CMake and want to build right now, use the GUI, or follow the command-line steps below.

1. Create a directory in this (MiniScript-cpp) folder called `build`.  (Don't worry, it will be ignored by git.)

2. `cd build` to change to that build directory.

3. `cmake ../..` (or on Windows, `cmake ..\..`) to generate a makefile and related files in the build directory.

4. `cmake --build . --config Release` to actually do the build.

If successful, you should find an executable called `miniscript` (or `miniscript.exe` on Windows) which you can install (see **Installation**, below).  You'll also find a shared library (libminiscript-cpp.a, miniscript-cpp.lib, or similar) for you to link to in your own projects if you like.  You can even `include()` the CMakeLists.txt of this project inside your own for clean dependency management.

If you are only interested in the C# edition of MiniScript, there is a project file provided in the respective directory.

### CMake Build Options

Options can be controlled in the usual way - either within the CMake GUI or by passing `-D<OPTION>=ON` from your terminal.

#### MINISCRIPT_BUILD_TESTING

This option controls whether or not unit tests binaries are built and added to CTest. For an overview of the flags passed to the binaries to cause them to execute tests, take a look in the testing section at the bottom of the `CMakeLists.txt` - however, rather than doing this, you can simply run `ctest` after building. Most IDEs integrate with CMake/CTest and will detect the tests. If you generated a multi-configuration build (such as a VS project) you would need to run `ctest -C <Debug/Release>`


## Installation

You can copy (or symlink) the `miniscript` executable anywhere in your search path, and either put the `lib` folder next to it, or point to the `lib` folder with an environment variable (see below).  For a standard Unix/Linux-style installation, and assuming you are currently in the `build` folder:

1. `ln -s "$(pwd)/miniscript" /usr/local/bin/miniscript` to get a symbolic link into the /usr/local/bin folder

2. `ln -s ../lib lib` to put a link to the `lib` folder next to the executable.


## About the lib folder

The **lib** folder contains importable scripts (modules) for use with the [import](https://miniscript.org/wiki/Import) command.  MiniScript will look for these in any directory in the `MS_IMPORT_PATH` environment variable.  If undefined, a value of `$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib` will be used — that is, it will look for import modules first next to the current script, then in a `lib` folder next to the current script, and finally  in a `lib` folder next to the executable.

The installation steps in the previous section place (a symbolic link to) the `lib` folder next to the executable, which will be found via `$MS_EXE_DIR/lib`.


## Quick Test

To ensure you've correctly built and installed command-line MiniScript:

1. `cd` to your home directory, or any other place on your system
2. `miniscript` should launch command-line MiniScript, looking something like:
```
MiniScript 
Command-Line (Unix) v1.21; language v1.6.1 (Jun 30 2023)
> 
```
3. `import "dateTime"` (at the MiniScript `>` prompt) should return no errors
4. `dateTime.now` should print the current date and time
5. `exit` (or contral-D) should exit MiniScript and return you to the shell.


