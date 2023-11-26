# MiniScript -- C++ Source

This folder contains the source code of the C++ implementation of the [MiniScript scripting language](http://miniscript.org), including the command-line host program.

## Building

MiniScript is built with CMake. You can generate your desired flavour of build files as usual from either the CMake GUI or using `cmake` on the commandline. If you are unfamiliar with CMake and want to build right now, use the GUI. If you cannot use the GUI, make a directory somewhere and, while in that directory, run `cmake path/to/miniscript` followed by `cmake --build`

Miniscript itself will be output as a shared library for you to link to. You can even `include()` the CMakeLists.txt of this project inside your own for clean dependency management.

If you are only interested in the C# edition of MiniScript, there is a project file provided in the respective directory.

### CMake Build Options

Options can be controlled in the usual way - either within the CMake GUI or by passing `-D<OPTION>=ON` from your terminal.

#### MINISCRIPT_BUILD_TESTING

This option controls whether or not unit tests binaries are built and added to CTest. For an overview of the flags passed to the binaries to cause them to execute tests, take a look in the testing section at the bottom of the `CMakeLists.txt` - however, rather than doing this, you can simply run `ctest` after building. Most IDEs integrate with CMake/CTest and will detect the tests. If you generated a multi-configuration build (such as a VS project) you would need to run `ctest -C <Debug/Release>`

### MINISCRIPT_BUILD_CSHARP

This option is only supported when outputting a Visual Studio project. Attempting to enable this in any other environment will likely result in a build error - consult the documentation for your version of CMake as support for other platforms may become possible in the future.

## About the lib folder

The **lib** folder contains importable scripts (modules) for use with the [import](https://miniscript.org/wiki/Import) command.  MiniScript will look for these in any directory in the `MS_IMPORT_PATH` environment variable.  If undefined, a value of `$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib` will be used — that is, it will look for import modules first in a `lib` folder next to the current script, and then in a `lib` folder next to the executable.
