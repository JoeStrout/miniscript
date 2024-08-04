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
#include "ShellExec.h"
#include "Key.h"

#include <cstdlib>
#include <sstream>
#include <iostream>
#include <memory>
#include <stdexcept>
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

// RefCountedStorage class to wrap raw data
class RawDataHandleStorage : public RefCountedStorage {
public:
	RawDataHandleStorage() : data(nullptr), dataSize(0) {}
	RawDataHandleStorage(FILE *f) {
		fseek(f, 0, SEEK_END);
		dataSize = ftell(f);
		data = malloc(dataSize);
		if (data) {
			fseek(f, 0, SEEK_SET);
			fread(data, 1, dataSize, f);
		}
	}
	RawDataHandleStorage(const char *buf, long nBytes) {
		data = nullptr;
		resize(nBytes);
		if (nBytes > 0) {
			memcpy(data, buf, nBytes);
		}
	}
	virtual ~RawDataHandleStorage() { free(data); }
	void resize(size_t newSize) {
		if (newSize == 0) {
			free(data);
			data = nullptr;
			dataSize = 0;
		} else {
			void *newData = realloc(data, newSize);
			if (newData) {
				data = newData;
				dataSize = newSize;
			}
		}
	}
	void copyFromOther(RawDataHandleStorage& other, long offset, long maxBytes, long otherOffset, long otherBytes) {
		if (offset >= dataSize || otherOffset >= other.dataSize) return;
		if (maxBytes < 0 || offset + maxBytes > dataSize) maxBytes = dataSize - offset;
		if (maxBytes < 1) return;
		if (otherBytes < 0 || otherOffset + otherBytes > other.dataSize) otherBytes = other.dataSize - otherOffset;
		if (otherBytes < 1) return;
		if (otherBytes > maxBytes) otherBytes = maxBytes;
		char *dst = (char *)data + offset;
		memcpy(dst, other.data, otherBytes);
	}

	void *data;
	size_t dataSize;
};

// hidden (unnamed) intrinsics, only accessible via other methods (such as the File module)
Intrinsic *i_getcwd = nullptr;
Intrinsic *i_chdir = nullptr;
Intrinsic *i_readdir = nullptr;
Intrinsic *i_basename = nullptr;
Intrinsic *i_dirname = nullptr;
Intrinsic *i_child = nullptr;
Intrinsic *i_exists = nullptr;
Intrinsic *i_info = nullptr;
Intrinsic *i_mkdir = nullptr;
Intrinsic *i_copy = nullptr;
Intrinsic *i_readLines = nullptr;
Intrinsic *i_writeLines = nullptr;
Intrinsic *i_loadRaw = nullptr;
Intrinsic *i_saveRaw = nullptr;
Intrinsic *i_rename = nullptr;
Intrinsic *i_remove = nullptr;
Intrinsic *i_fopen = nullptr;
Intrinsic *i_fclose = nullptr;
Intrinsic *i_isOpen = nullptr;
Intrinsic *i_fwrite = nullptr;
Intrinsic *i_fwriteLine = nullptr;
Intrinsic *i_fread = nullptr;
Intrinsic *i_freadLine = nullptr;
Intrinsic *i_fposition = nullptr;
Intrinsic *i_feof = nullptr;

Intrinsic *i_rawDataLen = nullptr;
Intrinsic *i_rawDataResize = nullptr;
Intrinsic *i_rawDataByte = nullptr;
Intrinsic *i_rawDataSetByte = nullptr;
Intrinsic *i_rawDataSbyte = nullptr;
Intrinsic *i_rawDataSetSbyte = nullptr;
Intrinsic *i_rawDataUshort = nullptr;
Intrinsic *i_rawDataSetUshort = nullptr;
Intrinsic *i_rawDataShort = nullptr;
Intrinsic *i_rawDataSetShort = nullptr;
Intrinsic *i_rawDataUint = nullptr;
Intrinsic *i_rawDataSetUint = nullptr;
Intrinsic *i_rawDataInt = nullptr;
Intrinsic *i_rawDataSetInt = nullptr;
Intrinsic *i_rawDataFloat = nullptr;
Intrinsic *i_rawDataSetFloat = nullptr;
Intrinsic *i_rawDataDouble = nullptr;
Intrinsic *i_rawDataSetDouble = nullptr;
Intrinsic *i_rawDataUtf8 = nullptr;
Intrinsic *i_rawDataSetUtf8 = nullptr;
Intrinsic *i_rawDataSub = nullptr;
Intrinsic *i_rawDataSetSub = nullptr;
Intrinsic *i_rawDataConcat = nullptr;

Intrinsic *i_keyAvailable = nullptr;
Intrinsic *i_keyGet = nullptr;
Intrinsic *i_keyPut = nullptr;
Intrinsic *i_keyClear = nullptr;
Intrinsic *i_keyPressed = nullptr;
Intrinsic *i_keyKeyNames = nullptr;
Intrinsic *i_keyAxis = nullptr;
Intrinsic *i_keyPutInFront = nullptr;
Intrinsic *i_keyEcho = nullptr;

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
static ValueDict& RawDataType();
static ValueDict& KeyModule();

