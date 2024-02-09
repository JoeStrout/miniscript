//
//  ShellIntrinsics.cpp
//  MiniScript
//
//  Created by Joe Strout on 3/9/19.
//  Copyright © 2021 Joe Strout. All rights reserved.
//

#include "ShellIntrinsics.h"
#include <iostream>
#include <fstream>
#include "MiniScript/SimpleString.h"
#include "MiniScript/UnicodeUtil.h"
#include "MiniScript/UnitTest.h"
#include "MiniScript/SimpleVector.h"
#include "MiniScript/List.h"
#include "MiniScript/Dictionary.h"
#include "MiniScript/MiniscriptParser.h"
#include "MiniScript/MiniscriptInterpreter.h"
#include "OstreamSupport.h"
#include "MiniScript/SplitJoin.h"
#include "whereami/whereami.h"
#include "DateTimeUtils.h"

#include <cstdlib>
#include <sstream>
#include <iostream>
#include <memory>
#include <stdexcept>
//#include <string>
#include <array>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if _WIN32 || _WIN64
	#define WINDOWS 1
	#include <windows.h>
	#include <Shlwapi.h>
	#include <Fileapi.h>
	#include <direct.h>
	#include <locale>
	#include <codecvt>
	#define getcwd _getcwd
	#define setenv _setenv
	#define PATHSEP '\\'
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
	#define PATHSEP '/'
	#include <unistd.h>
	#include <sys/wait.h>
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
static Value _MS_IMPORT_PATH("MS_IMPORT_PATH");

static ValueDict getEnvMap();

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

// Copy a file.  Return 0 on success, or some value < 0 on error.
static int UnixishCopyFile(const char* source, const char* destination) {
#if WINDOWS
	bool success = CopyFile(source, destination, false);
	return success ? 0 : -1;
#elif defined(__APPLE__) || defined(__FreeBSD__)
	// fcopyfile works on FreeBSD and OS X 10.5+
	int input, output;
	if ((input = open(source, O_RDONLY)) == -1)	return -1;
	if ((output = creat(destination, 0660)) == -1) {
		close(input);
		return -1;
	}
	int result = fcopyfile(input, output, 0, COPYFILE_ALL);
	close(input);
	close(output);
	return result;
#else
	// on Linux, the best way to copy a file with metadata is to let the shell do it:
	String command = String("cp -p \"") + source + "\" \"" + destination + "\"";
	return system(command.c_str());
#endif
}

// Expand any occurrences of $VAR, $(VAR) or ${VAR} on all platforms,
// and also of %VAR% under Windows only, using variables from GetEnvMap().
static String ExpandVariables(String path) {
	long p0, p1;
	long len = path.LengthB();
	ValueDict envMap = getEnvMap();
	while (true) {
		p0 = path.IndexOfB("${");
		if (p0 >= 0) {
			for (p1=p0+1; p1<len && path[p1] != '}'; p1++) {}
			if (p1 < len) {
				String varName = path.SubstringB(p0 + 2, p1 - p0 - 2);
				path = path.Substring(0, p0) + envMap.Lookup(varName, Value::emptyString).ToString() + path.SubstringB(p1 + 1);
				continue;
			}
		}
		p0 = path.IndexOfB("$(");
		if (p0 >= 0) {
			for (p1=p0+1; p1<len && path[p1] != ')'; p1++) {}
			if (p1 < len) {
				String varName = path.SubstringB(p0 + 2, p1 - p0 - 2);
				path = path.Substring(0, p0) + envMap.Lookup(varName, Value::emptyString).ToString() + path.SubstringB(p1 + 1);
				continue;
			}
		}
#if WINDOWS
		p0 = path.IndexOfB("%");
		if (p0 >= 0) {
			for (p1=p0+1; p1<len && path[p1] != '%'; p1++) {}
			if (p1 < len) {
				String varName = path.SubstringB(p0 + 1, p1 - p0 - 1);
				path = path.Substring(0, p0) + envMap.Lookup(varName, Value::emptyString).ToString() + path.SubstringB(p1 + 1);
				continue;
			}
		}
#endif		
		p0 = path.IndexOfB("$");
		if (p0 >= 0) {
			// variable continues until non-alphanumeric char
			p1 = p0+1;
			while (p1 < len) {
				char c = path[p1];
				if (c < '0' || (c > '9' && c < 'A') || (c > 'Z' && c < '_') || c == '`' || c > 'z') break;
				p1++;
			}
			String varName = path.SubstringB(p0 + 1, p1 - p0 - 1);
			path = path.Substring(0, p0) + envMap.Lookup(varName, Value::emptyString).ToString() + path.SubstringB(p1);
			continue;
		}
		break;
	}
	return path;
}

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
			closedir(dir);
		}
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

