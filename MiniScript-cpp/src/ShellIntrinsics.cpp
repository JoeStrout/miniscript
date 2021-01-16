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
	#define WINDOWS 1
	#include <windows.h>
	#include <Shlwapi.h>
	#include <Fileapi.h>
	#include <direct.h>
	#include <time.h>
	#define getcwd _getcwd
	#define setenv _setenv
#else
	#include <fcntl.h>
	#include <unistd.h>
	#include <unistd.h>
	#include <dirent.h>		// for readdir
	#include <libgen.h>		// for basename and dirname
	#include <sys/stat.h>	// for stat
	#include <stdlib.h>		// for realpath
	#if defined(__APPLE__) || defined(__FreeBSD__)
		#include <copyfile.h>
	#else
		#include <sys/sendfile.h>
	#endif
#endif

extern "C" {
	// list of environment variables provided by C standard library:
	extern char **environ;
}

using namespace MiniScript;

bool exitASAP = false;
int exitResult = 0;
ValueList shellArgs;

static Value _handle("_handle");

// RefCountedStorage class to wrap a FILE*
class FileHandleStorage : public RefCountedStorage {
public:
	FileHandleStorage(FILE *file) : f(file) {}
	virtual ~FileHandleStorage() { if (f) fclose(f); }

	FILE *f;
};

// hidden (unnamed) intrinsics, only accessible via other methods (such as the File module)
Intrinsic *i_getcwd = NULL;
Intrinsic *i_chdir = NULL;
Intrinsic *i_readdir = NULL;
Intrinsic *i_basename = NULL;
Intrinsic *i_dirname = NULL;
Intrinsic *i_child = NULL;
Intrinsic *i_exists = NULL;
Intrinsic *i_info = NULL;
Intrinsic *i_mkdir = NULL;
Intrinsic *i_copy = NULL;
Intrinsic *i_readLines = NULL;
Intrinsic *i_writeLines = NULL;
Intrinsic *i_rename = NULL;
Intrinsic *i_remove = NULL;
Intrinsic *i_fopen = NULL;
Intrinsic *i_fclose = NULL;
Intrinsic *i_isOpen = NULL;
Intrinsic *i_fwrite = NULL;
Intrinsic *i_fwriteLine = NULL;
Intrinsic *i_fread = NULL;
Intrinsic *i_freadLine = NULL;
Intrinsic *i_fposition = NULL;
Intrinsic *i_feof = NULL;

#if !_WIN32 && !_WIN64
// Copy a file.  Return 0 on success, or some value < 0 on error.
static int UnixishCopyFile(const char* source, const char* destination) {
	// Based on: https://stackoverflow.com/questions/2180079
	int input, output;
	if ((input = open(source, O_RDONLY)) == -1)	{
		return -1;
	}
	if ((output = creat(destination, 0660)) == -1) {
		close(input);
		return -1;
	}
	
	//Here we use kernel-space copying for performance reasons
#if defined(__APPLE__) || defined(__FreeBSD__)
	//fcopyfile works on FreeBSD and OS X 10.5+
	int result = fcopyfile(input, output, 0, COPYFILE_ALL);
#else
	//sendfile will work with non-socket output (i.e. regular file) on Linux 2.6.33+
	off_t bytesCopied = 0;
	struct stat fileinfo = {0};
	fstat(input, &fileinfo);
	int result = sendfile(output, input, &bytesCopied, fileinfo.st_size);
	if (result > 0) result = 0;  // sendfile returns # of bytes copied; any value >= 0 is success.
#endif
	
	close(input);
	close(output);
	
	return result;
}
#endif

static ValueDict& FileHandleClass();