static IntrinsicResult intrinsic_input(Context *context, IntrinsicResult partialResult) {
	Value prompt = context->GetVar("prompt");
	
	#if useEditline
		char *buf;
		buf = readline(prompt.ToString().c_str());
		if (buf == nullptr) return IntrinsicResult(Value::emptyString);
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
		if (dir != nullptr) {
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
		_splitpath_s(pathStr.c_str(), driveBuf, sizeof(driveBuf), nullptr, 0, nameBuf, sizeof(nameBuf), extBuf, sizeof(extBuf));
		String result = String(nameBuf) + String(extBuf);
	#elif defined(__APPLE__) || defined(__FreeBSD__)
		String result(basename((char*)pathStr.c_str()));
	#else
		char *duplicate = strdup((char*)pathStr.c_str());
		String result(basename(duplicate));
		free(duplicate);
	#endif
	return IntrinsicResult(result);
}

static String dirname(String pathStr) {
#if _WIN32 || _WIN64
	char pathBuf[512];
	_fullpath(pathBuf, pathStr.c_str(), sizeof(pathBuf));
	char driveBuf[3];
	char dirBuf[256];
	_splitpath_s(pathBuf, driveBuf, sizeof(driveBuf), dirBuf, sizeof(dirBuf), nullptr, 0, nullptr, 0);
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
	bool result = CreateDirectory(pathBuf, nullptr);	
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
		if (handle == nullptr) handle = fopen(path.c_str(), "w+");
	} else {
		handle = fopen(path.c_str(), mode.c_str());
	}
	if (handle == nullptr) return IntrinsicResult::Null;
	
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
	if (handle == nullptr) return IntrinsicResult(Value::zero);
	fclose(handle);
	storage->f = nullptr;
	return IntrinsicResult(Value::one);
}

static IntrinsicResult intrinsic_isOpen(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	return IntrinsicResult(Value::Truth(storage->f != nullptr));
}

static IntrinsicResult intrinsic_fwrite(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String data = context->GetVar("data").ToString();

	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == nullptr) return IntrinsicResult(Value::zero);

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
	if (handle == nullptr) return IntrinsicResult(Value::zero);
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
	if (handle == nullptr) return IntrinsicResult(Value::zero);
	
	String result = ReadFileHelper(handle, bytesToRead);
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_fposition(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == nullptr) return IntrinsicResult::Null;

	return IntrinsicResult(ftell(handle));
}

static IntrinsicResult intrinsic_feof(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	
	Value fileWrapper = self.Lookup(_handle);
	if (fileWrapper.IsNull() or fileWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	FileHandleStorage *storage = (FileHandleStorage*)fileWrapper.data.ref;
	FILE *handle = storage->f;
	if (handle == nullptr) return IntrinsicResult::Null;

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
	if (handle == nullptr) return IntrinsicResult::Null;

	char buf[1024];
	char *str = fgets(buf, sizeof(buf), handle);
	if (str == nullptr) return IntrinsicResult::Null;
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
	if (handle == nullptr) return IntrinsicResult::Null;

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
				if (buf[i] == '\r' && i+1 < bytesRead && buf[i+1] == '\n') i++;
				if (i+1 < bytesRead && buf[i+1] == 0) i++;
				lineStart = i + 1;
			}
		}
		if (lineStart < bytesRead) {
			partialLine = String(&buf[lineStart], bytesRead - lineStart);
		}
	}
	if (!partialLine.empty()) list.Add(partialLine);
	fclose(handle);
	return IntrinsicResult(list);
}