static String dirname(String pathStr) {
#if _WIN32 || _WIN64
	char pathBuf[512];
	_fullpath(pathBuf, pathStr.c_str(), sizeof(pathBuf));
	char driveBuf[3];
	char dirBuf[256];
	_splitpath_s(pathBuf, driveBuf, sizeof(driveBuf), dirBuf, sizeof(dirBuf), NULL, 0, NULL, 0);
	String result = String(driveBuf) + String(dirBuf);
#elif defined(__APPLE__) || defined(__FreeBSD__)
	String result(dirname((char*)pathStr.c_str()));
#else
	// Linux dirname modifies the argument passed in, so:
	char *duplicate = strdup((char*)pathStr.c_str());
	String result(dirname(duplicate));
	free(duplicate);
#endif
	return result;
}

static IntrinsicResult intrinsic_dirname(Context *context, IntrinsicResult partialResult) {
	Value path = context->GetVar("path");
	if (path.IsNull()) return IntrinsicResult(Value::zero);
	String pathStr = path.ToString();
	if (pathStr.LengthB() > 0 && pathStr[pathStr.LengthB()-1] == PATHSEP) {
		// For consistency across platforms: always strip a trailing path separator
		// (MacOS dirname ignores this, but Windows does not).
		pathStr = pathStr.SubstringB(0, pathStr.LengthB() - 1);
	}
	return IntrinsicResult(dirname(pathStr));
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

static double dateTimeEpoch() {
	static double _result = 0;
	if (_result == 0) {
		tm baseDate;
		memset(&baseDate, 0, sizeof(tm));
		baseDate.tm_year = 2000 - 1900;	// (because tm_year is years since 1900!)
		baseDate.tm_mon = 0;
		baseDate.tm_mday = 1;
		_result = mktime(&baseDate);
	}
	return _result;
}

static IntrinsicResult intrinsic_dateStr(Context *context, IntrinsicResult partialResult) {
	Value date = context->GetVar("date");
	Value format = context->GetVar("format");
	String formatStr;
	if (format.IsNull()) formatStr = "yyyy-MM-dd HH:mm:ss";
	else formatStr = format.ToString();
	double d;
	if (date.IsNull()) {
		time_t t;
		time(&t);
		d = t;
	} else if (date.type == ValueType::Number) {
		d = date.DoubleValue() + dateTimeEpoch();
	} else {
		d = ParseDate(date.ToString());
	}
	return IntrinsicResult(FormatDate((time_t)d, formatStr));
}

static IntrinsicResult intrinsic_dateVal(Context *context, IntrinsicResult partialResult) {
	Value date = context->GetVar("dateStr");
	time_t t;
	if (date.IsNull()) {
		time(&t);
	} else if (date.type == ValueType::Number) {
		return IntrinsicResult(date);
	} else {
		t = ParseDate(date.ToString());
	}
	return IntrinsicResult((double)t - dateTimeEpoch());
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
	int result = UnixishCopyFile(oldPath.c_str(), newPath.c_str());
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
	if (modeVal.IsNull() || mode.empty() || mode == "rw+" || mode == "r+") {
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

static String ReadFileHelper(FILE *handle, long bytesToRead) {
	// If bytesToRead < 0, read to EOF.
	// Otherwise, read bytesToRead bytes (1k at a time).
	char buf[1024];
	String result;
	while (!feof(handle) && (bytesToRead != 0)) {
		size_t read = fread(buf, 1, bytesToRead > 0 && bytesToRead < 1024 ? bytesToRead : 1024, handle);
		if (bytesToRead > 0) bytesToRead -= read;
		result += String(buf, read);
	}
	return result;
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
	
	String result = ReadFileHelper(handle, bytesToRead);
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

#if WINDOWS
// timeout : The time to wait in milliseconds before killing the child process.
bool CreateChildProcess(const std::string& cmd, std::string& out, std::string& err, DWORD& returnCode, DWORD timeout) {
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	HANDLE hChildStd_OUT_Rd = NULL;
	HANDLE hChildStd_OUT_Wr = NULL;
	HANDLE hChildStd_ERR_Rd = NULL;
	HANDLE hChildStd_ERR_Wr = NULL;

	// Create a pipe for the child process's STDOUT.
	if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0))
		return false;

	// Ensure the read handle to the pipe for STDOUT is not inherited.
	SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);

	// Create a pipe for the child process's STDERR.
	if (!CreatePipe(&hChildStd_ERR_Rd, &hChildStd_ERR_Wr, &saAttr, 0))
		return false;

	// Ensure the read handle to the pipe for STDERR is not inherited.
	SetHandleInformation(hChildStd_ERR_Rd, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO siStartInfo;
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = hChildStd_ERR_Wr;
	siStartInfo.hStdOutput = hChildStd_OUT_Wr;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION piProcInfo;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// Start the child process.
	if (!CreateProcessA(NULL,
		// (LPWSTR)cmd.c_str(), // command line
		(LPSTR)cmd.c_str(), // command line
		NULL,               // process security attributes
		NULL,               // primary thread security attributes
		TRUE,               // handles are inherited
		0,                  // creation flags
		NULL,               // use parent's environment
		NULL,               // use parent's current directory
		&siStartInfo,       // STARTUPINFO pointer
		&piProcInfo))       // receives PROCESS_INFORMATION
	{
		return false;
	}

	// Close handles to the stdin and stdout pipes no longer needed by the child process.
	// If they are not explicitly closed, there is no way to recognize that the child process has completed.
	CloseHandle(hChildStd_OUT_Wr);
	CloseHandle(hChildStd_ERR_Wr);

	// Read output from the child process's pipe for STDOUT
	// and print to the parent process's STDOUT.
	DWORD dwRead;
	CHAR chBuf[4096];
	bool bSuccess = FALSE;

	for (;;) {
		bSuccess = ReadFile(hChildStd_OUT_Rd, chBuf, 4096, &dwRead, NULL);
		if (!bSuccess || dwRead == 0) break;

		std::string outputStr(chBuf, chBuf + dwRead / sizeof(wchar_t));
		out += outputStr;
	}

	// Read from STDERR
	for (;;) {
		bSuccess = ReadFile(hChildStd_ERR_Rd, chBuf, 4096, &dwRead, NULL);
		if (!bSuccess || dwRead == 0) break;

		std::string errorStr(chBuf, chBuf + dwRead / sizeof(wchar_t));
		err += errorStr;
	}

	// Wait until child process exits or timeout
	DWORD waitResult = WaitForSingleObject(piProcInfo.hProcess, timeout);
	if (waitResult == WAIT_TIMEOUT) {
		// If the process is still running after the timeout, terminate it
		TerminateProcess(piProcInfo.hProcess, 1); // Use 1 or another number to indicate forced termination
		
		err += "Timed out";
		returnCode = 124 << 8;	// (124 is status code used by `timeout` command)
	}

	// Regardless of the outcome, try to get the exit code
	if (!GetExitCodeProcess(piProcInfo.hProcess, &returnCode)) {
		returnCode = (DWORD)-1; // Use -1 or another value to indicate that getting the exit code failed
	}

	// Close handles to the child process and its primary thread.
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);

	// Close the remaining pipe handles.
	CloseHandle(hChildStd_OUT_Rd);
	CloseHandle(hChildStd_ERR_Rd);

	return true;
}

static IntrinsicResult intrinsic_exec(Context* context, IntrinsicResult partialResult) {
	//std::string cmd = "cmd /k dir c:\\";
	std::string cmd = context->GetVar("cmd").ToString().c_str();
	std::string out;
	std::string err;
	DWORD returnCode;

	double timeoutSecs = context->GetVar("timeout").DoubleValue();
	double timeoutMs = (timeoutSecs == 0) ? INFINITE : (timeoutSecs * 1000);

	if (!CreateChildProcess(cmd, out, err, returnCode, timeoutMs)) {
		Error("Failed to create child process.");
	}

	// Build our result map.
	ValueDict result;
	result.SetValue("output", Value(out.c_str()));
	result.SetValue("errors", Value(err.c_str()));
	result.SetValue("status", Value(returnCode));
	return IntrinsicResult(result);
}
#else
// Helper function to read from file descriptor into string
String readFromFd(int fd, bool trimTrailingNewline=true) {
    String output;
    const int bufferSize = 1024;
    char buffer[bufferSize];
    ssize_t bytesRead;

	bool trimmed = false;
    while ((bytesRead = read(fd, buffer, bufferSize - 1)) > 0) {
        buffer[bytesRead] = '\0';
		if (trimTrailingNewline and bytesRead < bufferSize-1 and bytesRead > 0 and buffer[bytesRead-1] == '\n') {
			// Efficiently trim \n or \r\n from the end of the buffer
			buffer[bytesRead-1] = '\0';
			if (bytesRead > 1 and buffer[bytesRead-2] == '\r') {
				buffer[bytesRead-2] = '\0';
			}
			trimmed = true;
		}
        output += buffer;
    }

	if (trimTrailingNewline && !trimmed) {
		// Not-so-efficiently trim our final string, in the case where our data happened
		// to exactly align with the buffer size, so we couldn't know we were at the
		// end of it to trim it above.  (This is a rare edge case.)
		int cut = 0;
		if (output.LengthB() > 1 and output[-1] == '\n') {
			cut = 1;
			if (output.LengthB() > 2 and output[-2] == '\r') cut = 2;
		}
		if (cut) output = output.SubstringB(0, output.LengthB() - cut);
	}
	
    return output;
}

static IntrinsicResult intrinsic_exec(Context *context, IntrinsicResult partialResult) {
	double now = context->vm->RunTime();
	if (partialResult.Done()) {
		// This is the initial entry into `exec`.  Fork a subprocess to execute the
		// given command, and return a partial result we can use to check on its progress.
		String cmd = context->GetVar("cmd").ToString();
		double timeout = context->GetVar("timeout").DoubleValue();

		// Create a pipe each for stdout and stderr.
		// File descriptor 0 of each is the read end; element 1 is the write end.
		int stdoutPipe[2];
		int stderrPipe[2];
		pipe(stdoutPipe);
		pipe(stderrPipe);
		
		pid_t pid = fork(); // Fork the process
		
		if (pid == -1) {
			Error("Failed to fork the child process.");
		} else if (pid == 0) {
			// Child process.
			
			// Redirect stdout and stderr to our pipes, and then close the read ends.
			dup2(stdoutPipe[1], STDOUT_FILENO);
			dup2(stderrPipe[1], STDERR_FILENO);
			close(stdoutPipe[0]);
			close(stderrPipe[0]);
			
			// Call the host environment's command processor.  Or if the command
			// is empty, then return a nonzero value iff the command processor exists.
			const char* cmdPtr = cmd.empty() ? NULL : cmd.c_str();
			int cmdResult = std::system(cmdPtr);
			cmdResult = WEXITSTATUS(cmdResult);
			
			// All done!  Exit the child process and return the result.
			exit(cmdResult);
		}
		// Parent process.
		
		// Close the write end of the pipes.
		close(stdoutPipe[1]);
		close(stderrPipe[1]);
		
		// As our partial result, return a list with the pid, the two read pipes, and the final time.
		ValueList data;
		data.Add(Value(pid));
		data.Add(Value(stdoutPipe[0]));
		data.Add(Value(stderrPipe[0]));
		data.Add(Value(now + timeout));
		return IntrinsicResult(data, false);
	}
	
	// This is a subsequent entry to intrinsic_exec, where we've already forked
	// the subprocess, and now we're waiting for it to finish.
	// Start by getting the pid, the two read pipes, and the final time out of the partial result.
	ValueList data = partialResult.Result().GetList();
	int pid = data[0].IntValue();
	int stdoutPipe = data[1].IntValue();
	int stderrPipe = data[2].IntValue();
	double finalTime = data[3].DoubleValue();
	
	// Then, see if the child process has finished.
	int returnCode;
	String stdoutContent, stderrContent;
	int waitResult = waitpid(pid, &returnCode, WUNTRACED | WNOHANG);
	//std::cout << "waitpid returned " << waitResult << ", returnCode is " << returnCode << std::endl;
	if (waitResult <= 0) {
		// Child process not finished yet.
		if (now < finalTime) {
			// Not timed out, either — keep waiting.
			return IntrinsicResult(data, false);
		}

		// We've waited too long.  Time out.
		stderrContent = "Timed out";
		returnCode = 124 << 8;	// (124 is status code used by `timeout` command)
	} else {
		// Child process completed successfully.  Huzzah!
		// Read output from pipes.
		stdoutContent = readFromFd(stdoutPipe);
		stderrContent = readFromFd(stderrPipe);
	}
	// Close our pipes.
	close(stdoutPipe);
	close(stderrPipe);

	// Build our result map.
	ValueDict result;
	result.SetValue("output", Value(stdoutContent.c_str()));
	result.SetValue("errors", Value(stderrContent.c_str()));
	result.SetValue("status", Value(WEXITSTATUS(returnCode)));
	return IntrinsicResult(result);
}
#endif

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

static void setEnvVar(const char* key, const char* value) {
	#if WINDOWS
		_putenv_s(key, value);
	#else
		setenv(key, value, 1);
	#endif
}

static bool assignEnvVar(ValueDict& dict, Value key, Value value) {
	setEnvVar(key.ToString().c_str(), value.ToString().c_str());
	return false;	// allow standard assignment to also apply.
}

static ValueDict getEnvMap() {
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
		if (envMap.Lookup(_MS_IMPORT_PATH, Value::null).IsNull()) {
			envMap.SetValue(_MS_IMPORT_PATH, "$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib");
		}
		envMap.SetAssignOverride(assignEnvVar);
	}
	return envMap;
}