static IntrinsicResult intrinsic_input(Context *context, IntrinsicResult partialResult) {
	Value prompt = context->GetVar("prompt");
	
	#if useEditline
		char *buf;
		buf = readline(prompt.ToString().c_str());
		if (buf == NULL) return IntrinsicResult(Value::emptyString);
		String s(buf);
		free(buf);
		return IntrinsicResult(s);
	#else
		std::cout << prompt.ToString();
		char buf[1024];
		if (not std::cin.getline(buf, sizeof(buf))) return IntrinsicResult::Null;
		return IntrinsicResult(String(buf));
	#endif
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
	#if _WIN32 || _WIN64
		pathStr += "\\*";
		WIN32_FIND_DATA data;
		HANDLE hFind = FindFirstFile(pathStr.c_str(), &data);
		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				String name(data.cFileName);
				if (name == "." || name == "..") continue;
				result.Add(name);
			} while (FindNextFile(hFind, &data) != 0);
			FindClose(hFind);
		}
	#else
		DIR *dir = opendir(pathStr.c_str());
		if (dir != NULL) {
			while (struct dirent *entry = readdir(dir)) {
				String name(entry->d_name);
				if (name == "." || name == "..") continue;
				result.Add(name);
			}
		}
		closedir(dir);
	#endif
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_basename(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::zero);
	String pathStr = path.ToString();
	#if _WIN32 || _WIN64
		char driveBuf[3];
		char nameBuf[256];
		char extBuf[256];
		_splitpath_s(pathStr.c_str(), driveBuf, sizeof(driveBuf), NULL, 0, nameBuf, sizeof(nameBuf), extBuf, sizeof(extBuf));
		String result = String(nameBuf) + String(extBuf);
    #else
		String result(basename((char*)pathStr.c_str()));
	#endif
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_dirname(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::zero);
	String pathStr = path.ToString();
	#if _WIN32 || _WIN64
		char pathBuf[512];
		_fullpath(pathBuf, pathStr.c_str(), sizeof(pathBuf));
		char driveBuf[3];
		char dirBuf[256];
		_splitpath_s(pathBuf, driveBuf, sizeof(driveBuf), dirBuf, sizeof(dirBuf), NULL, 0, NULL, 0);
		String result = String(driveBuf) + String(dirBuf);
	#else
		String result(dirname((char*)pathStr.c_str()));
	#endif
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_exists(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::null);
	String pathStr = path.ToString();
	#if _WIN32 || _WIN64
		char pathBuf[512];
		_fullpath(pathBuf, pathStr.c_str(), sizeof(pathBuf));
		WIN32_FIND_DATA FindFileData;
		HANDLE handle = FindFirstFile(pathBuf, &FindFileData) ;
		bool found = handle != INVALID_HANDLE_VALUE;
		if (found) FindClose(handle);
	#else
		bool found = (access(pathStr.c_str(), F_OK ) != -1);
	#endif
	return IntrinsicResult(Value::Truth(found));
}

static String timestampToString(const struct tm& t) {
	String result = String::Format(1900 + t.tm_year) + "-";
	if (t.tm_mon < 10) result += "0";
	result += String::Format(1 + t.tm_mon) + "-";
	if (t.tm_mday < 10) result += "0";
	result += String::Format(t.tm_mday) + " ";
	if (t.tm_hour < 10) result += "0";
	result += String::Format(t.tm_hour) + ":";
	if (t.tm_min < 10) result += "0";
	result += String::Format(t.tm_min) + ":";
	if (t.tm_sec < 10) result += "0";
	result += String::Format(t.tm_sec);

	return result;
}