static IntrinsicResult intrinsic_writeLines(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	String path = context->GetVar("path").ToString();
	Value lines = context->GetVar("lines");

	FILE *handle = fopen(path.c_str(), "w");
	if (handle == nullptr) return IntrinsicResult::Null;

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

static IntrinsicResult intrinsic_loadRaw(Context *context, IntrinsicResult partialResult) {
	String path = context->GetVar("path").ToString();
	FILE *f = fopen(path.c_str(), "rb");
	if (f == nullptr) return IntrinsicResult::Null;
	Value dataWrapper = Value::NewHandle(new RawDataHandleStorage(f));
	ValueDict instance;
	instance.SetValue(Value::magicIsA, RawDataType());
	instance.SetValue(_handle, dataWrapper);
	Value result(instance);
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_saveRaw(Context *context, IntrinsicResult partialResult) {
	String path = context->GetVar("path").ToString();
	Value rawData = context->GetVar("rawData");
	if (!rawData.IsA(RawDataType(), context->vm)) {
		Value errMsg("Error: RawData parameter is required");
		return IntrinsicResult(errMsg);
	}
	Value dataWrapper = rawData.Lookup(_handle);
	if (dataWrapper.IsNull() or dataWrapper.type != ValueType::Handle) {
		Value errMsg("Error: RawData parameter is required");
		return IntrinsicResult(errMsg);
	}
	RawDataHandleStorage *storage = (RawDataHandleStorage*)dataWrapper.data.ref;
	if (storage->dataSize == 0) {
		Value errMsg("Error: RawData parameter is required");
		return IntrinsicResult(errMsg);
	}
	FILE *f = fopen(path.c_str(), "wb");
	if (f == nullptr) return IntrinsicResult::Null;
	size_t written = fwrite(storage->data, 1, storage->dataSize, f);
	fclose(f);
	if (written < storage->dataSize) {
		String s("Error: expected to write ");
		s += String::Format((long)storage->dataSize);
		s += " bytes, written ";
		s += String::Format((long)written);
		Value errMsg(s);
		return IntrinsicResult(errMsg);
	}
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_rawDataLen(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	Value dataWrapper = self.Lookup(_handle);
	if (dataWrapper.IsNull() or dataWrapper.type != ValueType::Handle) return IntrinsicResult(Value((double)0));
	RawDataHandleStorage *storage = (RawDataHandleStorage*)dataWrapper.data.ref;
	return IntrinsicResult(storage->dataSize);
}

static IntrinsicResult intrinsic_rawDataResize(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	long nBytes = context->GetVar("bytes").IntValue();
	if (nBytes < 0) {
		IndexException(String("bytes parameter must be >= 0")).raise();
	}
	Value dataWrapper = self.Lookup(_handle);
	if (dataWrapper.IsNull() or dataWrapper.type != ValueType::Handle) {
		dataWrapper = Value::NewHandle(new RawDataHandleStorage());
		self.GetDict().SetValue(_handle, dataWrapper);
	}
	RawDataHandleStorage *storage = (RawDataHandleStorage*)dataWrapper.data.ref;
	storage->resize(nBytes);
	return IntrinsicResult::Null;
}

// bufReadWord: According to endianness, takes several bytes from `buf` and returns them as a word.
static uint64_t bufReadWord(unsigned char *buf, size_t nBytes, bool isLittleEndian) {
	uint64_t word = 0;
	size_t i, step, stop;
	if (isLittleEndian) {
		i = nBytes - 1;
		step = -1;
		stop = 0;
	} else {
		i = 0;
		step = 1;
		stop = nBytes - 1;
	}
	while (true) {
		word |= buf[i];
		if (i == stop) break;
		i += step;
		word <<= 8;
	}
	return word;
}

// bufWriteWord: According to endianness, stores several bytes from a word into `buf`.
static void bufWriteWord(unsigned char *buf, size_t nBytes, bool isLittleEndian, uint64_t word) {
	size_t i, step, stop;
	if (isLittleEndian) {
		i = 0;
		step = 1;
		stop = nBytes - 1;
	} else {
		i = nBytes - 1;
		step = -1;
		stop = 0;
	}
	while (true) {
		buf[i] = word;
		if (i == stop) break;
		i += step;
		word >>= 8;
	}
}

enum RawDataNotAvailable { rdnaNull, rdnaRaise, rdnaAdjust };

// rawDataGetBytes: Returns a pointer to a fragment of RawData's memory, also checks that `nBytes` are available.
static unsigned char *rawDataGetBytes(Value& rawData, long& offset, long& nBytes, RawDataNotAvailable na = rdnaRaise) {
	Value dataWrapper = rawData.Lookup(_handle);
	if (dataWrapper.IsNull() or dataWrapper.type != ValueType::Handle) {
		switch (na) {
			case rdnaNull:
				return nullptr;
			case rdnaRaise: case rdnaAdjust:
				IndexException(String("Index Error (index out of range)")).raise();
		}
	}
	RawDataHandleStorage *storage = (RawDataHandleStorage*)dataWrapper.data.ref;
	if (offset < 0) offset += storage->dataSize;
	if (offset < 0 or offset > storage->dataSize) {
		IndexException(String("Index Error (index out of range)")).raise();
	}
	if (nBytes < 0) nBytes = storage->dataSize - offset;
	if (offset + nBytes > storage->dataSize) {
		switch (na) {
			case rdnaNull:
				return nullptr;
			case rdnaRaise:
				IndexException(String("Index Error (index out of range)")).raise();
			case rdnaAdjust:
				nBytes = storage->dataSize - offset;
		}
	}
	return (unsigned char *)storage->data + offset;
}

static Value rawDataGetInteger(Context *context, long nBytes, bool isSigned) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	bool isLittleEndian = self.Lookup("littleEndian").BoolValue();
	unsigned char *data = rawDataGetBytes(self, offset, nBytes);
	uint64_t word = bufReadWord(data, nBytes, isLittleEndian);
	if (!isSigned) return Value(word);
	switch (nBytes) {
		case 1:
		{
			union {
				unsigned char u;
				signed char s;
			} un;
			un.u = word;
			return Value(un.s);
		}
		case 2:
		{
			union {
				uint16_t u;
				int16_t s;
			} un;
			un.u = word;
			return Value(un.s);
		}
		case 4:
		{
			union {
				uint32_t u;
				int32_t s;
			} un;
			un.u = word;
			return Value(un.s);
		}
	}
	return Value::null;
}

static void rawDataSetInteger(Context *context, long nBytes, bool isSigned) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	bool littleEndian = self.Lookup("littleEndian").BoolValue();
	unsigned char *data = rawDataGetBytes(self, offset, nBytes);
	union {
		uint64_t u;
		int64_t s;
	} word;
	if (isSigned) {
		word.s = context->GetVar("value").IntValue();
	} else {
		word.u = context->GetVar("value").UIntValue();
	}
	bufWriteWord(data, nBytes, littleEndian, word.u);
}

static Value rawDataGetReal(Context *context, long nBytes) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	bool isLittleEndian = self.Lookup("littleEndian").BoolValue();
	unsigned char *data = rawDataGetBytes(self, offset, nBytes);
	uint64_t word = bufReadWord(data, nBytes, isLittleEndian);
	switch (nBytes) {
		case 4:
		{
			union {
				uint32_t i;
				float r;
			} un;
			un.i = word;
			return Value(un.r);
		}
		case 8:
		{
			union {
				uint64_t i;
				double r;
			} un;
			un.i = word;
			return Value(un.r);
		}
	}
	return Value::null;
}

