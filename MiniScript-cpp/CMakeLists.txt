cmake_minimum_required(VERSION 3.22)
project(MiniScript_cpp LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 14)

#####  common comp flags #####

add_compile_options(-fsigned-char -Wall)

##############################

set(RELEASE_FLAGS "-Ofast -DNDEBUG")
set(DEBUG_FLAGS "-O0 -ggdb3")

set(CMAKE_CXX_FLAGS_RELEASE ${RELEASE_FLAGS})
set(CMAKE_C_FLAGS_RELEASE ${RELEASE_FLAGS})

set(CMAKE_CXX_FLAGS_DEBUG ${DEBUG_FLAGS})
set(CMAKE_C_FLAGS_DEBUG ${DEBUG_FLAGS})

set(SOURCES
        src/main.cpp
        src/OstreamSupport.cpp
        src/ShellIntrinsics.cpp
        src/editline/complete.c
        src/editline/editline.c
        src/editline/sysunix.c
        src/MiniScript/Dictionary.cpp
        src/MiniScript/List.cpp
        src/MiniScript/MiniscriptInterpreter.cpp
        src/MiniScript/MiniscriptIntrinsics.cpp
        src/MiniScript/MiniscriptKeywords.cpp
        src/MiniScript/MiniscriptLexer.cpp
        src/MiniScript/MiniscriptParser.cpp
        src/MiniScript/MiniscriptTAC.cpp
        src/MiniScript/MiniscriptTypes.cpp
        src/MiniScript/QA.cpp
        src/MiniScript/SimpleString.cpp
        src/MiniScript/SimpleVector.cpp
        src/MiniScript/SplitJoin.cpp
        src/MiniScript/UnicodeUtil.cpp
        src/MiniScript/UnitTest.cpp
        )

add_executable(miniscript ${SOURCES})
target_include_directories(miniscript PRIVATE src src/editline src/MiniScript)
target_link_libraries(miniscript PRIVATE stdc++ m)

install(TARGETS miniscript DESTINATION bin)
