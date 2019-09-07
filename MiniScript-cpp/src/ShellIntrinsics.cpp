//
//  ShellIntrinsics.cpp
//  MiniScript
//
//  Created by Joe Strout on 3/9/19.
//  Copyright © 2019 Joe Strout. All rights reserved.
//

#include "ShellIntrinsics.h"
#include <iostream>
#include <fstream>
#include "MiniScript/String.h"
#include "MiniScript/UnicodeUtil.h"
#include "MiniScript/UnitTest.h"
#include "MiniScript/SimpleVector.h"
#include "MiniScript/List.h"
#include "MiniScript/Dictionary.h"
#include "MiniScript/MiniscriptParser.h"
#include "MiniScript/MiniscriptInterpreter.h"
#include "OstreamSupport.h"
#include "MiniScript/SplitJoin.h"

#include <stdio.h>
#if _WIN32 || _WIN64
	// ToDo!
#else
	#include <unistd.h>
	#include <dirent.h>		// for readdir
	#include <libgen.h>		// for basename and dirname
#endif

using namespace MiniScript;

bool exitASAP = false;
int exitResult = 0;
ValueList shellArgs;

static Dictionary<Value, FILE*, HashValue> openFileMap;

// hidden (unnamed) intrinsics, only accessible via other methods (such as the File module)
Intrinsic *i_getcwd = NULL;
Intrinsic *i_chdir = NULL;
Intrinsic *i_readdir = NULL;
Intrinsic *i_basename = NULL;
Intrinsic *i_dirname = NULL;
Intrinsic *i_child= NULL;
Intrinsic *i_rename = NULL;
Intrinsic *i_remove = NULL;
Intrinsic *i_fopen = NULL;
Intrinsic *i_fclose = NULL;
Intrinsic *i_isOpen = NULL;
Intrinsic *i_fwrite = NULL;
Intrinsic *i_fwriteLine = NULL;
Intrinsic *i_fread = NULL;
Intrinsic *i_freadLine = NULL;
Intrinsic *i_feof = NULL;

static ValueDict& FileHandleClass();

static IntrinsicResult intrinsic_input(Context *context, IntrinsicResult partialResult) {
	Value prompt = context->GetVar("prompt");
	
	if (useEditline) {
		char *buf;
		buf = readline(prompt.ToString().c_str());
		if (buf == NULL) return IntrinsicResult(Value::emptyString);
		String s(buf);
		free(buf);
		return IntrinsicResult(s);
	} else {
		std::cout << prompt.ToString();
		char buf[1024];
		if (not std::cin.getline(buf, sizeof(buf))) return IntrinsicResult::Null;
		return IntrinsicResult(String(buf));
	}
}

static IntrinsicResult intrinsic_shellArgs(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(shellArgs);
}


static IntrinsicResult intrinsic_exit(Context *context, IntrinsicResult partialResult) {
	exitASAP = true;
	Value resultCode = context->GetVar("resultCode");
	if (!resultCode.IsNull()) exitResult = (int)resultCode.IntValue();
	context->vm->Stop();
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_getcwd(Context *context, IntrinsicResult partialResult) {
	char buf[1024];
	getcwd(buf, sizeof(buf));
	return IntrinsicResult(String(buf));
}

static IntrinsicResult intrinsic_chdir(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::zero);
	String pathStr = path.ToString();
	bool ok = false;
	if (!pathStr.empty()) {
		if (chdir(pathStr.c_str()) == 0) ok = true;
	}
	return IntrinsicResult(Value::Truth(ok));
}