static void rawDataSetReal(Context *context, long nBytes) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	bool littleEndian = self.Lookup("littleEndian").BoolValue();
	unsigned char *data = rawDataGetBytes(self, offset, nBytes);
	switch (nBytes) {
		case 4:
		{
			union {
				uint32_t i;
				float r;
			} un;
			un.r = context->GetVar("value").FloatValue();
			bufWriteWord(data, nBytes, littleEndian, un.i);
			break;
		}
		case 8:
		{
			union {
				uint64_t i;
				double r;
			} un;
			un.r = context->GetVar("value").DoubleValue();
			bufWriteWord(data, nBytes, littleEndian, un.i);
			break;
		}
	}
}

// byte / sbyte:

static IntrinsicResult intrinsic_rawDataByte(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetInteger(context, 1, false));
}

static IntrinsicResult intrinsic_rawDataSetByte(Context *context, IntrinsicResult partialResult) {
	rawDataSetInteger(context, 1, false);
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_rawDataSbyte(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetInteger(context, 1, true));
}

static IntrinsicResult intrinsic_rawDataSetSbyte(Context *context, IntrinsicResult partialResult) {
	rawDataSetInteger(context, 1, true);
	return IntrinsicResult::Null;
}

// ushort / short

static IntrinsicResult intrinsic_rawDataUshort(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetInteger(context, 2, false));
}

static IntrinsicResult intrinsic_rawDataSetUshort(Context *context, IntrinsicResult partialResult) {
	rawDataSetInteger(context, 2, false);
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_rawDataShort(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetInteger(context, 2, true));
}

static IntrinsicResult intrinsic_rawDataSetShort(Context *context, IntrinsicResult partialResult) {
	rawDataSetInteger(context, 2, true);
	return IntrinsicResult::Null;
}

// uint / int:

static IntrinsicResult intrinsic_rawDataUint(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetInteger(context, 4, false));
}

static IntrinsicResult intrinsic_rawDataSetUint(Context *context, IntrinsicResult partialResult) {
	rawDataSetInteger(context, 4, false);
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_rawDataInt(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetInteger(context, 4, true));
}

static IntrinsicResult intrinsic_rawDataSetInt(Context *context, IntrinsicResult partialResult) {
	rawDataSetInteger(context, 4, true);
	return IntrinsicResult::Null;
}

// ***float and ***double:

static IntrinsicResult intrinsic_rawDataFloat(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetReal(context, 4));
}

