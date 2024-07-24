#include "Key.h"
#include "UnicodeUtil.h"

#if _WIN32 || _WIN64
	#define WINDOWS 1
	
	// ...
	
#else
	#include <termios.h>
	#include <unistd.h>
	#include <sys/select.h>
#endif

namespace MiniScript {


SimpleVector<struct InputBufferEntry> inputBuffer;

ValueDict& KeyDefaultScanMap() {
	static ValueDict scanMap;
	if (scanMap.Count() == 0) {
		#if WINDOWS
			
			// ...
			
		#else
			scanMap.SetValue("\x7F", "\x08");     // backspace
			scanMap.SetValue("\x1B[3~", "\x7F");  // delete
			scanMap.SetValue("\x1B[A", "\x13");   // up
			scanMap.SetValue("\x1B[B", "\x14");   // down
			scanMap.SetValue("\x1B[C", "\x12");   // right
			scanMap.SetValue("\x1B[D", "\x11");   // left
			scanMap.SetValue("\x1B[H", "\x01");   // home
			scanMap.SetValue("\x1B[F", "\x05");   // end
		#endif
	}
	return scanMap;
}

void KeyOptimizeScanMap(ValueDict& scanMap) {
	for (ValueDictIterator kv = scanMap.GetIterator(); !kv.Done(); kv.Next()) {
		Value k = kv.Key();
		Value v = kv.Value();
		if (k.type == ValueType::Number) {
			ValueList optimizedKey;
			optimizedKey.Add(Value::zero);
			optimizedKey.Add(k);
			scanMap.SetValue(optimizedKey, v);
		} else if (k.type == ValueType::String) {
			String kStr(k.ToString());
			if (kStr.Length() == 0) continue;
			String first(kStr.Substring(0, 1));
			String rest(kStr.Substring(1));
			ValueList optimizedKey;
			optimizedKey.Add(UTF8Decode((unsigned char *)first.c_str()));
			optimizedKey.Add(Value::zero);
			if (rest.Length() == 0) {
				scanMap.SetValue(optimizedKey, v);
			} else {
				Value subV = scanMap.Lookup(optimizedKey, Value::null);
				if (subV.IsNull() || subV.type != ValueType::Map) {
					ValueDict d;
					scanMap.SetValue(optimizedKey, d);
				}
				ValueDict sub = scanMap.Lookup(optimizedKey, Value::null).GetDict();
				sub.SetValue(rest, v);
				KeyOptimizeScanMap(sub);
			}
		}
	}
}

// Helper function to read all available characters from STDIN into the global input buffer.
void slurpStdin() {
	struct InputBufferEntry e = {0, 0};
	
	#if WINDOWS
	
	// ...
	
	#else
	
	// Make terminal reads non-blocking
	struct termios ttystate, backUp;
	if (tcgetattr(STDIN_FILENO, &ttystate) < 0) return;
	memcpy(&backUp, &ttystate, sizeof(ttystate));
	ttystate.c_lflag &= ~ICANON;
	ttystate.c_cc[VMIN] = 0;
	ttystate.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSANOW, &ttystate) < 0) return;
	
	unsigned char buf[5] = {0, 0, 0, 0, 0};
	unsigned char *p = buf;
	while (true) {
		
		// Check if there are some key presses
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) < 0) break;
		if (!FD_ISSET(STDIN_FILENO, &fds)) {
			
			// Nothing's left to slurp ; save to the input buffer a current unsaved character
			if (p > buf) {
				e.c = UTF8Decode(buf);
				inputBuffer.push_back(e);
			}
			break;
		}
		
		// Read one byte from STDIN
		if (read(STDIN_FILENO, p, 1) < 1) break;
		if (p > buf && !IsUTF8IntraChar(*p)) {
			
			// A new character has begun under `p`, save the previous one to the input buffer and continue slurping
			e.c = UTF8Decode(buf);
			inputBuffer.push_back(e);
			buf[0] = *p;
			buf[1] = 0;
			buf[2] = 0;
			buf[3] = 0;
			buf[4] = 0;
			p = buf;
		}
		p++;
	}
	
	// Restore terminal
	tcsetattr(STDIN_FILENO, TCSANOW, &backUp);
	
	#endif
}

Value KeyAvailable() {
	slurpStdin();
	return Value::Truth(inputBuffer.size() > 0);
}

Value KeyGet(ValueDict& scanMap) {
	slurpStdin();
	if (inputBuffer.size() == 0) return Value::null;
	struct InputBufferEntry e = inputBuffer[0];
	struct InputBufferEntry initialE = e;
	inputBuffer.deleteIdx(0);
	int nScanned = 0;
	while (true) {
		ValueList optimizedKey;
		optimizedKey.Add(e.c);
		optimizedKey.Add(e.scanCode);
		Value foundVal = scanMap.Lookup(optimizedKey, Value::null);
		if (foundVal.IsNull()) break;
		if (foundVal.type == ValueType::String) {
			for (int i=0; i<nScanned; i++) {
				inputBuffer.deleteIdx(0);
			}
			return foundVal;
		} else if (foundVal.type == ValueType::Map) {
			scanMap = foundVal.GetDict();
			e = inputBuffer[nScanned++];
			continue;
		} else {
			break;	// malformed scan map
		}
	}
	if (initialE.c == 0) {
		Value v(initialE.scanCode);
		return v;
	}
	unsigned char buf[5] = {0, 0, 0, 0, 0};
	long nBytes = UTF8Encode(initialE.c, buf);
	String s((char *)buf, nBytes);
	Value v(s);
	return v;
}

void KeyPutCodepoint(long codepoint, bool inFront) {
	struct InputBufferEntry e = {0, 0};
	e.c = codepoint;
	if (inFront) {
		inputBuffer.insert(e, 0);
	} else {
		inputBuffer.push_back(e);
	}
}

void KeyPutString(String s, bool inFront) {
	if (inFront) {
		for (int i=s.Length()-1; i>=0; i--) {
			struct InputBufferEntry e = {0, 0};
			String character = s.Substring(i, 1);
			e.c = UTF8Decode((unsigned char *)character.c_str());
			inputBuffer.insert(e, 0);
		}
	} else {
		for (int i=0; i<s.Length(); i++) {
			struct InputBufferEntry e = {0, 0};
			String character = s.Substring(i, 1);
			e.c = UTF8Decode((unsigned char *)character.c_str());
			inputBuffer.push_back(e);
		}
	}
}

void KeyClear() {
	slurpStdin();
	inputBuffer.deleteAll();
}

bool KeyGetEcho() {
	#if WINDOWS
	
	// ...
	return false;
	
	#else
	
	struct termios ttystate;
	if (tcgetattr(STDIN_FILENO, &ttystate) < 0) return false;
	return ttystate.c_lflag & ECHO;
	
	#endif
}

void KeySetEcho(bool on) {
	#if WINDOWS
	
	// ...
	
	#else
	
	struct termios ttystate;
	if (tcgetattr(STDIN_FILENO, &ttystate) < 0) return;
	if (on) {
		ttystate.c_lflag |= ECHO;
	} else {
		ttystate.c_lflag &= ~ECHO;
	}
	tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
	
	#endif
}


}