static IntrinsicResult intrinsic_readdir(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	String pathStr = path.ToString();
	if (path.IsNull() || pathStr.empty()) pathStr = ".";
	ValueList result;
	DIR *dir = opendir(pathStr.c_str());
	if (dir != NULL) {
		while (struct dirent *entry = readdir(dir)) {
			String name(entry->d_name);
			if (name == "." || name == "..") continue;
			result.Add(name);
		}
	}
	closedir(dir);
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_basename(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::zero);
	String pathStr = path.ToString();
	String result(basename((char*)pathStr.c_str()));
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_dirname(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::zero);
	String pathStr = path.ToString();
	String result(dirname((char*)pathStr.c_str()));
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_child(Context *context, IntrinsicResult partialResult) {
	String path = context->GetVar("parentPath").ToString();
	String filename = context->GetVar("childName").ToString();
	if (path.EndsWith("/")) return IntrinsicResult(path + filename);
	return IntrinsicResult(path + "/" + filename);
}

static IntrinsicResult intrinsic_rename(Context *context, IntrinsicResult partialResult) {
	String oldPath = context->GetVar("oldPath").ToString();
	String newPath = context->GetVar("newPath").ToString();
	int err = rename(oldPath.c_str(), newPath.c_str());
	return IntrinsicResult(Value::Truth(err == 0));
}

static IntrinsicResult intrinsic_remove(Context *context, IntrinsicResult partialResult) {
	String path = context->GetVar("path").ToString();
	int err = remove(path.c_str());
	return IntrinsicResult(Value::Truth(err == 0));
}

static IntrinsicResult intrinsic_fopen(Context *context, IntrinsicResult partialResult) {
	String path = context->GetVar("path").ToString();
	Value modeVal = context->GetVar("mode");
	String mode = modeVal.ToString();
	FILE *handle;
	if (modeVal.IsNull() || mode.empty() || mode == "rw+") {
		// special case: open for reading/updating, creating it if it doesn't exist
		handle = fopen(path.c_str(), "r+");
		if (handle == NULL) handle = fopen(path.c_str(), "w+");
	} else {
		handle = fopen(path.c_str(), mode.c_str());
	}
	if (handle == NULL) return IntrinsicResult::Null;
	
	ValueDict instance;
	instance.SetValue("__isa", FileHandleClass());
	Value result(instance);
	openFileMap.SetValue(result, handle);
	
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_fclose(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	FILE *handle;
	if (!openFileMap.Get(self, &handle)) return IntrinsicResult(Value::zero);
	fclose(handle);
	openFileMap.Remove(self);
	return IntrinsicResult(Value::one);
}

static IntrinsicResult intrinsic_isOpen(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	return IntrinsicResult(Value::Truth(openFileMap.ContainsKey(self)));
}

static IntrinsicResult intrinsic_fwrite(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String data = context->GetVar("data").ToString();
	FILE *handle;
	if (!openFileMap.Get(self, &handle)) return IntrinsicResult(Value::zero);
	size_t written = fwrite(data.c_str(), 1, data.sizeB(), handle);
	return IntrinsicResult((int)written);
}

static IntrinsicResult intrinsic_fwriteLine(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String data = context->GetVar("data").ToString();
	FILE *handle;
	if (!openFileMap.Get(self, &handle)) return IntrinsicResult(Value::zero);
	size_t written = fwrite(data.c_str(), 1, data.sizeB(), handle);
	written += fwrite("\n", 1, 1, handle);
	return IntrinsicResult((int)written);
}

static IntrinsicResult intrinsic_fread(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	long bytesToRead = context->GetVar("byteCount").IntValue();
	if (bytesToRead == 0) return IntrinsicResult(Value::emptyString);

	FILE *handle;
	if (!openFileMap.Get(self, &handle)) return IntrinsicResult::Null;

	// If bytesToRead < 0, read to EOF.
	// Otherwise, read bytesToRead bytes (1k at a time).
	char buf[1024];
	String result;
	while (!feof(handle) && (bytesToRead != 0)) {
		size_t read = fread(buf, 1, bytesToRead > 0 && bytesToRead < 1024 ? bytesToRead : 1024, handle);
		if (bytesToRead > 0) bytesToRead -= read;
		result += String(buf, read);
	}
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_feof(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	
	FILE *handle;
	if (!openFileMap.Get(self, &handle)) return IntrinsicResult::Null;
	return IntrinsicResult(Value::Truth(feof(handle) != 0));
}

static IntrinsicResult intrinsic_freadLine(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	FILE *handle;
	if (!openFileMap.Get(self, &handle)) return IntrinsicResult::Null;

	char buf[1024];
	char *str = fgets(buf, sizeof(buf), handle);
	if (str == NULL) return IntrinsicResult::Null;
	// Grr... we need to strip the terminating newline.
	// Still probably faster than reading character by character though.
	for (int i=0; i<1024; i++) {
		if (buf[i] == '\n') {
			buf[i] = 0;
			break;
		}
	}
	String result(buf);
	
	return IntrinsicResult(result);
}


static IntrinsicResult intrinsic_File(Context *context, IntrinsicResult partialResult) {
	static ValueDict fileModule;
	
	if (fileModule.Count() == 0) {
		fileModule.SetValue("curdir", i_getcwd->GetFunc());
		fileModule.SetValue("setdir", i_chdir->GetFunc());
		fileModule.SetValue("children", i_readdir->GetFunc());
		fileModule.SetValue("name", i_basename->GetFunc());
		fileModule.SetValue("parent", i_dirname->GetFunc());
		fileModule.SetValue("child", i_child->GetFunc());
		fileModule.SetValue("move", i_rename->GetFunc());
		fileModule.SetValue("delete", i_remove->GetFunc());
		fileModule.SetValue("open", i_fopen->GetFunc());
	}
	
	return IntrinsicResult(fileModule);
}


static ValueDict& FileHandleClass() {
	static ValueDict result;
	if (result.Count() == 0) {
		result.SetValue("close", i_fclose->GetFunc());
		result.SetValue("isOpen", i_isOpen->GetFunc());
		result.SetValue("write", i_fwrite->GetFunc());
		result.SetValue("writeLine", i_fwriteLine->GetFunc());
		result.SetValue("read", i_fread->GetFunc());
		result.SetValue("readLine", i_freadLine->GetFunc());
		result.SetValue("atEnd", i_feof->GetFunc());
	}
	
	return result;
}

/// Add intrinsics that are not part of core MiniScript, but which make sense
/// in this command-line environment.
void AddShellIntrinsics() {
	Intrinsic *f;
	
	f = Intrinsic::Create("exit");
	f->AddParam("resultCode");
	f->code = &intrinsic_exit;
	
	f = Intrinsic::Create("shellArgs");
	f->code = &intrinsic_shellArgs;
	
	f = Intrinsic::Create("input");
	f->AddParam("prompt", "");
	f->code = &intrinsic_input;

	f = Intrinsic::Create("File");
	f->code = &intrinsic_File;

	i_getcwd = Intrinsic::Create("");
	i_getcwd->code = &intrinsic_getcwd;
	
	i_chdir = Intrinsic::Create("");
	i_chdir->AddParam("path");
	i_chdir->code = &intrinsic_chdir;
	
	i_readdir = Intrinsic::Create("");
	i_readdir->AddParam("path");
	i_readdir->code = &intrinsic_readdir;
	
	i_basename = Intrinsic::Create("");
	i_basename->AddParam("path");
	i_basename->code = &intrinsic_basename;
	
	i_dirname = Intrinsic::Create("");
	i_dirname->AddParam("path");
	i_dirname->code = &intrinsic_dirname;
	
	i_child = Intrinsic::Create("");
	i_child->AddParam("parentPath");
	i_child->AddParam("childName");
	i_child->code = &intrinsic_child;
	
	i_rename = Intrinsic::Create("");
	i_rename->AddParam("oldPath");
	i_rename->AddParam("newPath");
	i_rename->code = &intrinsic_rename;
	
	i_remove = Intrinsic::Create("");
	i_remove->AddParam("path");
	i_remove->code = &intrinsic_remove;
	
	i_fopen = Intrinsic::Create("");
	i_fopen->AddParam("path");
	i_fopen->AddParam("mode");
	i_fopen->code = &intrinsic_fopen;

	i_fclose = Intrinsic::Create("");
	i_fclose->code = &intrinsic_fclose;
	
	i_isOpen = Intrinsic::Create("");
	i_isOpen->code = &intrinsic_isOpen;
	
	i_fwrite = Intrinsic::Create("");
	i_fwrite->AddParam("data");
	i_fwrite->code = &intrinsic_fwrite;
	
	i_fwriteLine = Intrinsic::Create("");
	i_fwriteLine->AddParam("data");
	i_fwriteLine->code = &intrinsic_fwriteLine;
	
	i_fread = Intrinsic::Create("");
	i_fread->AddParam("byteCount", -1);
	i_fread->code = &intrinsic_fread;
	
	i_freadLine = Intrinsic::Create("");
	i_freadLine->code = &intrinsic_freadLine;
	
	i_feof = Intrinsic::Create("");
	i_feof->code = &intrinsic_feof;

}