static IntrinsicResult intrinsic_info(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	String pathStr;
	if (!path.IsNull()) pathStr = path.ToString();
	if (pathStr.empty()) {
		char buf[1024];
		getcwd(buf, sizeof(buf));
		pathStr = buf;
	}
	struct tm t;
#if _WIN32 || _WIN64
	char pathBuf[512];
	_fullpath(pathBuf, pathStr.c_str(), sizeof(pathBuf));
	struct _stati64 stats;
	if (_stati64(pathBuf, &stats) != 0) return IntrinsicResult(Value::null);
	ValueDict map;
	map.SetValue("path", String(pathBuf));
	map.SetValue("isDirectory", (stats.st_mode & _S_IFDIR) != 0);
	map.SetValue("size", stats.st_size);
	gmtime_s(&t, &stats.st_mtime);
	Value result(map);
	
#else
	char pathBuf[PATH_MAX];
	realpath(pathStr.c_str(), pathBuf);
	struct stat stats;
	if (stat(pathStr.c_str(), &stats) < 0) return IntrinsicResult(Value::null);
	ValueDict map;
	map.SetValue("path", String(pathBuf));
	map.SetValue("isDirectory", S_ISDIR (stats.st_mode));
	map.SetValue("size", stats.st_size);
	#if defined(__APPLE__) || defined(__FreeBSD__)
		tzset();
		localtime_r(&(stats.st_mtimespec.tv_sec), &t);
	#else
		localtime_r(&stats.st_mtime, &t);
	#endif
	Value result(map);
#endif
	map.SetValue("date", timestampToString(t));
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_mkdir(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::null);
	String pathStr = path.ToString();
#if _WIN32 || _WIN64
	char pathBuf[512];
	_fullpath(pathBuf, pathStr.c_str(), sizeof(pathBuf));
	bool result = CreateDirectory(pathBuf, NULL);	
#else
	bool result = (mkdir(pathStr.c_str(), 0755) == 0);
#endif
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_child(Context *context, IntrinsicResult partialResult) {
	String path = context->GetVar("parentPath").ToString();
	String filename = context->GetVar("childName").ToString();
	#if _WIN32 || _WIN64
		String pathSep = "\\";
	#else
		String pathSep = "/";
	#endif
	if (path.EndsWith(pathSep)) return IntrinsicResult(path + filename);
	return IntrinsicResult(path + pathSep + filename);
}

static IntrinsicResult intrinsic_rename(Context *context, IntrinsicResult partialResult) {
	String oldPath = context->GetVar("oldPath").ToString();
	String newPath = context->GetVar("newPath").ToString();
	int err = rename(oldPath.c_str(), newPath.c_str());
	return IntrinsicResult(Value::Truth(err == 0));
}

static IntrinsicResult intrinsic_copy(Context *context, IntrinsicResult partialResult) {
	String oldPath = context->GetVar("oldPath").ToString();
	String newPath = context->GetVar("newPath").ToString();
	
	#if _WIN32 || _WIN64
		bool success = CopyFile(oldPath.c_str(), newPath.c_str(), false);
		int result = success ? 0 : 1;
	#else
		int result = UnixishCopyFile(oldPath.c_str(), newPath.c_str());
	#endif

	return IntrinsicResult(Value::Truth(result == 0));
}

static IntrinsicResult intrinsic_remove(Context *context, IntrinsicResult partialResult) {
	String path = context->GetVar("path").ToString();
	#if _WIN32 || _WIN64
		bool isDir = false;
		struct _stati64 stats;
		if (_stati64(path.c_str(), &stats) == 0) {
			isDir = ((stats.st_mode & _S_IFDIR) != 0);
		}
		bool ok;
		if (isDir) ok = RemoveDirectory(path.c_str());
		else ok = DeleteFile(path.c_str());
		int err = ok ? 0 : 1;
	#else
		int err = remove(path.c_str());
	#endif
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
	instance.SetValue(Value::magicIsA, FileHandleClass());
	
	Value fileWrapper = Value::NewHandle(new FileHandleStorage(handle));
	instance.SetValue(_handle, fileWrapper);
	
	Value result(instance);
	instance.SetValue(result, fileWrapper);
	
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_fclose(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == NULL) return IntrinsicResult(Value::zero);
	fclose(handle);
	storage->f = NULL;
	return IntrinsicResult(Value::one);
}

static IntrinsicResult intrinsic_isOpen(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	return IntrinsicResult(Value::Truth(storage->f != NULL));
}

static IntrinsicResult intrinsic_fwrite(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String data = context->GetVar("data").ToString();

	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == NULL) return IntrinsicResult(Value::zero);

	size_t written = fwrite(data.c_str(), 1, data.sizeB(), handle);
	return IntrinsicResult((int)written);
}