static IntrinsicResult intrinsic_import(Context *context, IntrinsicResult partialResult) {
	if (!partialResult.Result().IsNull()) {
		// When we're invoked with a partial result, it means that the import
		// function has finished, and stored its result (the values that were
		// created by the import code) in Temp 0.
		Value importedValues = context->GetTemp(0);
		// Now we're going to do something slightly evil.  We're going to reach
		// up into the *parent* context, and store these imported values under
		// the import library name.  Thus, there will always be a standard name
		// by which you can refer to the imported stuff.
		String libname = partialResult.Result().ToString();
		Context *callerContext = context->parent;
		callerContext->SetVar(libname, importedValues);
		return IntrinsicResult::Null;
	}
	// When we're invoked without a partial result, it's time to start the import.
	// Begin by finding the actual code.
	String libname = context->GetVar("libname").ToString();
	if (libname.empty()) {
		RuntimeException("import: libname required").raise();
	}
	
	// Figure out what directories to look for the import modules in.
	Value searchPath = getEnvMap().Lookup(_MS_IMPORT_PATH, Value::null);
	StringList libDirs;
	if (!searchPath.IsNull()) libDirs = Split(searchPath.ToString(), ":");
	
	// Search the lib dirs for a matching file.
	String moduleSource;
	bool found = false;
	for (long i=0, len=libDirs.Count(); i<len; i++) {
		String path = libDirs[i];
		if (path.empty()) path = ".";
		else if (path[path.LengthB() - 1] != PATHSEP) path += String(PATHSEP);
		path += libname + ".ms";
		path = ExpandVariables(path);
		FILE *handle = fopen(path.c_str(), "r");
		if (handle == NULL) continue;
		moduleSource = ReadFileHelper(handle, -1);
		fclose(handle);
		found = true;
		break;
	}
	if (!found) {
		RuntimeException("import: library not found: " + libname).raise();
	}
	
	// Now, parse that code, and build a function around it that returns
	// its own locals as its result.  Push a manual call.
	Parser parser;
	parser.errorContext = libname + ".ms";
	parser.Parse(moduleSource);
	FunctionStorage *import = parser.CreateImport();
	context->vm->ManuallyPushCall(import, Value::Temp(0));
	
	// That call will not be able to run until we return from this intrinsic.
	// So, return a partial result, with the lib name.  We'll get invoked
	// again after the import function has finished running.
	return IntrinsicResult(libname, false);
}

