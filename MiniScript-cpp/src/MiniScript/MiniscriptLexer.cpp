//
//  MiniscriptLexer.cpp
//  MiniScript
//
//  Created by Joe Strout on 5/30/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "MiniscriptLexer.h"
#include "MiniscriptKeywords.h"
#include "MiniscriptErrors.h"
#include "UnitTest.h"

namespace MiniScript {

	Token Token::EOL(Token::Type::EOL);
	
	String Token::ToString() {
		String result;
		switch (type) {
			case Token::Type::Unknown: 		result = "Unknown"; 	break;
			case Token::Type::Keyword: 		result = "Keyword"; 	break;
			case Token::Type::Number: 		result = "Number"; 		break;
			case Token::Type::String: 		result = "String"; 		break;
			case Token::Type::Identifier: 	result = "Identifier"; 	break;
			case Token::Type::OpAssign: 	result = "OpAssign"; 	break;
			case Token::Type::OpPlus: 		result = "OpPlus"; 		break;
			case Token::Type::OpMinus: 		result = "OpMinus"; 	break;
			case Token::Type::OpTimes: 		result = "OpTimes"; 	break;
			case Token::Type::OpDivide: 	result = "OpDivide"; 	break;
			case Token::Type::OpMod: 		result = "OpMod"; 		break;
			case Token::Type::OpPower: 		result = "OpPower"; 	break;
			case Token::Type::OpEqual: 		result = "OpEqual"; 	break;
			case Token::Type::OpNotEqual: 	result = "OpNotEqual"; 	break;
			case Token::Type::OpGreater: 	result = "OpGreater"; 	break;
			case Token::Type::OpGreatEqual: result = "OpGreatEqual"; break;
			case Token::Type::OpLesser: 	result = "OpLesser"; 	break;
			case Token::Type::OpLessEqual: 	result = "OpLessEqual"; break;
			case Token::Type::LParen: 		result = "LParen"; 		break;
			case Token::Type::RParen: 		result = "RParen"; 		break;
			case Token::Type::LSquare:		result = "LSquare"; 	break;
			case Token::Type::RSquare: 		result = "RSquare"; 	break;
			case Token::Type::LCurly: 		result = "LCurly"; 		break;
			case Token::Type::RCurly: 		result = "RCurly"; 		break;
			case Token::Type::AddressOf: 	result = "AddressOf"; 	break;
			case Token::Type::Comma: 		result = "Comma"; 		break;
			case Token::Type::Dot: 			result = "Dot"; 		break;
			case Token::Type::Colon: 		result = "Colon"; 		break;
			case Token::Type::Comment: 		result = "Comment"; 	break;
			case Token::Type::EOL: 			result = "EOL"; 		break;
		}
		if (!text.empty()) result = result + "(" + text + ")";
		return result;
	}

	bool Lexer::IsNumeric(char c) {
		return c >= '0' && c <= '9';
	}
	
	bool Lexer::IsIdentifier(char c) {
		return c == '_'
		|| (c >= 'a' && c <= 'z')
		|| (c >= 'A' && c <= 'Z')
		|| (c >= '0' && c <= '9')
		|| (unsigned char)c > 0x9F;
	}
	
	bool Lexer::IsWhitespace(char c) {
		return c == ' ' || c == '\t';
	}

