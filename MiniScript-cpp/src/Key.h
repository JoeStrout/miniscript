//
//  Key.hpp
//  MiniScript
//

#ifndef KEY_MODULE_H
#define KEY_MODULE_H

#include <stdio.h>
#include <wchar.h>
#include "MiniscriptTypes.h"
#include "SimpleVector.h"

namespace MiniScript {


// InputBufferEntry: Elements of the input buffer.
struct InputBufferEntry {
	wchar_t c;         // inputted character (if a regular key, otherwise 0)
	wchar_t scanCode;  // scan code (if a special key, otherwise 0)
};

// inputBuffer: Global input buffer to store key presses.
extern SimpleVector<struct InputBufferEntry> inputBuffer;

// KeyDefaultScanMap: Returns a platform specific dictionary that maps keys' scan codes to the return values of `key.get()`.
ValueDict& KeyDefaultScanMap();

// KeyOptimizeScanMap: Adds optimized keys to the scan map for faster `key.get()`s.
void KeyOptimizeScanMap(ValueDict& scanMap);

// KeyAvailable: Checks whether there is a keypress in the input buffer.
Value KeyAvailable();

// KeyGet: Pulls the next key out of the input buffer.
// (This function immediately returns Value::null if the buffer is empty, but the caller intrinsic will still wait if necessary
// to comply with the specification of `key.get()`).
Value KeyGet(ValueDict& scanMap);

// KeyPutCodepoint: Enqueues a single character by code point into the keyboard buffer.
void KeyPutCodepoint(long codepoint, bool inFront=false);

// KeyPutString: Enqueues a string into the keyboard buffer.
void KeyPutString(String s, bool inFront=false);

// KeyClear: Clears the input buffer.
void KeyClear();

// KeyGetEcho: Returns whether terminal echo is on.
bool KeyGetEcho();

// KeySetEcho: Sets terminal echo on or off.
void KeySetEcho(bool on);


}

#endif /* KEY_MODULE_H */