static IntrinsicResult intrinsic_env(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(getEnvMap());
}

void AddScriptPathVar(const char* scriptPartialPath) {
	String scriptDir;
	if (!scriptPartialPath || scriptPartialPath[0] == 0) {
		#if WINDOWS
			char s[512];
			_fullpath(s, ".", sizeof(s));
			scriptDir = s;
		#else
			char* s = realpath(".", NULL);
			scriptDir = s;
			free(s);
		#endif
	} else {
		#if WINDOWS
			char s[512];
			_fullpath(s, scriptPartialPath, sizeof(s));
			String scriptFullPath = s;
		#else
			char* s = realpath(scriptPartialPath, NULL);
			String scriptFullPath(s);
			free(s);
		#endif
		scriptDir = dirname(scriptFullPath);
	}
	setEnvVar("MS_SCRIPT_DIR", scriptDir.c_str());
}

void AddPathEnvVars() {
	int length = wai_getExecutablePath(NULL, 0, NULL);
	char* path = (char*)malloc(length + 1);
	int dirname_length;
	wai_getExecutablePath(path, length, &dirname_length);
	path[dirname_length] = '\0';
	setEnvVar("MS_EXE_DIR", path);
}

/// Add intrinsics that are not part of core MiniScript, but which make sense
/// in this command-line environment.
void AddShellIntrinsics() {
	AddPathEnvVars();
	
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

	f = Intrinsic::Create("import");
	f->AddParam("libname", "");
	f->code = &intrinsic_import;

	f = Intrinsic::Create("file");
	f->code = &intrinsic_File;

	f = Intrinsic::Create("_dateVal");
	f->AddParam("dateStr");
	f->code = &intrinsic_dateVal;

	f = Intrinsic::Create("_dateStr");
	f->AddParam("date");
	f->AddParam("format", "yyyy-MM-dd HH:mm:ss");
	f->code = &intrinsic_dateStr;

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
	i_fopen->AddParam("mode", "r+");
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
	
	f = Intrinsic::Create("exec");
	f->AddParam("cmd");
	f->AddParam("timeout", 30);
	f->code = &intrinsic_exec;
}