static IntrinsicResult intrinsic_rawDataSetFloat(Context *context, IntrinsicResult partialResult) {
	rawDataSetReal(context, 4);
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_rawDataDouble(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(rawDataGetReal(context, 8));
}

static IntrinsicResult intrinsic_rawDataSetDouble(Context *context, IntrinsicResult partialResult) {
	rawDataSetReal(context, 8);
	return IntrinsicResult::Null;
}

// utf8:

static IntrinsicResult intrinsic_rawDataUtf8(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	long nBytes = context->GetVar("bytes").IntValue();
	const char *data = (const char *)rawDataGetBytes(self, offset, nBytes, rdnaNull);
	if (!data) return IntrinsicResult::Null;
	String result(data, nBytes);
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_rawDataSetUtf8(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	String value = context->GetVar("value").GetString();
	long nBytes = value.LengthB();
	unsigned char *data = rawDataGetBytes(self, offset, nBytes, rdnaAdjust);
	if (!data) return IntrinsicResult::Null;
	memcpy(data, value.c_str(), nBytes);
	return IntrinsicResult(nBytes);
}

// _sub: Returns a fragment of RawData as another RawData object.
static IntrinsicResult intrinsic_rawDataSub(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	long nBytes = context->GetVar("bytes").IntValue();
	const char *data = (const char *)rawDataGetBytes(self, offset, nBytes, rdnaNull);
	if (!data) return IntrinsicResult::Null;
	Value dataWrapper = Value::NewHandle(new RawDataHandleStorage(data, nBytes));
	ValueDict instance;
	instance.SetValue(Value::magicIsA, RawDataType());
	instance.SetValue(_handle, dataWrapper);
	Value result(instance);
	return IntrinsicResult(result);
}

// _setSub: Rewrites a frament of RawData with another RawData.
static IntrinsicResult intrinsic_rawDataSetSub(Context *context, IntrinsicResult partialResult) {
	Value self = context->GetVar("self");
	long offset = context->GetVar("offset").IntValue();
	Value other = context->GetVar("rawData");
	if (!self.IsA(RawDataType(), context->vm)) TypeException("RawData parameter is required").raise();
	if (!other.IsA(RawDataType(), context->vm)) TypeException("RawData parameter is required").raise();
	Value selfDataWrapper = self.Lookup(_handle);
	if (selfDataWrapper.IsNull() or selfDataWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	Value otherDataWrapper = other.Lookup(_handle);
	if (otherDataWrapper.IsNull() or otherDataWrapper.type != ValueType::Handle) return IntrinsicResult::Null;
	if (Value::Equality(selfDataWrapper, otherDataWrapper)) RuntimeException("cannot copy RawData into itself").raise();
	RawDataHandleStorage *selfStorage = (RawDataHandleStorage*)selfDataWrapper.data.ref;
	RawDataHandleStorage *otherStorage = (RawDataHandleStorage*)otherDataWrapper.data.ref;
	selfStorage->copyFromOther(*otherStorage, offset, -1, 0, -1);
	return IntrinsicResult::Null;
}

// _concat: Joins several RawData objects into a single one.
static IntrinsicResult intrinsic_rawDataConcat(Context *context, IntrinsicResult partialResult) {
	Value rawDataListV = context->GetVar("rawDataList");
	if (rawDataListV.type != ValueType::List) TypeException("List parameter is required").raise();
	ValueList rawDataList = rawDataListV.GetList();
	ValueList wrappers;
	ValueList offsets;
	long totalBytes = 0;
	for (int i=0; i<rawDataList.Count(); i++) {
		Value elem = rawDataList[i];
		if (!elem.IsA(RawDataType(), context->vm)) {
			TypeException(String("element ") + String::Format(i, "%d") + " should be a RawData object").raise();
		}
		Value elemDataWrapper = elem.Lookup(_handle);
		if (elemDataWrapper.IsNull() or elemDataWrapper.type != ValueType::Handle) continue;
		RawDataHandleStorage *elemStorage = (RawDataHandleStorage*)elemDataWrapper.data.ref;
		if (elemStorage->dataSize > 0) {
			wrappers.Add(elemDataWrapper);
			offsets.Add(totalBytes);
			totalBytes += elemStorage->dataSize;
		}
	}
	RawDataHandleStorage *storage = new RawDataHandleStorage();
	storage->resize(totalBytes);
	for (int i=0; i<wrappers.Count(); i++) {
		Value elemDataWrapper = wrappers[i];
		long offset = offsets[i].IntValue();
		RawDataHandleStorage *elemStorage = (RawDataHandleStorage*)elemDataWrapper.data.ref;
		storage->copyFromOther(*elemStorage, offset, -1, 0, -1);
	}
	Value dataWrapper = Value::NewHandle(storage);
	ValueDict instance;
	instance.SetValue(Value::magicIsA, RawDataType());
	instance.SetValue(_handle, dataWrapper);
	Value result(instance);
	return IntrinsicResult(result);
}

static IntrinsicResult intrinsic_keyAvailable(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(KeyAvailable());
}

static IntrinsicResult intrinsic_keyGet(Context *context, IntrinsicResult partialResult) {
	if (!KeyAvailable().BoolValue()) return IntrinsicResult(Value::null, false);
	ValueDict keyModule = KeyModule();
	Value scanMapV = keyModule.Lookup("_scanMap", Value::null);
	if (scanMapV.type != ValueType::Map) keyModule.ApplyAssignOverride("_scanMap", KeyDefaultScanMap());
	ValueDict scanMap = keyModule.Lookup("_scanMap", Value::null).GetDict();
	return IntrinsicResult(KeyGet(scanMap));
}

static IntrinsicResult intrinsic_keyPut(Context *context, IntrinsicResult partialResult) {
	Value keyChar = context->GetVar("keyChar");
	if (keyChar.type == ValueType::Number) {
		KeyPutCodepoint(keyChar.UIntValue());
	} else if (keyChar.type == ValueType::String) {
		KeyPutString(keyChar.ToString());
	} else {
		TypeException("string or number required for keyChar").raise();
	}
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_keyPutInFront(Context *context, IntrinsicResult partialResult) {
	Value keyChar = context->GetVar("keyChar");
	if (keyChar.type == ValueType::Number) {
		KeyPutCodepoint(keyChar.UIntValue(), true);
	} else if (keyChar.type == ValueType::String) {
		KeyPutString(keyChar.ToString(), true);
	} else {
		TypeException("string or number required for keyChar").raise();
	}
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_keyClear(Context *context, IntrinsicResult partialResult) {
	KeyClear();
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_keyPressed(Context *context, IntrinsicResult partialResult) {
	RuntimeException("`key.pressed` is not implemented").raise();
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_keyKeyNames(Context *context, IntrinsicResult partialResult) {
	RuntimeException("`key.keyNames` is not implemented").raise();
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_keyAxis(Context *context, IntrinsicResult partialResult) {
	RuntimeException("`key.axis` is not implemented").raise();
	return IntrinsicResult::Null;
}

static IntrinsicResult intrinsic_keyEcho(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(KeyGetEcho());
}


static IntrinsicResult intrinsic_exec(Context *context, IntrinsicResult partialResult) {
	double now = context->vm->RunTime();
	if (partialResult.Done()) {
		// This is the initial entry into `exec`.  Fork a subprocess to execute the
		// given command, and return a partial result we can use to check on its progress.
		String cmd = context->GetVar("cmd").ToString();
		double timeout = context->GetVar("timeout").DoubleValue();
		ValueList data;
		if (BeginExec(cmd, timeout, now, &data)) {
			return IntrinsicResult(data, false);
		}
		return IntrinsicResult::Null;
	}
	
	// This is a subsequent entry to intrinsic_exec, where we've already forked
	// the subprocess, and now we're waiting for it to finish.	al time out of the partial result.
	ValueList data = partialResult.Result().GetList();
	String stdOut, stdErr;
	int status = -1;
	if (FinishExec(data, now, &stdOut, &stdErr, &status)) {
		// All done!
		ValueDict result;
		result.SetValue("output", Value(stdOut));
		result.SetValue("errors", Value(stdErr));
		result.SetValue("status", Value(status));
		return IntrinsicResult(result);
	} else {
		// Not done yet.
		return IntrinsicResult(data, false);
	}
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
		fileModule.SetValue("loadRaw", i_loadRaw->GetFunc());
		fileModule.SetValue("saveRaw", i_saveRaw->GetFunc());
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


static bool assignKey(ValueDict& dict, Value key, Value value) {
	if (key.ToString() == "_scanMap") {
		if (value.type != ValueType::Map) return true;	// silently fail because of wrong type.
		ValueDict scanMap = value.GetDict();
		KeyOptimizeScanMap(scanMap);
		dict.SetValue("_scanMap", value);
		return true;
	}
	if (key.ToString() == "_echo") {
		KeySetEcho(value.BoolValue());
		return true;
	}
	return false;	// allow standard assignment to also apply.
}

static ValueDict& KeyModule() {
	static ValueDict keyModule;
	
	if (keyModule.Count() == 0) {
		keyModule.SetValue("available", i_keyAvailable->GetFunc());
		keyModule.SetValue("get", i_keyGet->GetFunc());
		keyModule.SetValue("put", i_keyPut->GetFunc());
		keyModule.SetValue("clear", i_keyClear->GetFunc());
		keyModule.SetValue("pressed", i_keyPressed->GetFunc());
		keyModule.SetValue("keyNames", i_keyKeyNames->GetFunc());
		keyModule.SetValue("axis", i_keyAxis->GetFunc());
		keyModule.SetValue("_putInFront", i_keyPutInFront->GetFunc());
		keyModule.SetValue("_echo", i_keyEcho->GetFunc());
		keyModule.SetAssignOverride(assignKey);
		keyModule.ApplyAssignOverride("_scanMap", KeyDefaultScanMap());
	}
	
	return keyModule;
}

static IntrinsicResult intrinsic_Key(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(KeyModule());
}


static ValueDict& RawDataType() {
	static ValueDict result;
	if (result.Count() == 0) {
		result.SetValue("littleEndian", Value::Truth(true));
		result.SetValue("len", i_rawDataLen->GetFunc());
		result.SetValue("resize", i_rawDataResize->GetFunc());
		result.SetValue("byte", i_rawDataByte->GetFunc());
		result.SetValue("setByte", i_rawDataSetByte->GetFunc());
		result.SetValue("sbyte", i_rawDataSbyte->GetFunc());
		result.SetValue("setSbyte", i_rawDataSetSbyte->GetFunc());
		result.SetValue("ushort", i_rawDataUshort->GetFunc());
		result.SetValue("setUshort", i_rawDataSetUshort->GetFunc());
		result.SetValue("short", i_rawDataShort->GetFunc());
		result.SetValue("setShort", i_rawDataSetShort->GetFunc());
		result.SetValue("uint", i_rawDataUint->GetFunc());
		result.SetValue("setUint", i_rawDataSetUint->GetFunc());
		result.SetValue("int", i_rawDataInt->GetFunc());
		result.SetValue("setInt", i_rawDataSetInt->GetFunc());
		result.SetValue("float", i_rawDataFloat->GetFunc());
		result.SetValue("setFloat", i_rawDataSetFloat->GetFunc());
		result.SetValue("double", i_rawDataDouble->GetFunc());
		result.SetValue("setDouble", i_rawDataSetDouble->GetFunc());
		result.SetValue("utf8", i_rawDataUtf8->GetFunc());
		result.SetValue("setUtf8", i_rawDataSetUtf8->GetFunc());
		result.SetValue("_sub", i_rawDataSub->GetFunc());
		result.SetValue("_setSub", i_rawDataSetSub->GetFunc());
		result.SetValue("_concat", i_rawDataConcat->GetFunc());
	}
	
	return result;
}

static IntrinsicResult intrinsic_RawData(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(RawDataType());
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
			envMap.SetValue(_MS_IMPORT_PATH, "$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib");
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
		if (handle == nullptr) continue;
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
			char* s = realpath(".", nullptr);
			scriptDir = s;
			free(s);
		#endif
	} else {
		#if WINDOWS
			char s[512];
			_fullpath(s, scriptPartialPath, sizeof(s));
			String scriptFullPath = s;
		#else
			char* s = realpath(scriptPartialPath, nullptr);
			String scriptFullPath(s);
			free(s);
		#endif
		scriptDir = dirname(scriptFullPath);
	}
	setEnvVar("MS_SCRIPT_DIR", scriptDir.c_str());
}

void AddPathEnvVars() {
	int length = wai_getExecutablePath(nullptr, 0, nullptr);
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
	
	i_loadRaw = Intrinsic::Create("");
	i_loadRaw->AddParam("path");
	i_loadRaw->code = &intrinsic_loadRaw;
	
	i_saveRaw = Intrinsic::Create("");
	i_saveRaw->AddParam("path");
	i_saveRaw->AddParam("rawData");
	i_saveRaw->code = &intrinsic_saveRaw;
	
	f = Intrinsic::Create("exec");
	f->AddParam("cmd");
	f->AddParam("timeout", 30);
	f->code = &intrinsic_exec;
	
	f = Intrinsic::Create("RawData");
	f->code = &intrinsic_RawData;
	
	f = Intrinsic::Create("key");
	f->code = &intrinsic_Key;
	
	
	// RawData methods
	
	i_rawDataLen = Intrinsic::Create("");
	i_rawDataLen->code = &intrinsic_rawDataLen;
	
	i_rawDataResize = Intrinsic::Create("");
	i_rawDataResize->AddParam("bytes", 32);
	i_rawDataResize->code = &intrinsic_rawDataResize;
	
	i_rawDataByte = Intrinsic::Create("");
	i_rawDataByte->AddParam("self");
	i_rawDataByte->AddParam("offset", 0);
	i_rawDataByte->code = &intrinsic_rawDataByte;
	
	i_rawDataSetByte = Intrinsic::Create("");
	i_rawDataSetByte->AddParam("self");
	i_rawDataSetByte->AddParam("offset", 0);
	i_rawDataSetByte->AddParam("value", 0);
	i_rawDataSetByte->code = &intrinsic_rawDataSetByte;
	
	i_rawDataSbyte = Intrinsic::Create("");
	i_rawDataSbyte->AddParam("self");
	i_rawDataSbyte->AddParam("offset", 0);
	i_rawDataSbyte->code = &intrinsic_rawDataSbyte;
	
	i_rawDataSetSbyte = Intrinsic::Create("");
	i_rawDataSetSbyte->AddParam("self");
	i_rawDataSetSbyte->AddParam("offset", 0);
	i_rawDataSetSbyte->AddParam("value", 0);
	i_rawDataSetSbyte->code = &intrinsic_rawDataSetSbyte;
	
	i_rawDataUshort = Intrinsic::Create("");
	i_rawDataUshort->AddParam("self");
	i_rawDataUshort->AddParam("offset", 0);
	i_rawDataUshort->code = &intrinsic_rawDataUshort;
	
	i_rawDataSetUshort = Intrinsic::Create("");
	i_rawDataSetUshort->AddParam("self");
	i_rawDataSetUshort->AddParam("offset", 0);
	i_rawDataSetUshort->AddParam("value", 0);
	i_rawDataSetUshort->code = &intrinsic_rawDataSetUshort;
	
	i_rawDataShort = Intrinsic::Create("");
	i_rawDataShort->AddParam("self");
	i_rawDataShort->AddParam("offset", 0);
	i_rawDataShort->code = &intrinsic_rawDataShort;
	
	i_rawDataSetShort = Intrinsic::Create("");
	i_rawDataSetShort->AddParam("self");
	i_rawDataSetShort->AddParam("offset", 0);
	i_rawDataSetShort->AddParam("value", 0);
	i_rawDataSetShort->code = &intrinsic_rawDataSetShort;
	
	i_rawDataUint = Intrinsic::Create("");
	i_rawDataUint->AddParam("self");
	i_rawDataUint->AddParam("offset", 0);
	i_rawDataUint->code = &intrinsic_rawDataUint;
	
	i_rawDataSetUint = Intrinsic::Create("");
	i_rawDataSetUint->AddParam("self");
	i_rawDataSetUint->AddParam("offset", 0);
	i_rawDataSetUint->AddParam("value", 0);
	i_rawDataSetUint->code = &intrinsic_rawDataSetUint;
	
	i_rawDataInt = Intrinsic::Create("");
	i_rawDataInt->AddParam("self");
	i_rawDataInt->AddParam("offset", 0);
	i_rawDataInt->code = &intrinsic_rawDataInt;
	
	i_rawDataSetInt = Intrinsic::Create("");
	i_rawDataSetInt->AddParam("self");
	i_rawDataSetInt->AddParam("offset", 0);
	i_rawDataSetInt->AddParam("value", 0);
	i_rawDataSetInt->code = &intrinsic_rawDataSetInt;
	
	i_rawDataFloat = Intrinsic::Create("");
	i_rawDataFloat->AddParam("self");
	i_rawDataFloat->AddParam("offset", 0);
	i_rawDataFloat->code = &intrinsic_rawDataFloat;
	
	i_rawDataSetFloat = Intrinsic::Create("");
	i_rawDataSetFloat->AddParam("self");
	i_rawDataSetFloat->AddParam("offset", 0);
	i_rawDataSetFloat->AddParam("value", 0);
	i_rawDataSetFloat->code = &intrinsic_rawDataSetFloat;
	
	i_rawDataDouble = Intrinsic::Create("");
	i_rawDataDouble->AddParam("self");
	i_rawDataDouble->AddParam("offset", 0);
	i_rawDataDouble->code = &intrinsic_rawDataDouble;
	
	i_rawDataSetDouble = Intrinsic::Create("");
	i_rawDataSetDouble->AddParam("self");
	i_rawDataSetDouble->AddParam("offset", 0);
	i_rawDataSetDouble->AddParam("value", 0);
	i_rawDataSetDouble->code = &intrinsic_rawDataSetDouble;
	
	i_rawDataUtf8 = Intrinsic::Create("");
	i_rawDataUtf8->AddParam("self");
	i_rawDataUtf8->AddParam("offset", 0);
	i_rawDataUtf8->AddParam("bytes", -1);
	i_rawDataUtf8->code = &intrinsic_rawDataUtf8;
	
	i_rawDataSetUtf8 = Intrinsic::Create("");
	i_rawDataSetUtf8->AddParam("self");
	i_rawDataSetUtf8->AddParam("offset", 0);
	i_rawDataSetUtf8->AddParam("value", "");
	i_rawDataSetUtf8->code = &intrinsic_rawDataSetUtf8;
	
	i_rawDataSub = Intrinsic::Create("");
	i_rawDataSub->AddParam("self");
	i_rawDataSub->AddParam("offset", 0);
	i_rawDataSub->AddParam("bytes", -1);
	i_rawDataSub->code = &intrinsic_rawDataSub;
	
	i_rawDataSetSub = Intrinsic::Create("");
	i_rawDataSetSub->AddParam("self");
	i_rawDataSetSub->AddParam("offset", 0);
	i_rawDataSetSub->AddParam("rawData");
	i_rawDataSetSub->code = &intrinsic_rawDataSetSub;
	
	i_rawDataConcat = Intrinsic::Create("");
	i_rawDataConcat->AddParam("rawDataList");
	i_rawDataConcat->code = &intrinsic_rawDataConcat;
	
	// END RawData methods
	
	
	// key.* methods
	
	i_keyAvailable = Intrinsic::Create("");
	i_keyAvailable->code = &intrinsic_keyAvailable;
	
	i_keyGet = Intrinsic::Create("");
	i_keyGet->code = &intrinsic_keyGet;
	
	i_keyPut = Intrinsic::Create("");
	i_keyPut->AddParam("keyChar");
	i_keyPut->code = &intrinsic_keyPut;
	
	i_keyClear = Intrinsic::Create("");
	i_keyClear->code = &intrinsic_keyClear;
	
	i_keyPressed = Intrinsic::Create("");
	i_keyPressed->AddParam("keyName", "space");
	i_keyPressed->code = &intrinsic_keyPressed;
	
	i_keyKeyNames = Intrinsic::Create("");
	i_keyKeyNames->code = &intrinsic_keyKeyNames;
	
	i_keyAxis = Intrinsic::Create("");
	i_keyAxis->AddParam("axis", "Horizontal");
	i_keyAxis->code = &intrinsic_keyAxis;
	
	i_keyPutInFront = Intrinsic::Create("");
	i_keyPutInFront->AddParam("keyChar");
	i_keyPutInFront->code = &intrinsic_keyPutInFront;
	
	i_keyEcho = Intrinsic::Create("");
	i_keyEcho->code = &intrinsic_keyEcho;
	
	// END key.* methods
	
}