	Token Lexer::Dequeue() {
		if (!ls) return Token(Token::Type::Unknown);
		if (ls->pending.Count() > 0) {
			Token result = ls->pending[0];
			ls->pending.RemoveAt(0);
			return result;
		}
		
		long oldPos = ls->positionB;
		SkipWhitespaceAndComment();
		
		if (atEnd()) return Token::EOL;
		
		Token result;
		long startPosB = ls->positionB;
		result.afterSpace = (startPosB > oldPos);
		char c = ls->input[ls->positionB++];
		
		// Handle two-character operators first.
		if (!atEnd()) {
			char c2 = ls->input[ls->positionB];
			if (c == '=' && c2 == '=') result.type = Token::Type::OpEqual;
			if (c == '!' && c2 == '=') result.type = Token::Type::OpNotEqual;
			if (c == '>' && c2 == '=') result.type = Token::Type::OpGreatEqual;
			if (c == '<' && c2 == '=') result.type = Token::Type::OpLessEqual;
			
			if (result.type != Token::Type::Unknown) {
				ls->positionB++;
				return result;
			}
		}

		if (c == '+') result.type = Token::Type::OpPlus;
		else if (c == '-') result.type = Token::Type::OpMinus;
		else if (c == '*') result.type = Token::Type::OpTimes;
		else if (c == '/') result.type = Token::Type::OpDivide;
		else if (c == '%') result.type = Token::Type::OpMod;
		else if (c == '^') result.type = Token::Type::OpPower;
		else if (c == '(') result.type = Token::Type::LParen;
		else if (c == ')') result.type = Token::Type::RParen;
		else if (c == '[') result.type = Token::Type::LSquare;
		else if (c == ']') result.type = Token::Type::RSquare;
		else if (c == '{') result.type = Token::Type::LCurly;
		else if (c == '}') result.type = Token::Type::RCurly;
		else if (c == ',') result.type = Token::Type::Comma;
		else if (c == ':') result.type = Token::Type::Colon;
		else if (c == '=') result.type = Token::Type::OpAssign;
		else if (c == '<') result.type = Token::Type::OpLesser;
		else if (c == '>') result.type = Token::Type::OpGreater;
		else if (c == '@') result.type = Token::Type::AddressOf;
		else if (c == ';' || c == '\n') {
			result.type = Token::Type::EOL;
			result.text = c == ';' ? ";" : "\n";
			if (c != ';') ls->lineNum++;
		}
		if (c == '\r') {
			// Careful; DOS may use \r\n, so we need to check for that too.
			result.type = Token::Type::EOL;
			if (ls->positionB < ls->inputLengthB && ls->input[ls->positionB] == '\n') {
				ls->positionB++;
				result.text = "\r\n";
			} else {
				result.text = "\r";
			}
			ls->lineNum++;
		}
		if (result.type != Token::Type::Unknown) return result;

		
		// Then, handle more extended tokens.
		
		if (c == '.') {
			// A token that starts with a dot is just Type.Dot, UNLESS
			// it is followed by a number, in which case it's a decimal number.
			if (ls->positionB >= ls->inputLengthB || !IsNumeric(ls->input[ls->positionB])) {
				result.type = Token::Type::Dot;
				return result;
			}
		}
		
		if (c == '.' or IsNumeric(c)) {
			result.type = Token::Type::Number;
			while (ls->positionB < ls->inputLengthB) {
				char lastc = c;
				c = ls->input[ls->positionB];
				if (IsNumeric(c) or c == '.' or c == 'E' or c == 'e' ||
					((c == '-' or c == '+') and (lastc == 'E' or lastc == 'e'))) {
					ls->positionB++;
				} else break;
			}
		} else if (IsIdentifier(c)) {
			while (ls->positionB < ls->inputLengthB) {
				if (IsIdentifier(ls->input[ls->positionB])) ls->positionB++;
				else break;
			}
			result.text = ls->input.SubstringB(startPosB, ls->positionB - startPosB);
			result.type = (Keywords::IsKeyword(result.text) ? Token::Type::Keyword : Token::Type::Identifier);
			if (result.text == "end") {
				// As a special case: when we see "end", grab the next keyword (after whitespace)
				// too, and conjoin it, so our token is "end if", "end function", etc.
				Token nextWord = Dequeue();
				if (nextWord.type == Token::Type::Keyword) {
					result.text = result.text + " " + nextWord.text;
				} else {
					// Oops, didn't find another keyword.  User error.
					throw new LexerException("'end' without following keyword ('if', 'function', etc.)");
				}
			} else if (result.text == "else") {
				// And similarly, conjoin an "if" after "else" (to make "else if").
				long p = ls->positionB;
				while (p < ls->inputLengthB and (ls->input[p]==' ' or ls->input[p]=='\t')) p++;
				if (p+1 < ls->inputLengthB and ls->input.SubstringB(p, 2) == "if" and
						(p+2 >= ls->inputLengthB or IsWhitespace(ls->input[p+2]))) {
					result.text = "else if";
					ls->positionB = p + 2;
				}
			}
			return result;
		} else if (c == '"') {
			// Lex a string... to the closing ", but skipping (and singling) a doubled double quote ("")
			result.type = Token::Type::String;
			bool haveDoubledQuotes = false;
			startPosB = ls->positionB;
			bool gotEndQuote = false;
			while (ls->positionB < ls->inputLengthB) {
				c = ls->input[ls->positionB++];
				if (c == '"') {
					if (ls->positionB < ls->inputLengthB and ls->input[ls->positionB] == '"') {
						// This is just a doubled quote.
						haveDoubledQuotes = true;
						ls->positionB++;
					} else {
						// This is the closing quote, marking the end of the string.
						gotEndQuote = true;
						break;
					}
				}
			}
			if (!gotEndQuote) throw new LexerException("missing closing quote (\")");
			result.text = ls->input.SubstringB(startPosB, ls->positionB - startPosB - 1);
			if (haveDoubledQuotes) result.text = result.text.Replace("\"\"", "\"");
			return result;
			
		} else {
			result.type = Token::Type::Unknown;
		}
		
		result.text = ls->input.SubstringB(startPosB, ls->positionB - startPosB);
		return result;
	}
	
