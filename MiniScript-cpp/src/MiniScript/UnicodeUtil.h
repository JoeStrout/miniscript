
#ifndef UNICODEUTIL_H
#define UNICODEUTIL_H

namespace MiniScript {	

	// IsUTF8IntraChar
	//
	//	Return whether this is an intra-character byte (in UTF-8 encoding),
	//	i.e., it's not the first or only byte of a character, but some subsequent
	//	byte of a multi-byte character.
	//
	// Author: JJS
	// Used in: AdvanceUTF8, maybe elsewhere
	// Gets: charByte -- byte value to consider
	// Returns: true iff charByte is not the first byte of a character
	// Comment: Jun 03 2002 -- JJS (1)
	inline bool IsUTF8IntraChar(const unsigned char charByte)
	{
		// It's an intra-char character if its high 2 bits are set to 10.
		return 0x80 == (charByte & 0xC0);
	}

	// AdvanceUTF8
	//
	//	Advance the character pointer within a UTF-8 buffer until it hits
	//	a limit defined by another character pointer, or has advanced the
	//	given number of characters.
	//
	// Author: JJS
	// Used in: StringDBCSLeft, etc.
	// Gets: c -- address of a pointer to advance
	//		 maxc -- pointer beyond which *c won't be moved
	//		 count -- how many characters to advance
	// Returns: <nothing>
	// Comment: Jun 03 2002 -- JJS (1)
	void AdvanceUTF8(unsigned char **c, const unsigned char *maxc, int count);

	// BackupUTF8
	//
	//	Back up the character pointer within a UTF-8 buffer until it hits
	//	a limit defined by another character pointer, or has advanced the
	//	given number of characters.
	//
	// Author: JJS
	// Used in: StringDBCSLeft, etc.
	// Gets: c -- address of a pointer to advance
	//		 minc -- pointer beyond which *c won't be moved
	//		 count -- how many characters to advance
	// Returns: <nothing>
	// Comment: Jun 03 2002 -- JJS (1)
	void BackupUTF8(unsigned char **c, const unsigned char *minc, int count);

	// UTF8Encode
	//
	//	Encode the given Unicode code point in UTF-8 form, followed by a
	//	null terminator.  This requires up to 5 bytes.
	//
	// Author: JJS
	// Used in: various
	// Gets: uniChar -- Unicode code point (between 0 and 0x1FFFFF, inclusive)
	//		 outBuf -- pointer to buffer at least 5 bytes long
	// Returns: how many bytes were used (not counting the null), i.e., character Length in bytes
	// Comment: Nov 22 2002 -- JJS (1)
	long UTF8Encode(unsigned long uniChar, unsigned char *outBuf);

	// UTF8Decode
	//
	//	Decode the first character of the given UTF-8 String back into its
	//	Unicode code point.  This is the inverse of UTF8Encode.
	//
	// Author: JJS
	// Used in: various
	// Gets: inBuf -- pointer to buffer containing at least one UTF-8 character
	// Returns: Unicode code point (between 0 and 0x1FFFFF, inclusive)
	// Comment: Dec 02 2002 -- JJS (1)
	unsigned long UTF8Decode(unsigned char *inBuf);

	// UTF8DecodeAndAdvance
	//
	//	Decode the first character of the given UTF-8 String back into its
	//	Unicode code point, and advance the given pointer to the next character.
	//	This is like calling UTF8Decode followed by UTF8Advance, but is more
	//	efficient.
	//
	// Author: JJS
	// Used in: various
	// Gets: inBuf -- address of pointer to buffer containing at least one UTF-8 character
	// Returns: Unicode code point (between 0 and 0x1FFFFF, inclusive)
	// Comment: Mar 04 2003 -- JJS (1)
	unsigned long UTF8DecodeAndAdvance(unsigned char **inBuf);

	// Various case-conversion utilities