static IntrinsicResult intrinsic_fwriteLine(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String data = context->GetVar("data").ToString();
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == NULL) return IntrinsicResult(Value::zero);
	size_t written = fwrite(data.c_str(), 1, data.sizeB(), handle);
	written += fwrite("\n", 1, 1, handle);
	return IntrinsicResult((int)written);
}

static IntrinsicResult intrinsic_fread(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	long bytesToRead = context->GetVar("byteCount").IntValue();
	if (bytesToRead == 0) return IntrinsicResult(Value::emptyString);

	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == NULL) return IntrinsicResult(Value::zero);
	
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

static IntrinsicResult intrinsic_fposition(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == NULL) return IntrinsicResult::Null;

	return IntrinsicResult(ftell(handle));
}

static IntrinsicResult intrinsic_feof(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == NULL) return IntrinsicResult::Null;

	// Note: feof returns true only after attempting to read PAST the end of the file.
	// Not after the last successful read.  See: https://stackoverflow.com/questions/34888776
	return IntrinsicResult(Value::Truth(feof(handle) != 0));
}

static IntrinsicResult intrinsic_freadLine(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == NULL) return IntrinsicResult::Null;

	char buf[1024];
	char *str = fgets(buf, sizeof(buf), handle);
	if (str == NULL) return IntrinsicResult::Null;
	// Grr... we need to strip the terminating newline.
	// Still probably faster than reading character by character though.
	for (int i=0; i<sizeof(buf); i++) {
		if (buf[i] == '\n') {
			buf[i] = 0;
			break;
		}
	}
	String result(buf);
	
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_readLines(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String path = context->GetVar("path").ToString();
	
	FILE *handle = fopen(path.c_str(), "r");
	if (handle == NULL) return IntrinsicResult::Null;

	// Read in 1K chunks, dividing into lines.
	ValueList list;
	char buf[1024];
	String partialLine;
	while (!feof(handle)) {
		size_t bytesRead = fread(buf, 1, sizeof(buf), handle);
		if (bytesRead == 0) break;
		int lineStart = 0;
		for (int i=0; i<bytesRead; i++) {
			if (buf[i] == '\n' || buf[i] == '\r') {
				String line(&buf[lineStart], i - lineStart);
				if (!partialLine.empty()) {
					line = partialLine + line;
					partialLine = "";
				}
				list.Add(line);
				if (buf[i] == '\n' && i+1 < bytesRead && buf[i+1] == '\r') i++;
				if (i+1 < bytesRead && buf[i+1] == 0) i++;
				lineStart = i + 1;
			}
		}
		if (lineStart < bytesRead) {
			partialLine = String(&buf[lineStart], bytesRead - lineStart);
		}
	}
	fclose(handle);
	return IntrinsicResult(list);
}

static IntrinsicResult intrinsic_writeLines(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String path = context->GetVar("path").ToString();
	Value lines = context->GetVar("lines");

	FILE *handle = fopen(path.c_str(), "w");
	if (handle == NULL) return IntrinsicResult::Null;

	size_t written = 0;
	if (lines.type == ValueType::List) {
		ValueList list = lines.GetList();
		for (int i=0; i<list.Count(); i++) {
			String data = list[i].ToString();
			written += fwrite(data.c_str(), 1, data.sizeB(), handle);
			written += fwrite("\n", 1, 1, handle);
		}
	} else {
		// Anything other than a list, just convert to a string and write it out.
		String data = lines.ToString();
		written = fwrite(data.c_str(), 1, data.sizeB(), handle);
		written += fwrite("\n", 1, 1, handle);
	}
	
	fclose(handle);
	return IntrinsicResult((int)written);
}

static bool disallowAssignment(ValueDict& dict, Value key, Value value) {
	return true;
}

