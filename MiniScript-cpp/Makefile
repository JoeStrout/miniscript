CC=gcc
CXX=g++
RM=rm -f

INC=-Isrc -Isrc/editline -Isrc/MiniScript

CFLAGS=-fsigned-char $(INC)
CXXFLAGS=-std=c++14 $(INC)
CPPFLAGS=-MMD -MP
LIBS=-lstdc++ -lm

CONFIG:=Release
ifeq ($(CONFIG), Debug)
CFLAGS:=$(CFLAGS) -O0 -ggdb3
CXXFLAGS:=$(CXXFLAGS) -O0 -ggdb3
else
CFLAGS:=$(CFLAGS) -Ofast
CXXFLAGS:=$(CXXFLAGS) -Ofast
endif

TOOL_EXE=miniscript
PREFIX=/usr/local/bin

TOOL_SRC=src/ShellIntrinsics.cpp src/OstreamSupport.cpp src/main.cpp
TOOL_OBJ=$(TOOL_SRC:.cpp=.o)

MINISCRIPT_SRC=src/MiniScript/SplitJoin.cpp src/MiniScript/MiniscriptInterpreter.cpp src/MiniScript/MiniscriptIntrinsics.cpp src/MiniScript/SimpleString.cpp src/MiniScript/QA.cpp src/MiniScript/MiniscriptParser.cpp src/MiniScript/UnicodeUtil.cpp src/MiniScript/MiniscriptTAC.cpp src/MiniScript/MiniscriptTypes.cpp src/MiniScript/List.cpp src/MiniScript/SimpleVector.cpp src/MiniScript/MiniscriptKeywords.cpp src/MiniScript/UnitTest.cpp src/MiniScript/Dictionary.cpp src/MiniScript/MiniscriptLexer.cpp
MINISCRIPT_OBJ=$(MINISCRIPT_SRC:.cpp=.o)

EDITLINE_SRC=src/editline/complete.c src/editline/editline.c src/editline/sysunix.c
EDITLINE_OBJ=$(EDITLINE_SRC:.c=.o)

OBJS=$(TOOL_OBJ) $(MINISCRIPT_OBJ) $(EDITLINE_OBJ)
DEPS=$(OBJS:.o=.d)

.PHONY: all clean distclean install

all: $(TOOL_EXE)

$(TOOL_EXE): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	$(RM) $(OBJS) $(DEPS)

distclean: clean
	$(RM) $(TOOL_EXE)
	$(RM) $(PREFIX)/$(TOOL_EXE)

install: $(TOOL_EXE)
	chmod +x $(TOOL_EXE)
	install $(TOOL_EXE) $(PREFIX)

-include $(DEPS)