	Token Lexer::Peek() {
		if (!ls) return Token(Token::Type::Unknown);
		if (ls->pending.Count() == 0) {
			if (atEnd()) return Token::EOL;
			ls->pending.Add(Dequeue());
		}
		return ls->pending[0];
	}

	void Lexer::SkipWhitespaceAndComment() {
		while (!atEnd() && IsWhitespace(ls->input[ls->positionB])) {
			ls->positionB++;
		}
		
		if (ls->positionB < ls->input.LengthB() - 1 and ls->input[ls->positionB] == '/' and ls->input[ls->positionB + 1] == '/') {
			// Comment.  Skip to end of line.
			ls->positionB += 2;
			while (!atEnd() && ls->input[ls->positionB] != '\n') ls->positionB++;
		}
	}
	
	bool Lexer::IsInStringLiteral(long charPosB, String source, long startPosB) {
		bool inString = false;
		for (long i=startPosB; i<charPosB; i++) {
			if (source[i] == '"') inString = !inString;
		}
		return inString;
	}
	
	long Lexer::CommentStartPosB(String source, long startPosB) {
		// Find the first occurrence of "//" in this line that
		// is not within a string literal.
		long commentStartB = startPosB-2;
		while (true) {
			commentStartB = source.IndexOfB("//", commentStartB + 2);
			if (commentStartB < 0) break;	// no comment found
			if (!IsInStringLiteral(commentStartB, source, startPosB)) break;	// valid comment
		}
		return commentStartB;
	}
	
	String Lexer::TrimComment(String source) {
		long startPosB = source.LastIndexOfB('\n') + 1;
		long commentStartB = CommentStartPosB(source, startPosB);
		if (commentStartB >= 0) return source.Substring(startPosB, commentStartB - startPosB);
		return source;
	}
	
	// Find the last token in the given source, ignoring any whitespace
	// or comment at the end of that line.
	Token Lexer::LastToken(String source) {
		// Start by finding the start and logical  end of the last line.
		long startPosB = source.LastIndexOfB('\n') + 1;
		long commentStartB = CommentStartPosB(source, startPosB);
		
		// Walk back from end of string or start of comment, skipping whitespace.
		long endPos = (commentStartB >= 0 ? commentStartB-1 : source.LengthB() - 1);
		while (endPos >= 0 and IsWhitespace(source[endPos])) endPos--;
		if (endPos < 0) return Token::EOL;
		
		// Find the start of that last token.
		// There are several cases to consider here.
		long tokStartB = endPos;
		char c = source[endPos];
		if (IsIdentifier(c)) {
			while (tokStartB > startPosB && IsIdentifier(source[tokStartB-1])) tokStartB--;
		} else if (c == '"') {
			bool inQuote = true;
			while (tokStartB > startPosB) {
				tokStartB--;
				if (source[tokStartB] == '"') {
					inQuote = !inQuote;
					if (!inQuote && tokStartB > startPosB && source[tokStartB-1] != '"') break;
				}
			}
		} else if (c == '=' && tokStartB > startPosB) {
			char c2 = source[tokStartB-1];
			if (c2 == '>' || c2 == '<' || c2 == '=' || c2 == '!') tokStartB--;
		}
		
		// Now use the standard lexer to grab just that bit.
		Lexer lex(source);
		lex.ls->positionB = tokStartB;
		return lex.Dequeue();
	}

	
	//--------------------------------------------------------------------------------
	class TestLexer : public UnitTest
	{
	public:
		TestLexer() : UnitTest("Lexer") {}
		virtual void Run();
		
