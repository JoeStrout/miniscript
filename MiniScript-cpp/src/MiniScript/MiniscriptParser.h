//
//  MiniscriptParser.hpp
//  MiniScript
//
//  Created by Joe Strout on 6/1/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MINISCRIPTPARSER_H
#define MINISCRIPTPARSER_H

#include "String.h"
#include "List.h"
#include "Dictionary.h"
#include "MiniscriptTAC.h"
#include "MiniscriptLexer.h"

namespace MiniScript {
	
	// BackPatch: represents a place where we need to patch the code to fill
	// in a jump destination (once we figure out where that destination is).
	class BackPatch {
	public:
		long lineNum;			// which code line to patch
		String waitingFor;		// what keyword we're waiting for (e.g., "end if")
		
		BackPatch() {}
		BackPatch(long lineNum, String waitFor) : lineNum(lineNum), waitingFor(waitFor) {}
	};
	
	// JumpPoint: represents a place in the code we will need to jump to later
	// (typically, the top of a loop of some sort).
	class JumpPoint {
	public:
		long lineNum;			// line number to jump to
		String keyword;			// jump type, by keyword: "while", "for", etc.
		
		JumpPoint() {}
		JumpPoint(long lineNum, String keyword) : lineNum(lineNum), keyword(keyword) {}
	};

	class ParseState {
	public:
		List<TACLine> code;
		List<BackPatch> backpatches;
		List<JumpPoint> jumpPoints;
		int nextTempNum;
		
		bool empty() { return code.Count() == 0; }
		
		void Clear() {
			code = List<TACLine>();
			backpatches = List<BackPatch>();
			jumpPoints = List<JumpPoint>();
			nextTempNum = 0;
		}
		
		void Add(TACLine line) { code.Add(line); }
		
		/// <summary>
		/// Add the last code line as a backpatch point, to be patched
		/// (in rhsA) when we encounter a line with the given waitFor.
		/// </summary>
		/// <param name="waitFor">what to wait for</param>
		void AddBackpatch(String waitFor) {
			backpatches.Add(BackPatch(code.Count()-1, waitFor));
		}

		void AddJumpPoint(String jumpKeyword) {
			jumpPoints.Add(JumpPoint(code.Count(), jumpKeyword));
		}
		
		JumpPoint CloseJumpPoint(String keyword);
		
		bool IsJumpTarget(long lineNum);
		
		/// <summary>
		/// Call this method when we've found an 'end' keyword, and want
		/// to patch up any jumps that were waiting for that.  Patch the
		/// matching backpatch (and any after it) to the current code end.
		/// </summary>
		/// <param name="keywordFound">Keyword found.</param>
		/// <param name="reservingLines">Extra lines (after the current position) to patch to.</param>
		void Patch(String keywordFound, long reservingLines=0) {
			Patch(keywordFound, false, reservingLines);
		}
		
		/// <summary>
		/// Call this method when we've found an 'end' keyword, and want
		/// to patch up any jumps that were waiting for that.  Patch the
		/// matching backpatch (and any after it) to the current code end.
		/// </summary>
		/// <param name="keywordFound">Keyword found.</param>
		/// <param name="alsoBreak">If true, also patch "break"; otherwise skip it.</param>
		/// <param name="reservingLines">Extra lines (after the current position) to patch to.</param>
		void Patch(String keywordFound, bool alsoBreak, long reservingLines=0);

		/// <summary>
		/// Patches up all the branches for a single open if block.  That includes
		/// the last "else" block, as well as one or more "end if" jumps.
		/// </summary>
		void PatchIfBlock();
	};
	
	class Parser {
	public:
		String errorContext;	// name of file, etc., used for error reporting
		
		// Stack of open code blocks we're working on (while compiling a function,
		// we push a new one onto this stack, compile to that, and then pop it
		// off when we reach the end of the function).
		List<ParseState> outputStack;
		
		// Partial input, in the case where line continuation has been used.
		String partialInput;
		
		// Handy reference to the top of outputStack.
		ParseState* output;
		
		// A new parse state that needs to be pushed onto the stack, as soon as we
		// finish with the current line we're working on; valid when pending=true.
		ParseState pendingState;
		bool pending;
		
		Parser() { Reset(); }
		
		/// <summary>
		/// Completely clear out and reset our parse state, throwing out
		/// any code and intermediate results.
		/// </summary>
		void Reset() {
			outputStack.Clear();
			outputStack.Add(ParseState());
			output = &outputStack[0];
			pendingState.Clear();
			pending = false;
		}

		/// <summary>
		/// Partially reset, abandoning backpatches, but keeping already-
		/// compiled code.  This would be used in a REPL, when the user
		/// may want to reset and continue after a botched loop or function.
		/// </summary>
		void PartialReset() {
			while (outputStack.Count() > 1) outputStack.Pop();
			output = &outputStack[0];
			output->backpatches.Clear();
			output->jumpPoints.Clear();
			output->nextTempNum = 0;
		}

		bool NeedMoreInput() {
			return (not partialInput.empty() or outputStack.Count() > 1 or output->backpatches.Count() > 0);
		}

		void Parse(String sourceCode, bool replMode=false);

		Machine *CreateVM(TextOutputMethod standardOutput);
		
	private:
		static ParseState nullState;
		
		/// <summary>
		/// Parse multiple statements until we run out of tokens, or reach 'end function'.
		/// </summary>
		/// <param name="tokens">Tokens.</param>
		void ParseMultipleLines(Lexer tokens);

		void ParseStatement(Lexer tokens, bool allowExtra=false);
		void ParseAssignment(Lexer tokens, bool allowExtra=false);
		Value ParseExpr(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseFunction(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseOr(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseAnd(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseNot(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseIsA(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseComparisons(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseAddSub(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseMultDiv(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseUnaryMinus(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseNew(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseAddressOf(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParsePower(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseDotExpr(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseCallExpr(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseSeqLookup(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseMap(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseList(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseQuantity(Lexer tokens, bool asLval=false, bool statementStart=false);
		Value ParseCallArgs(Value funcRef, Lexer tokens);
		Value ParseAtom(Lexer tokens, bool asLval=false, bool statementStart=false);

		Value FullyEvaluate(Value val);
		void StartElseClause();
		Token RequireToken(Lexer tokens, Token::Type type, String text=String());
		Token RequireEitherToken(Lexer tokens, Token::Type type1, String text1, Token::Type type2, String text2=String());
		Token RequireEitherToken(Lexer tokens, Token::Type type1, Token::Type type2, String text2=String()) {
			return RequireEitherToken(tokens, type1, String(), type2, text2);
		}

	};
	
}

#endif /* MINISCRIPTPARSER_H */
