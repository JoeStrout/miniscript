# Key module

`key` module is a port of a [module](https://miniscript.org/wiki/Key) by the same name from Mini Micro that adds keyboard functions for immediate input in the console.

| API | Description |
|---|---|
| `key.available` | compatible |
| `key.get` | compatible |
| `key.put(keyChar)` | compatible |
| `key.clear` | compatible |
| `key.pressed(keyName="space")` | not ported |
| `key.keyNames` | not ported |
| `key.axis(axis="Horizontal")` | not ported |
| `key._putInFront(keyChar)` | (non-standard) same as `key.put` but instead of adding its arg at the end of the input buffer, inserts it at the beginning |
| `key._echo` | (non-standard, only unixes) property that controls whether typed characters are echoed in the terminal |
| `key._scanMap` | (non-standard) property that controls how scan codes and escape sequences are mapped to the values that `key.get` is expected to return |

There's a small demo that demonstrates the use of `key.available` and `key.get`: `demo/tetris.ms`.


## Implementation of the input buffer

The functions of this module maintain their shared internal buffer where key presses are stored.

It's implemented as a `SimpleVector` of entries where each entry is structure of two fields:

| `c` | character code point of a regular (symbol) key |
| `scanCode` | a code of a special key |

Only one of these fields is non-zero at any times.

This data type allows registering key presses on unixes where only code points are used, and Windows where key presses generate either a code point or a sequence of two integers: `0` and a scan code.


## Scan map

Terminals vary in how they report special keys' presses.

For example this is what \[Arrow up\] becomes in the Linux terminal: `char(27) + "[A"` (3 ASCII characters). The same key press on Windows produces `0` followed by scan code `72`. Finally, on Mini Micro it's `char(19)`.

*Scan maps* is an internal mechanism of the `key` module that converts all various values into the same values that are returned by Mini Micro's `key.get` and hence should ensure portability of scripts.

Scan maps are stored in a `key._scanMap` property and are MiniScript maps where the keys are either `number` type (in case you're mapping a scan code) or `string` type (in case of a sequence of characters), and the values are what you want to be returned by `key.get`.

So, to overcome the above \[Arrow up\] problem one could define `key._scanMap` as

```c
key._scanMap = {
	char(27) + "[A": char(19),
	72: char(19),
}
```

There is already a predefined scan map in the `key` module that covers certain special keys (including arrow keys) for each platform.


## Scan map optimization

In games, handling the user input is tipically a part of a game loop.

So, to make `key.get` a bit faster and avoid converting strings into code points on each frame, the scan map gets populated with optimized keys.

This optimization happens on assignment to the `_scanMap` property via the `*AssignOverride` trick.