	private:
		void check(Token tok, Token::Type type);
		void check(Token tok, Token::Type type, String text);
	};

	void TestLexer::check(Token tok, Token::Type type) {
		Assert(tok.type == type);
	}
	
	void TestLexer::check(Token tok, Token::Type type, String text) {
		Assert(tok.type == type and tok.text == text);
	}
	
	void TestLexer::Run()
	{
		Lexer lex("42  * 3.14158");
		Assert(not lex.isNull() and not lex.atEnd());
		Assert(lex.lineNum() == 1);
		check(lex.Dequeue(), Token::Type::Number, "42");
		check(lex.Dequeue(), Token::Type::OpTimes);
		check(lex.Dequeue(), Token::Type::Number, "3.14158");
		Assert(lex.atEnd());
		Assert(lex.lineNum() == 1);
		
		lex = Lexer("6*(.1-foo) end if // and a comment!");
		check(lex.Dequeue(), Token::Type::Number, "6");
		Assert(lex.lineNum() == 1);
		check(lex.Dequeue(), Token::Type::OpTimes);
		check(lex.Dequeue(), Token::Type::LParen);
		check(lex.Dequeue(), Token::Type::Number, ".1");
		check(lex.Dequeue(), Token::Type::OpMinus);
		check(lex.Peek(), Token::Type::Identifier, "foo");
		check(lex.Peek(), Token::Type::Identifier, "foo");
		check(lex.Dequeue(), Token::Type::Identifier, "foo");
		check(lex.Dequeue(), Token::Type::RParen);
		check(lex.Dequeue(), Token::Type::Keyword, "end if");
		check(lex.Dequeue(), Token::Type::EOL);
		Assert(lex.atEnd());
		Assert(lex.lineNum() == 1);
		
		lex = Lexer("\"foo\" \"isn't \"\"real\"\"\" \"now \"\"\"\" double!\"");
		check(lex.Dequeue(), Token::Type::String, "foo");
		check(lex.Dequeue(), Token::Type::String, "isn't \"real\"");
		check(lex.Dequeue(), Token::Type::String, "now \"\" double!");
		Assert(lex.atEnd());
		
		lex = Lexer("foo\nbar\rbaz\r\nbamf");
		check(lex.Dequeue(), Token::Type::Identifier, "foo");
		Assert(lex.lineNum() == 1);
		check(lex.Dequeue(), Token::Type::EOL);
		check(lex.Dequeue(), Token::Type::Identifier, "bar");
		Assert(lex.lineNum() == 2);
		check(lex.Dequeue(), Token::Type::EOL);
		check(lex.Dequeue(), Token::Type::Identifier, "baz");
		Assert(lex.lineNum() == 3);
		check(lex.Dequeue(), Token::Type::EOL);
		check(lex.Dequeue(), Token::Type::Identifier, "bamf");
		Assert(lex.lineNum() == 4);
		check(lex.Dequeue(), Token::Type::EOL);
		Assert(lex.atEnd());
		
		check(Lexer::LastToken("x=42 // foo"), Token::Type::Number, "42");
		check(Lexer::LastToken("x = [1, 2, // foo"), Token::Type::Comma);
		check(Lexer::LastToken("x = [1, 2 // foo"), Token::Type::Number, "2");
		check(Lexer::LastToken("x = [1, 2 // foo // and \"more\" foo"), Token::Type::Number, "2");
		check(Lexer::LastToken("x = [\"foo\", \"//bar\"]"), Token::Type::RSquare);
		check(Lexer::LastToken("print 1 // line 1\nprint 2"), Token::Type::Number, "2");
		check(Lexer::LastToken("print \"Hi\"\"Quote\" // foo bar"), Token::Type::String, "Hi\"Quote");
	}
	
	RegisterUnitTest(TestLexer);
}

