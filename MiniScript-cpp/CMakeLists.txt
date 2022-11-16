cmake_minimum_required(VERSION 3.22)
project(MiniScript_cpp LANGUAGES CXX C)

if(MSVC)
    add_compile_options("/EHsc" "/wd4068" "/W4" "$<$<CONFIG:RELEASE>:/O2>" "$<$<CONFIG:DEBUG>:>")
    add_link_options("$<$<CONFIG:DEBUG>:>")
else()
    add_compile_options("-fsigned-char" "-Wall" "$<$<CONFIG:RELEASE>:-O3>" "$<$<CONFIG:DEBUG>:-O0>" "$<$<CONFIG:DEBUG>:-ggdb3>")
    set(LIBS stdc++ m)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options("-stdlib=libc++")
    else()
        # nothing special for gcc at the moment
    endif()
endif()

##############################

set(SOURCES
        src/main.cpp
        src/OstreamSupport.cpp
        src/ShellIntrinsics.cpp
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

if(UNIX)
    set(SOURCES
            ${SOURCES}
            src/editline/complete.c
            src/editline/editline.c
            src/editline/sysunix.c
            )
endif()

add_executable(miniscript ${SOURCES})
target_compile_features(miniscript PRIVATE cxx_std_14)
target_include_directories(miniscript PRIVATE src src/editline src/MiniScript)
target_link_libraries(miniscript PRIVATE ${LIBS})

install(TARGETS miniscript DESTINATION bin)