static IntrinsicResult intrinsic_File(Context *context, IntrinsicResult partialResult) {
	static ValueDict fileModule;
	
	if (fileModule.Count() == 0) {
		fileModule.SetValue("curdir", i_getcwd->GetFunc());
		fileModule.SetValue("setdir", i_chdir->GetFunc());
		fileModule.SetValue("children", i_readdir->GetFunc());
		fileModule.SetValue("name", i_basename->GetFunc());
		fileModule.SetValue("exists", i_exists->GetFunc());
		fileModule.SetValue("info", i_info->GetFunc());
		fileModule.SetValue("makedir", i_mkdir->GetFunc());
		fileModule.SetValue("parent", i_dirname->GetFunc());
		fileModule.SetValue("child", i_child->GetFunc());
		fileModule.SetValue("move", i_rename->GetFunc());
		fileModule.SetValue("copy", i_copy->GetFunc());
		fileModule.SetValue("delete", i_remove->GetFunc());
		fileModule.SetValue("open", i_fopen->GetFunc());
		fileModule.SetValue("readLines", i_readLines->GetFunc());
		fileModule.SetValue("writeLines", i_writeLines->GetFunc());
		fileModule.SetAssignOverride(disallowAssignment);
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
		result.SetValue("position", i_fposition->GetFunc());
		result.SetValue("atEnd", i_feof->GetFunc());
	}
	
	return result;
}

static IntrinsicResult intrinsic_env(Context *context, IntrinsicResult partialResult) {
	static ValueDict envMap;
	if (envMap.Count() == 0) {
		// The stdlib-supplied `environ` is a null-terminated array of char* (C strings).
		// Each such C string is of the form NAME=VALUE.  So we need to split on the
		// first '=' to separate this into keys and values for our env map.
		for (char **current = environ; *current; current++) {
			char* eqPos = strchr(*current, '=');
			if (!eqPos) continue;	// (should never happen, but just in case)
			String varName(*current, eqPos - *current);
			String valueStr(eqPos+1);
			envMap.SetValue(varName, valueStr);
		}
	}
	return IntrinsicResult(envMap);
}

static bool assignEnvVar(ValueDict& dict, Value key, Value value) {
	#if WINDOWS
		_putenv_s(key.ToString().c_str(), value.ToString().c_str());
	#else
		setenv(key.ToString().c_str(), value.ToString().c_str(), 1);
	#endif
	return false;	// allow standard assignment to also apply.
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
	
	f = Intrinsic::Create("env");
	f->code = &intrinsic_env;
	
	f = Intrinsic::Create("input");
	f->AddParam("prompt", "");
	f->code = &intrinsic_input;

	f = Intrinsic::Create("file");
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
	
	i_exists = Intrinsic::Create("");
	i_exists->AddParam("path");
	i_exists->code = &intrinsic_exists;
	
	i_info = Intrinsic::Create("");
	i_info->AddParam("path");
	i_info->code = &intrinsic_info;
	
	i_mkdir = Intrinsic::Create("");
	i_mkdir->AddParam("path");
	i_mkdir->code = &intrinsic_mkdir;

	i_rename = Intrinsic::Create("");
	i_rename->AddParam("oldPath");
	i_rename->AddParam("newPath");
	i_rename->code = &intrinsic_rename;
	
	i_copy = Intrinsic::Create("");
	i_copy->AddParam("oldPath");
	i_copy->AddParam("newPath");
	i_copy->code = &intrinsic_copy;
	
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
	
	i_fposition = Intrinsic::Create("");
	i_fposition->code = &intrinsic_fposition;
	
	i_readLines = Intrinsic::Create("");
	i_readLines->AddParam("path");
	i_readLines->code = &intrinsic_readLines;
	
	i_writeLines = Intrinsic::Create("");
	i_writeLines->AddParam("path");
	i_writeLines->AddParam("lines");
	i_writeLines->code = &intrinsic_writeLines;
}