	unsigned long UnicodeCharToUpper(unsigned long low);
	unsigned long UnicodeCharToLower(unsigned long upper);
	void UTF8ToUpper(unsigned char *utf8String, unsigned long byteCount,
					 unsigned char **outBuf, unsigned long *outByteCount);
	void UTF8ToLower(unsigned char *utf8String, unsigned long byteCount,
					 unsigned char **outBuf, unsigned long *outByteCount);
	void UTF8Capitalize(unsigned char *utf8String, unsigned long byteCount,
					 unsigned char **outBuf, unsigned long *outByteCount);
	bool UTF8IsCaseless(unsigned char *utf8String, unsigned long byteCount);				// Apr 02 2003 -- JJS (1)
	unsigned short *UCS2ToUpper(unsigned short *ucs2String, unsigned long byteCount);
	unsigned short *UCS2ToLower(unsigned short *ucs2String, unsigned long byteCount);
	unsigned short *UCS2Capitalize(unsigned short *ucs2String, unsigned long byteCount);

	// UnicodeCharCompare
	//
	//	Compare the two Unicode code points as for sorting.  True sorting is
	//	extremely difficult as the preferred sort order varies by language
	//	and region.  So, for now, we just sort lower Unicode code points
	//	before higher ones.
	//
	// Author: JJS
	// Used in: various
	// Gets: leftChar -- left character to compare
	//		 rightChar -- right chracter to compare
	//		 ignoreCase -- if true, consider different case versions to be equal
	// Returns: -1 if leftChar < rightChar, 0 if equal, 1 if leftChar > rightChar
	// Comment: Mar 04 2003 -- JJS (1)
	inline long UnicodeCharCompare(unsigned long leftChar, unsigned long rightChar,
								   bool ignoreCase=false)
	{
		if (leftChar == rightChar) return 0;
		if (ignoreCase) {
			leftChar = UnicodeCharToUpper( leftChar );
			rightChar = UnicodeCharToUpper( rightChar );
		}
		if (leftChar < rightChar) return -1;
		if (leftChar > rightChar) return 1;
		return 0;
	}

	// String comparisons

	long UTF8StringCompare(unsigned char *leftBuf, unsigned long leftByteCount,
						   unsigned char *rightBuf, unsigned long rightByteCount,
						   bool ignoreCase=false);

	long UCS2StringCompare(unsigned short *leftBuf, unsigned long leftByteCount,
						   unsigned short *rightBuf, unsigned long rightByteCount,
						   bool ignoreCase=false);

	// UnicodeCharIsWhitespace
	//
	//	Return whether the given Unicode character should be considered whitespace.
	//
	// Author: JJS
	// Used in: UTF8Capitalize, UCS2Capitalize
	// Gets: uniChar -- Unicode code point (between 0 and 0x1FFFFF, inclusive)
	// Returns: true if it's whitespace, false if not
	// Comment: Mar 04 2003 -- JJS (1)
	inline bool UnicodeCharIsWhitespace(unsigned long uniChar)
	{

		// From the list of whitespace on the Unicode webpage, found
		// at: <http://www.unicode.org/Public/UNIDATA/PropList.txt>
		//
		// 0009..000D    ; White_Space # Cc   [5] <control-0009>..<control-000D>
		// 0020          ; White_Space # Zs       SPACE
		// 0085          ; White_Space # Cc       <control-0085>
		// 00A0          ; White_Space # Zs       NO-BREAK SPACE
		// 1680          ; White_Space # Zs       OGHAM SPACE MARK
		// 180E          ; White_Space # Zs       MONGOLIAN VOWEL SEPARATOR
		// 2000..200A    ; White_Space # Zs  [11] EN QUAD..HAIR SPACE
		// 2028          ; White_Space # Zl       LINE SEPARATOR
		// 2029          ; White_Space # Zp       PARAGRAPH SEPARATOR
		// 202F          ; White_Space # Zs       NARROW NO-BREAK SPACE
		// 205F          ; White_Space # Zs       MEDIUM MATHEMATICAL SPACE
		// 3000          ; White_Space # Zs       IDEOGRAPHIC SPACE
		
		return ((uniChar >= 0x9 && uniChar <= 0xD) ||
			(uniChar >= 0x2000 && uniChar <= 0x200A) ||
			uniChar == 0x20 || uniChar == 0x85 ||
			uniChar == 0xA0 || uniChar == 0x1680 ||
			uniChar == 0x180E || uniChar == 0x2028 ||
			uniChar == 0x2029 || uniChar == 0x202F ||
			uniChar == 0x205F || uniChar == 0x3000);		// Jan 05 2005 -- AJB (1)
	}
}
	
#endif // UNICODEUTIL_H
