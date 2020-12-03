//
//  MiniscriptParser.cpp
//  MiniScript
//
//  Created by Joe Strout on 6/1/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "MiniscriptParser.h"
#include "MiniscriptErrors.h"
#include "MiniscriptIntrinsics.h"
#include "UnitTest.h"

namespace MiniScript {
	
	// Find the TAC operator that corresponds to the given token type,
	// for comparisons.  If it's not a comparison operator, return TACLine::Op::Noop.
	static TACLine::Op ComparisonOp(Token::Type tokenType) {
		switch (tokenType) {
			case Token::Type::OpEqual:		return TACLine::Op::AEqualB;
			case Token::Type::OpNotEqual:	return TACLine::Op::ANotEqualB;
			case Token::Type::OpGreater:	return TACLine::Op::AGreaterThanB;
			case Token::Type::OpGreatEqual:	return TACLine::Op::AGreatOrEqualB;
			case Token::Type::OpLesser:		return TACLine::Op::ALessThanB;
			case Token::Type::OpLessEqual:	return TACLine::Op::ALessOrEqualB;
			default: return TACLine::Op::Noop;
		}
	}
	
	JumpPoint ParseState::CloseJumpPoint(String keyword) {
		long idx = jumpPoints.Count() - 1;
		if (idx < 0 || jumpPoints[idx].keyword != keyword) {
			throw new CompilerException(String("'end ") + keyword + "' without matching '" + keyword + "'");
		}
		JumpPoint result = jumpPoints[idx];
		jumpPoints.RemoveAt(idx);
		return result;
	}
	
	// Return whether the given line is a jump target.
	bool ParseState::IsJumpTarget(long lineNum) {
		for (int i=0; i < code.Count(); i++) {
			TACLine::Op op = code[i].op;
			if ((op == TACLine::Op::GotoA || op == TACLine::Op::GotoAifB
				 || op == TACLine::Op::GotoAifNotB || op == TACLine::Op::GotoAifTrulyB)
				&& code[i].rhsA.type == ValueType::Number && code[i].rhsA.IntValue() == lineNum) return true;
		}
		for (int i=0; i<jumpPoints.Count(); i++) {
			if (jumpPoints[i].lineNum == lineNum) return true;
		}
		return false;
	}


	void ParseState::Patch(String keywordFound, bool alsoBreak, long reservingLines) {
		Value target = code.Count() + reservingLines;
		bool done = false;
		for (long idx = backpatches.Count() - 1; idx >= 0 and not done; idx--) {
			bool patchIt = false;
			if (backpatches[idx].waitingFor == keywordFound) patchIt = done = true;
			else if (backpatches[idx].waitingFor == "break") {
				// Not the expected keyword, but "break"; this is always OK,
				// but we may or may not patch it depending on the call.
				patchIt = alsoBreak;
			} else {
				// Not the expected patch, and not "break"; we have a mismatched block start/end.
				throw new CompilerException("'" + keywordFound + "' skips expected '" + backpatches[idx].waitingFor + "'");
			}
			if (patchIt) {
				code[backpatches[idx].lineNum].rhsA = target;
				backpatches.RemoveAt(idx);
			}
		}
		// Make sure we found one...
		if (!done) throw new CompilerException("'" + keywordFound + "' without matching block starter");
	}

	/// <summary>
	/// Patches up all the branches for a single open if block.  That includes
	/// the last "else" block, as well as one or more "end if" jumps.
	/// </summary>
	void ParseState::PatchIfBlock() {
		Value target = code.Count();
		
		long idx = backpatches.Count() - 1;
		while (idx >= 0) {
			BackPatch bp = backpatches[idx];
			if (bp.waitingFor == "if:MARK") {
				// There's the special marker that indicates the true start of this if block.
				backpatches.RemoveAt(idx);
				return;
			} else if (bp.waitingFor == "end if" or bp.waitingFor == "else") {
				code[bp.lineNum].rhsA = target;
				backpatches.RemoveAt(idx);
			} else if (backpatches[idx].waitingFor == "break") {
				// Not the expected keyword, but "break"; this is always OK.
			} else {
				// Not the expected patch, and not "break"; we have a mismatched block start/end.
				throw new CompilerException("'end if' without matching 'if'");
			}
			idx--;
		}
		// If we get here, we never found the expected if:MARK.  That's an error.
		throw new CompilerException("'end if' without matching 'if'");
	}
	
	static void AllowLineBreak(Lexer tokens) {
		while (tokens.Peek().type == Token::Type::EOL && not tokens.atEnd()) tokens.Dequeue();
	}
	
	//------------------------------------------------------------------------------------------
	
	ParseState Parser::nullState;
	
	void Parser::Parse(String sourceCode, bool replMode) {
		if (replMode) {
			// Check for an incomplete final line by finding the last (non-comment) token.
			bool isPartial;
			Token lastTok = Lexer::LastToken(sourceCode);
			// Almost any token at the end will signify line continuation, except:
			switch (lastTok.type) {
				case Token::Type::EOL:
				case Token::Type::Identifier:
				case Token::Type::Keyword:
				case Token::Type::Number:
				case Token::Type::RCurly:
				case Token::Type::RParen:
				case Token::Type::RSquare:
				case Token::Type::String:
				case Token::Type::Unknown:
					isPartial = false;
					break;
				default:
					isPartial = true;
					break;
			}
			if (isPartial) {
				partialInput += Lexer::TrimComment(sourceCode);
				return;
			}
		}
		
		Lexer tokens(partialInput + sourceCode);
		partialInput = "";
		ParseMultipleLines(tokens);
		
		if (not replMode and NeedMoreInput()) {
			// Whoops, we need more input but we don't have any.  This is an error.
			if (outputStack.Count() > 1) {
				throw new CompilerException(errorContext, tokens.lineNum() + 1,
											"'function' without matching 'end function'");
			} else if (output->backpatches.Count() > 0) {
				BackPatch bp = output->backpatches[output->backpatches.Count() - 1];
				String msg;
				if (bp.waitingFor == "end for") msg = "'for' without matching 'end for'";
				else if (bp.waitingFor == "end if") msg = "'if' without matching 'end if'";
				else if (bp.waitingFor == "end while") msg = "'while' without matching 'end while'";
				else msg = "unmatched block opener";
				throw new CompilerException(errorContext, tokens.lineNum() + 1, msg);
			}
		}
	}
	
	/// <summary>
	/// Parse multiple statements until we run out of tokens, or reach 'end function'.
	/// </summary>
	/// <param name="tokens">Tokens.</param>
	void Parser::ParseMultipleLines(Lexer tokens) {
		while (!tokens.atEnd()) {
			// Skip any blank lines
			if (tokens.Peek().type == Token::Type::EOL) {
				tokens.Dequeue();
				continue;
			}
			
			// Prepare a source code location for error reporting
			SourceLoc location(errorContext, tokens.lineNum());
			
			// Pop our context if we reach 'end function'.
			if (tokens.Peek().type == Token::Type::Keyword && tokens.Peek().text == "end function") {
				tokens.Dequeue();
				if (outputStack.Count() > 1) {
					// Console.WriteLine("Popping compiler output stack");
					outputStack.Pop();
					output = &outputStack.Last();
				} else {
					CompilerException* e = new CompilerException("'end function' without matching block starter");
					e->location = location;
					throw e;
				}
				continue;
			}
			
			// Parse one line (statement).
			long outputStart = output->code.Count();
			try {
				ParseStatement(tokens);
			} catch (MiniscriptException* mse) {
				if (mse->location.lineNum == 0) mse->location = location;
				throw mse;
			}
			// Fill in the location info for all the TAC lines we just generated.
			for (long i = outputStart; i < output->code.Count(); i++) {
				output->code[i].location = location;
			}
		}
	}

	void Parser::ParseStatement(Lexer tokens, bool allowExtra) {
		if (tokens.Peek().type == Token::Type::Keyword and tokens.Peek().text != "not"
				and tokens.Peek().text != "true" and tokens.Peek().text != "false") {
			// Handle statements that begin with a keyword.
			String keyword = tokens.Dequeue().text;
			if (keyword == "return") {
				Value returnValue;
				if (tokens.Peek().type != Token::Type::EOL) {
					returnValue = ParseExpr(tokens);
				}
				output->Add(TACLine(Value::Temp(0), TACLine::Op::ReturnA, returnValue));
			}
			else if (keyword == "if") {
				Value condition = ParseExpr(tokens);
				RequireToken(tokens, Token::Type::Keyword, "then");
				// OK, now we need to emit a conditional branch, but keep track of this
				// on a stack so that when we get the corresponding "else" or  "end if",
				// we can come back and patch that jump to the right place.
				output->Add(TACLine(TACLine::Op::GotoAifNotB, Value::null, condition));

				// ...but if blocks also need a special marker in the backpack stack
				// so we know where to stop when patching up (possibly multiple) 'end if' jumps.
				// We'll push a special dummy backpatch here that we look for in PatchIfBlock.
				output->AddBackpatch("if:MARK");
				output->AddBackpatch("else");
				
				// Allow for the special one-statement if: if the next token after "then"
				// is not EOL, then parse a statement, and do the same for any else or
				// else-if blocks, until we get to EOL (and then implicitly do "end if").
				if (tokens.Peek().type != Token::Type::EOL) {
					ParseStatement(tokens, true);  // parses a single statement for the "then" body
					if (tokens.Peek().type == Token::Type::Keyword and tokens.Peek().text == "else") {
						tokens.Dequeue();	// skip "else"
						StartElseClause();
						ParseStatement(tokens, true);		// parse a single statement for the "else" body
					} else {
						RequireEitherToken(tokens, Token::Type::Keyword, "else", Token::Type::EOL);
					}
					output->PatchIfBlock();	// terminate the single-line if
				} else {
					tokens.Dequeue();	// skip EOL
				}
				return;
			} else if (keyword == "else") {
				StartElseClause();
			} else if (keyword == "else if") {
				StartElseClause();

				Value condition = ParseExpr(tokens);
				RequireToken(tokens, Token::Type::Keyword, "then");
				output->Add(TACLine(TACLine::Op::GotoAifNotB, Value::null, condition));
				output->AddBackpatch("else");
			} else if (keyword == "end if") {
				// OK, this is tricky.  We might have an open "else" block or we might not.
				// And, we might have multiple open "end if" jumps (one for the if part,
				// and another for each else-if part).  Patch all that as a special case.
				output->PatchIfBlock();
			} else if (keyword == "while") {
				// We need to note the current line, so we can jump back up to it at the end.
				output->AddJumpPoint(keyword);

				// Then parse the condition.
				Value condition = ParseExpr(tokens);

				// OK, now we need to emit a conditional branch, but keep track of this
				// on a stack so that when we get the corresponding "end while",
				// we can come back and patch that jump to the right place.
				output->Add(TACLine(TACLine::Op::GotoAifNotB, Value::null, condition));
				output->AddBackpatch("end while");
			} else if (keyword == "end while") {
				// Unconditional jump back to the top of the while loop.
				JumpPoint jump = output->CloseJumpPoint("while");
				output->Add(TACLine(TACLine::Op::GotoA, jump.lineNum));
				// Then, backpatch the open "while" branch to here, right after the loop.
				// And also patch any "break" branches emitted after that point.
				output->Patch(keyword, "break");
			} else if (keyword == "for") {
				// Get the loop variable, "in" keyword, and expression to loop over.
				// (Note that the expression is only evaluated once, before the loop.)
				Token loopVarTok = RequireToken(tokens, Token::Type::Identifier);
				Value loopVar = Value::Var(loopVarTok.text);
				RequireToken(tokens, Token::Type::Keyword, "in");
				Value stuff = ParseExpr(tokens);
				if (stuff.type == ValueType::Null) {
					throw new CompilerException(errorContext, tokens.lineNum(),
												"sequence expression expected for 'for' loop");
				}

				// Create an index variable to iterate over the sequence, initialized to -1.
				Value idxVar = Value::Var("__" + loopVarTok.text + "_idx");
				output->Add(TACLine(idxVar, TACLine::Op::AssignA, -1));

				// We need to note the current line, so we can jump back up to it at the end.
				output->AddJumpPoint(keyword);

				// Now increment the index variable, and branch to the end if it's too big.
				// (We'll have to backpatch this branch later.)
				output->Add(TACLine(idxVar, TACLine::Op::APlusB, idxVar, Value::one));
				Value sizeOfSeq = Value::Temp(output->nextTempNum++);
				output->Add(TACLine(sizeOfSeq, TACLine::Op::LengthOfA, stuff));
				Value isTooBig = Value::Temp(output->nextTempNum++);
				output->Add(TACLine(isTooBig, TACLine::Op::AGreatOrEqualB, idxVar, sizeOfSeq));
				output->Add(TACLine(TACLine::Op::GotoAifB, Value::null, isTooBig));
				output->AddBackpatch("end for");

				// Otherwise, get the sequence value into our loop variable.
				output->Add(TACLine(loopVar, TACLine::Op::ElemBofIterA, stuff, idxVar));
			} else if (keyword == "end for") {
				// Unconditional jump back to the top of the for loop.
				JumpPoint jump = output->CloseJumpPoint("for");
				output->Add(TACLine(TACLine::Op::GotoA, jump.lineNum));
				// Then, backpatch the open "for" branch to here, right after the loop.
				// And also patch any "break" branches emitted after that point.
				output->Patch(keyword, "break");
			} else if (keyword == "break") {
				// Emit a jump to the end, to get patched up later.
				output->Add(TACLine(TACLine::Op::GotoA, Value::null));
				output->AddBackpatch("break");
			} else if (keyword == "continue") {
				// Jump unconditionally back to the current open jump point.
				if (output->jumpPoints.Count() == 0) {
					throw new CompilerException(errorContext, tokens.lineNum(),
												"'continue' without open loop block");
				}
				JumpPoint jump = output->jumpPoints.Last();
				output->Add(TACLine(TACLine::Op::GotoA, jump.lineNum));
			} else {
					throw new CompilerException(errorContext, tokens.lineNum(),
												"unexpected keyword '" + keyword + "' at start of line");
			}
		} else {
			ParseAssignment(tokens, allowExtra);
		}

		// A statement should consume everything to the end of the line.
		if (!allowExtra) RequireToken(tokens, Token::Type::EOL);

		// Finally, if we have a pending state, because we encountered a function(),
		// then push it onto our stack now that we're done with that statement.
		if (pending) {
			//				Console.WriteLine("PUSHING NEW PARSE STATE");
			outputStack.Add(pendingState);
			output = &outputStack.Last();
			pending = false;
		}
	}

	void Parser::StartElseClause() {
		// Back-patch the open if block, but leaving room for the jump:
		// Emit the jump from the current location, which is the end of an if-block,
		// to the end of the else block (which we'll have to back-patch later).
		output->Add(TACLine(TACLine::Op::GotoA, Value::null));
		// Back-patch the previously open if-block to jump here (right past the goto).
		output->Patch("else");
		// And open a new back-patch for this goto (which will jump all the way to the end if).
		output->AddBackpatch("end if");
	}
	
	void Parser::ParseAssignment(Lexer tokens, bool allowExtra) {
		Value expr = ParseExpr(tokens, true, true);
		Value lhs, rhs;
		Token peek = tokens.Peek();
		if (peek.type == Token::Type::EOL ||
				(peek.type == Token::Type::Keyword && peek.text == "else")) {
			// No explicit assignment; store an implicit result
			rhs = FullyEvaluate(expr);
			output->Add(TACLine(TACLine::Op::AssignImplicit, rhs));
			return;
		}
		if (peek.type == Token::Type::OpAssign) {
			tokens.Dequeue();	// skip '='
			lhs = expr;
			rhs = ParseExpr(tokens);
		} else {
			// This looks like a command statement. Parse the rest
			// of the line as arguments to a function call.
			Value funcRef = expr;
			int argCount = 0;
			while (true) {
				Value arg = ParseExpr(tokens);
				output->Add(TACLine(TACLine::Op::PushParam, arg));
				argCount++;
				if (tokens.Peek().type == Token::Type::EOL) break;
				if (tokens.Peek().type == Token::Type::Keyword and tokens.Peek().text == "else") break;
				if (tokens.Peek().type == Token::Type::Comma) {
					tokens.Dequeue();
					AllowLineBreak(tokens);
					continue;
				}
				if (RequireEitherToken(tokens, Token::Type::Comma, Token::Type::EOL).type == Token::Type::EOL) break;
			}
			Value result = Value::Temp(output->nextTempNum++);
			output->Add(TACLine(result, TACLine::Op::CallFunctionA, funcRef, Value(argCount)));
			output->Add(TACLine(TACLine::Op::AssignImplicit, result));
			return;
		}

		// OK, now, in many cases our last TAC line at this point is an assignment to our RHS temp.
		// In that case, as a simple (but very useful) optimization, we can simply patch that to
		// assign to our lhs instead.  BUT, we must not do this if there are any jumps to the next
		// line, as may happen due to short-cut evaluation (issue #6).
		if (rhs.type == ValueType::Temp and output->code.Count() > 0 and !output->IsJumpTarget(output->code.Count())) {
			TACLine& line = output->code[output->code.Count() - 1];
			if (line.lhs == rhs) {
				// Yep, that's the case.  Patch it up.
				line.lhs = lhs;
				return;
			}
		}
		
		// If the last line was us creating and assigning a function, then we don't add a second assign
		// op, we instead just update that line with the proper LHS.
		if (rhs.type == ValueType::Function && output->code.Count() > 0) {
			TACLine& line = output->code[output->code.Count() - 1];
			if (line.op == TACLine::Op::BindAssignA) {
				line.lhs = lhs;
				return;
			}
		}
		
		// In any other case, do an assignment statement to our lhs.
		output->Add(TACLine(lhs, TACLine::Op::AssignA, rhs));
	}

	Value Parser::ParseExpr(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseFunction;
		return (*this.*nextLevel)(tokens, asLval, statementStart);
	}

	Value Parser::ParseFunction(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseOr;
		
		Token tok = tokens.Peek();
		if (tok.type != Token::Type::Keyword or tok.text != "function") return (*this.*nextLevel)(tokens, asLval, statementStart);
		tokens.Dequeue();
		
		RequireToken(tokens, Token::Type::LParen);
		
		FunctionStorage *func = new FunctionStorage();
		
		while (tokens.Peek().type != Token::Type::RParen) {
			// parse a parameter: a comma-separated list of
			//			identifier
			//	or...	identifier = expr
			Token id = tokens.Dequeue();
			if (id.type != Token::Type::Identifier) throw new CompilerException(errorContext, tokens.lineNum(),
											String("got ") + id.ToString() + " where an identifier is required");
			Value defaultValue;
			if (tokens.Peek().type == Token::Type::OpAssign) {
				tokens.Dequeue();	// skip '='
				defaultValue = ParseExpr(tokens);
			}
			func->parameters.Add(FuncParam(id.text, defaultValue));
			if (tokens.Peek().type == Token::Type::RParen) break;
			RequireToken(tokens, Token::Type::Comma);
		}
		
		RequireToken(tokens, Token::Type::RParen);
		
		// Now, we need to parse the function body into its own parsing context.
		// But don't push it yet -- we're in the middle of parsing some expression
		// or statement in the current context, and need to finish that.
		if (pending) throw new CompilerException(errorContext, tokens.lineNum(),
													  "can't start two functions in one statement");
		pendingState = ParseState();
		pendingState.code = List<TACLine>(16);	// Important to ensure we have storage, which will get shared with that in outputStack.
		pendingState.nextTempNum = 1;			// (since 0 is used to hold return value)
		pending = true;
		//			Console.WriteLine("STARTED FUNCTION");
		
		// Create a function object attached to the new parse state code.
		func->code = pendingState.code;
		Value valFunc = Value(func);
		output->Add(TACLine(Value::null, TACLine::Op::BindAssignA, valFunc));
		return valFunc;
	}
	
	Value Parser::ParseOr(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseAnd;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		List<long> jumpLineIndexes;
		Token tok = tokens.Peek();
		while (tok.type == Token::Type::Keyword and tok.text == "or") {
			tokens.Dequeue();		// discard "or"
			val = FullyEvaluate(val);
			
			AllowLineBreak(tokens); // allow a line break after a binary operator
			
			// Set up a short-circuit jump based on the current value;
			// we'll fill in the jump destination later.  Note that the
			// usual GotoAifB opcode won't work here, without breaking
			// our calculation of intermediate truth.  We need to jump
			// only if our truth value is >= 1 (i.e. absolutely true).
			jumpLineIndexes.Add(output->code.Count());
			TACLine jump(TACLine::Op::GotoAifTrulyB, Value::null, val);
			output->Add(jump);
			
			Value opB = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::AOrB, val, opB));
			val = Value::Temp(tempNum);
			
			tok = tokens.Peek();
		}
		
		// Now, if we have any short-circuit jumps, those are going to need
		// to copy the short-circuit result (always 1) to our output temp.
		// And anything else needs to skip over that.  So:
		long jumps = jumpLineIndexes.Count();
		if (jumps > 0) {
			output->Add(TACLine(TACLine::Op::GotoA, Value(output->code.Count()+2)));	// skip over this line:
			output->Add(TACLine(val, TACLine::Op::AssignA, Value::one));	// result = 1
			for (long i=0; i<jumps; i++) {
				long idx = jumpLineIndexes[i];
				output->code[idx].rhsA = Value(output->code.Count()-1);	// short-circuit to the above result=1 line
			}
		}
		
		return val;
	}

	Value Parser::ParseAnd(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseNot;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		List<long> jumpLineIndexes;
		Token tok = tokens.Peek();
		while (tok.type == Token::Type::Keyword and tok.text == "and") {
			tokens.Dequeue();		// discard "and"
			val = FullyEvaluate(val);
			
			AllowLineBreak(tokens); // allow a line break after a binary operator
			
			// Set up a short-circuit jump based on the current value;
			// we'll fill in the jump destination later.
			jumpLineIndexes.Add(output->code.Count());
			TACLine jump = TACLine(Value::null, TACLine::Op::GotoAifNotB, Value::null, val);
			output->Add(jump);
			
			Value opB = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::AAndB, val, opB));
			val = Value::Temp(tempNum);
			
			tok = tokens.Peek();
		}
		
		// Now, if we have any short-circuit jumps, those are going to need
		// to copy the short-circuit result (always 0) to our output temp.
		// And anything else needs to skip over that.  So:
		long jumps = jumpLineIndexes.Count();
		if (jumps > 0) {
			output->Add(TACLine(Value::null, TACLine::Op::GotoA, Value(output->code.Count()+2)));	// skip over this line:
			output->Add(TACLine(val, TACLine::Op::AssignA, Value::zero));	// result = 0
			for (long i=0; i<jumps; i++) {
				long codeIdx = jumpLineIndexes[i];
				output->code[codeIdx].rhsA = Value(output->code.Count()-1);	// short-circuit to the above result=0 line
			}
		}
		
		return val;
	}
	
	Value Parser::ParseNot(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseIsA;
		Token tok = tokens.Peek();
		Value val;
		if (tok.type == Token::Type::Keyword and tok.text == "not") {
			tokens.Dequeue();		// discard "not"

			AllowLineBreak(tokens); // allow a line break after a unary operator
			
			val = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::NotA, val));
			val = Value::Temp(tempNum);
		} else {
			val = (*this.*nextLevel)(tokens, asLval, statementStart);
		}
		return val;
	}

	Value Parser::ParseIsA(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseComparisons;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		if (tokens.Peek().type == Token::Type::Keyword && tokens.Peek().text == "isa") {
			tokens.Dequeue();		// discard the isa operator
			AllowLineBreak(tokens); // allow a line break after a binary operator
			Value opB = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::AisaB, val, opB));
			val = Value::Temp(tempNum);
		}
		return val;
	}

	Value Parser::ParseComparisons(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseAddSub;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		Value opA = val;
		TACLine::Op opcode = ComparisonOp(tokens.Peek().type);
		// Parse a String of comparisons, all multiplied together
		// (so every comparison must be true for the whole expression to be true).
		bool firstComparison = true;
		while (opcode != TACLine::Op::Noop) {
			tokens.Dequeue();	// discard the operator (we have the opcode)
			opA = FullyEvaluate(opA);
			
			AllowLineBreak(tokens); // allow a line break after a binary operator
			
			Value opB = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			output->Add(TACLine(Value::Temp(tempNum), opcode, opA, opB));
			if (firstComparison) {
				firstComparison = false;
			} else {
				tempNum = output->nextTempNum++;
				output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::ATimesB, val, Value::Temp(tempNum - 1)));
			}
			val = Value::Temp(tempNum);
			opA = opB;
			opcode = ComparisonOp(tokens.Peek().type);
		}
		return val;
	}
	
	Value Parser::ParseAddSub(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseMultDiv;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		Token tok = tokens.Peek();
		while (tok.type == Token::Type::OpPlus ||
			   (tok.type == Token::Type::OpMinus
				&& (!statementStart || !tok.afterSpace  || tokens.isAtWhitespace()))) {
			tokens.Dequeue();

			AllowLineBreak(tokens); // allow a line break after a binary operator
				   
			val = FullyEvaluate(val);
			Value opB = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			output->Add(TACLine(Value::Temp(tempNum),
							   tok.type == Token::Type::OpPlus ? TACLine::Op::APlusB : TACLine::Op::AMinusB,
							   val, opB));
			val = Value::Temp(tempNum);
			
			tok = tokens.Peek();
		}
		return val;
	}
	
	Value Parser::ParseMultDiv(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseUnaryMinus;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		Token tok = tokens.Peek();
		while (tok.type == Token::Type::OpTimes or tok.type == Token::Type::OpDivide or tok.type == Token::Type::OpMod) {
			tokens.Dequeue();

			AllowLineBreak(tokens); // allow a line break after a binary operator
			
			val = FullyEvaluate(val);
			Value opB = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			switch (tok.type) {
				case Token::Type::OpTimes:
					output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::ATimesB, val, opB));
					break;
				case Token::Type::OpDivide:
					output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::ADividedByB, val, opB));
					break;
				default:  // Token::Type::OpMod:
					output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::AModB, val, opB));
					break;
			}
			val = Value::Temp(tempNum);
			
			tok = tokens.Peek();
		}
		return val;
	}
	
	Value Parser::ParseUnaryMinus(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseNew;
		if (tokens.Peek().type != Token::Type::OpMinus) return (*this.*nextLevel)(tokens, asLval, statementStart);
		tokens.Dequeue();		// skip '-'

		AllowLineBreak(tokens); // allow a line break after a unary operator
		
		Value val = (*this.*nextLevel)(tokens, false, false);
		if (val.type == ValueType::Number) {
			// If what follows is a numeric literal, just invert it and be done!
			val.data.number = -val.data.number;
			return val;
		}
		// Otherwise, subtract it from 0 and return a new temporary.
		int tempNum = output->nextTempNum++;
		output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::AMinusB, Value::zero, val));
		return Value::Temp(tempNum);
	}

	Value Parser::ParseNew(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseAddressOf;
		if (tokens.Peek().type != Token::Type::Keyword or tokens.Peek().text != "new") return (*this.*nextLevel)(tokens, asLval, statementStart);
		tokens.Dequeue();		// skip 'new'

		AllowLineBreak(tokens); // allow a line break after a unary operator
		
		// Grab a reference to our __isa value
		Value isa = (*this.*nextLevel)(tokens, false, false);
		// Now, create a new map, and set __isa on it to that.
		// NOTE: we must be sure this map gets created at runtime, not here at parse time.
		// Since it is a mutable object, we need to return a different one each time
		// this code executes (in a loop, function, etc.).  So, we use Op.CopyA below!
		ValueDict map;
		map.SetValue(Value::magicIsA, isa);
		Value result = Value::Temp(output->nextTempNum++);
		output->Add(TACLine(result, TACLine::Op::CopyA, Value(map)));
		return result;
	}
	
	Value Parser::ParseAddressOf(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParsePower;
		if (tokens.Peek().type != Token::Type::AddressOf) return (*this.*nextLevel)(tokens, asLval, statementStart);
		tokens.Dequeue();
		AllowLineBreak(tokens); // allow a line break after a unary operator
		Value val = (*this.*nextLevel)(tokens, true, statementStart);
		val.noInvoke = true;
		return val;
	}

	Value Parser::ParsePower(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseCallExpr;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		Token tok = tokens.Peek();
		while (tok.type == Token::Type::OpPower) {
			tokens.Dequeue();

			AllowLineBreak(tokens); // allow a line break after a binary operator
			
			val = FullyEvaluate(val);
			Value opB = (*this.*nextLevel)(tokens, false, false);
			int tempNum = output->nextTempNum++;
			output->Add(TACLine(Value::Temp(tempNum), TACLine::Op::APowB, val, opB));
			val = Value::Temp(tempNum);

			tok = tokens.Peek();
		}
		return val;
	}

	Value Parser::ParseCallExpr(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseMap;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);
		while (true) {
			if (tokens.Peek().type == Token::Type::Dot) {
				tokens.Dequeue();	// discard '.'
				AllowLineBreak(tokens); // allow a line break after a binary operator
				Token nextIdent = RequireToken(tokens, Token::Type::Identifier);
				// We're chaining sequences here; look up (by invoking)
				// the previous part of the sequence, so we can build on it.
				val = FullyEvaluate(val);
				// Now build the lookup.
				val = Value::SeqElem(val, Value(nextIdent.text));
				if (tokens.Peek().type == Token::Type::LParen && !tokens.Peek().afterSpace) {
					// If this new element is followed by parens, we need to
					// parse it as a call right away.
					val = ParseCallArgs(val, tokens);
				}
			} else if (tokens.Peek().type == Token::Type::LSquare && !tokens.Peek().afterSpace) {
				tokens.Dequeue();	// discard '['
				AllowLineBreak(tokens); // allow a line break after open bracket
				val = FullyEvaluate(val);
				
				if (tokens.Peek().type == Token::Type::Colon) {	// e.g., foo[:4]
					tokens.Dequeue();	// discard ':'
					AllowLineBreak(tokens); // allow a line break after colon
					Value index2;
					if (tokens.Peek().type != Token::Type::RSquare) index2 = ParseExpr(tokens);
					Value temp = Value::Temp(output->nextTempNum++);
					Intrinsics::CompileSlice(output->code, val, Value::null, index2, temp.data.tempNum);
					val = temp;
				} else {
					Value index = ParseExpr(tokens);
					if (tokens.Peek().type == Token::Type::Colon) {	// e.g., foo[2:4] or foo[2:]
						tokens.Dequeue();	// discard ':'
						AllowLineBreak(tokens); // allow a line break after colon
						Value index2;
						if (tokens.Peek().type != Token::Type::RSquare) index2 = ParseExpr(tokens);
						Value temp = Value::Temp(output->nextTempNum++);
						Intrinsics::CompileSlice(output->code, val, index, index2, temp.data.tempNum);
						val = temp;
					} else {			// e.g., foo[3]  (not a slice at all)
						if (statementStart) {
							// At the start of a statement, we don't want to compile the
							// last sequence lookup, because we might have to convert it into
							// an assignment.  But we want to compile any previous one.
							if (val.type == ValueType::SeqElem) {
								SeqElemStorage *vsVal = (SeqElemStorage*)(val.data.ref);
								Value temp = Value::Temp(output->nextTempNum++);
								output->Add(TACLine(temp, TACLine::Op::ElemBofA, vsVal->sequence, vsVal->index));
								val = temp;
							}
							val = Value::SeqElem(val, index);
						} else {
							// Anywhere else in an expression, we can compile the lookup right away.
							Value temp = Value::Temp(output->nextTempNum++);
							output->Add(TACLine(temp, TACLine::Op::ElemBofA, val, index));
							val = temp;
						}
					}
				}
				
				RequireToken(tokens, Token::Type::RSquare);
			} else if ((val.type == ValueType::Var or val.type == ValueType::SeqElem) && !val.noInvoke) {
				// Got a variable... it might refer to a function!
				if (not asLval or (tokens.Peek().type == Token::Type::LParen && !tokens.Peek().afterSpace)) {
					// If followed by parens, definitely a function call, possibly with arguments!
					// If not, well, let's call it anyway unless we need an lvalue.
					val = ParseCallArgs(val, tokens);
				} else break;
			} else break;
		}
		
		return val;
	}

	Value Parser::ParseCallArgs(Value funcRef, Lexer tokens) {
		int argCount = 0;
		if (tokens.Peek().type == Token::Type::LParen) {
			tokens.Dequeue();		// remove '('
			if (tokens.Peek().type == Token::Type::RParen) {
				tokens.Dequeue();
			} else while (true) {
				AllowLineBreak(tokens); // allow a line break after a comma or open paren
				Value arg = ParseExpr(tokens);
				output->Add(TACLine(TACLine::Op::PushParam, arg));
				argCount++;
				if (RequireEitherToken(tokens, Token::Type::Comma, Token::Type::RParen).type == Token::Type::RParen) break;
			}
		}
		Value result = Value::Temp(output->nextTempNum++);
		output->Add(TACLine(result, TACLine::Op::CallFunctionA, funcRef, Value(argCount)));
		return result;
	}

	Value Parser::ParseSeqLookup(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseMap;
		Value val = (*this.*nextLevel)(tokens, asLval, statementStart);

		while (tokens.Peek().type == Token::Type::LSquare) {
			tokens.Dequeue();	// discard '['
			val = FullyEvaluate(val);

			if (tokens.Peek().type == Token::Type::Colon) {	// e.g., foo[:4]
				tokens.Dequeue();	// discard ':'
				Value index2 = ParseExpr(tokens);
				Value temp = Value::Temp(output->nextTempNum++);
				Intrinsics::CompileSlice(output->code, val, Value::null, index2, temp.data.tempNum);
				val = temp;
			} else {
				Value index = ParseExpr(tokens);
				if (tokens.Peek().type == Token::Type::Colon) {	// e.g., foo[2:4] or foo[2:]
					tokens.Dequeue();	// discard ':'
					Value index2 = Value::null;
					if (tokens.Peek().type != Token::Type::RSquare) index2 = ParseExpr(tokens);
					Value temp = Value::Temp(output->nextTempNum++);
					Intrinsics::CompileSlice(output->code, val, index, index2, temp.data.tempNum);
					val = temp;
				} else {			// e.g., foo[3]  (not a slice at all)
					if (statementStart) {
						// At the start of a statement, we don't want to compile the
						// last sequence lookup, because we might have to convert it into
						// an assignment.  But we want to compile any previous one.
						if (val.type == ValueType::SeqElem) {
							SeqElemStorage *vsVal = (SeqElemStorage*)val.data.ref;
							Value temp = Value::Temp(output->nextTempNum++);
							output->Add(TACLine(temp, TACLine::Op::ElemBofA, vsVal->sequence, vsVal->index));
							val = temp;
						}
						val = Value::SeqElem(val, index);
					} else {
						// Anywhere else in an expression, we can compile the lookup right away.
						Value temp = Value::Temp(output->nextTempNum++);
						output->Add(TACLine(temp, TACLine::Op::ElemBofA, val, index));
						val = temp;
					}
				}
			}

			RequireToken(tokens, Token::Type::RSquare);
		}
		return val;
	}

	Value Parser::ParseMap(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseList;
		if (tokens.Peek().type != Token::Type::LCurly) return (*this.*nextLevel)(tokens, asLval, statementStart);
		tokens.Dequeue();
		// NOTE: we must be sure this map gets created at runtime, not here at parse time.
		// Since it is a mutable object, we need to return a different one each time
		// this code executes (in a loop, function, etc.).  So, we use Op.CopyA below!
		ValueDict map;
		if (tokens.Peek().type == Token::Type::RCurly) {
			tokens.Dequeue();
		} else while (true) {
			AllowLineBreak(tokens); // allow a line break after a comma or open brace
			
			// Allow the map to close with a } on its own line.
			if (tokens.Peek().type == Token::Type::RCurly) {
				tokens.Dequeue();
				break;
			}

			Value key = ParseExpr(tokens);
			RequireToken(tokens, Token::Type::Colon);
			AllowLineBreak(tokens); // allow a line break after a colon
			Value value = ParseExpr(tokens);

			map.SetValue(key, value);

			if (RequireEitherToken(tokens, Token::Type::Comma, Token::Type::RCurly).type == Token::Type::RCurly) break;
		}
		Value result = Value::Temp(output->nextTempNum++);
		output->Add(TACLine(result, TACLine::Op::CopyA, map));
		return result;
	}

	//		list	:= '[' expr [, expr, ...] ']'
	//				 | quantity
	Value Parser::ParseList(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseQuantity;
		if (tokens.Peek().type != Token::Type::LSquare) return (*this.*nextLevel)(tokens, asLval, statementStart);
		tokens.Dequeue();
		// NOTE: we must be sure this list gets created at runtime, not here at parse time.
		// Since it is a mutable object, we need to return a different one each time
		// this code executes (in a loop, function, etc.).  So, we use Op.CopyA below!
		ValueList list;
		list.EnsureStorage();
		if (tokens.Peek().type == Token::Type::RSquare) {
			tokens.Dequeue();
		} else while (true) {
			AllowLineBreak(tokens); // allow a line break after a comma or open bracket

			// Allow the list to close with a ] on its own line.
			if (tokens.Peek().type == Token::Type::RSquare) {
				tokens.Dequeue();
				break;
			}

			Value elem = ParseExpr(tokens);
			list.Add(elem);
			if (RequireEitherToken(tokens, Token::Type::Comma, Token::Type::RSquare).type == Token::Type::RSquare) break;
		}
		if (statementStart) return list;	// return the list as-is for indexed assignment (foo[3]=42)
		Value result = Value::Temp(output->nextTempNum++);
		output->Add(TACLine(result, TACLine::Op::CopyA, list));	// use COPY on this mutable list!
		return result;
	}

	//		quantity := '(' expr ')'
	//				  | call
	Value Parser::ParseQuantity(Lexer tokens, bool asLval, bool statementStart) {
		Value (Parser::*nextLevel)(Lexer tokens, bool asLval, bool statementStart) = &Parser::ParseAtom;
		if (tokens.Peek().type != Token::Type::LParen) return (*this.*nextLevel)(tokens, asLval, statementStart);
		tokens.Dequeue();
		AllowLineBreak(tokens); // allow a line break after an open paren
		Value val = ParseExpr(tokens);
		RequireToken(tokens, Token::Type::RParen);
		return val;
	}

	Value Parser::ParseAtom(Lexer tokens, bool asLval, bool statementStart) {
		Token tok = !tokens.atEnd() ? tokens.Dequeue() : Token::EOL;
		if (tok.type == Token::Type::Number) {
			int ok = 0;
			double retval = 0;
			if (tok.text.LengthB() > 0 && tok.text[tok.text.LengthB()-1] != 'e') {
				ok = sscanf(tok.text.c_str(), "%lf", &retval);
			}
			if (ok == 1) return Value(retval);
			throw new CompilerException("invalid numeric literal: " + tok.text);
		} else if (tok.type == Token::Type::String) {
			return Value(tok.text);
		} else if (tok.type == Token::Type::Identifier) {
			return Value::Var(tok.text);
		} else if (tok.type == Token::Type::Keyword) {
			if (tok.text == "null") return Value();
			if (tok.text == "true") return Value::one;
			if (tok.text == "false") return Value::zero;
		}
		throw new CompilerException(String("got ") + tok.ToString() + " where number, string, or identifier is required");
	}

	Value Parser::FullyEvaluate(Value val) {
		// If var was protected with @, then return it as-is; don't attempt to call it.
		if (val.noInvoke) return val;
		if (val.type == ValueType::Var) {
			// Don't invoke super; leave as-is so we can do special handling
			// of it at runtime.  Also, as an optimization, same for "self".
			String identifier = val.ToString();
			if (identifier == "super" or identifier == "self") return val;
			// Evaluate a variable (which might be a function we need to call).
			Value temp = Value::Temp(output->nextTempNum++);
			output->Add(TACLine(temp, TACLine::Op::CallFunctionA, val, Value::zero));
			return temp;
		} else if (val.type == ValueType::SeqElem) {
			// Evaluate a sequence lookup (which might be a function we need to call).
			Value temp = Value::Temp(output->nextTempNum++);
			output->Add(TACLine(temp, TACLine::Op::CallFunctionA, val, Value::zero));
			return temp;
		}
		return val;
	}

	Machine *Parser::CreateVM(TextOutputMethod standardOutput) {
		Context *root = new Context();
		if (output) {
			root->code = output->code;
		}
		return new Machine(root, standardOutput);
	}
	
	/// <summary>
	/// The given token type and text is required. So, consume the next token,
	/// and if it doesn't match, throw new an error.
	/// </summary>
	/// <param name="tokens">Token queue.</param>
	/// <param name="type">Required token type.</param>
	/// <param name="text">Required token text (if applicable).</param>
	Token Parser::RequireToken(Lexer tokens, Token::Type type, String text) {
		Token got = (tokens.atEnd() ? Token::EOL : tokens.Dequeue());
		if (got.type != type or (!text.empty() and got.text != text)) {
			Token expected(type, text);
			throw new CompilerException(String("got ") + got.ToString() + " where " + expected.ToString() + " is required");
		}
		return got;
	}
	
	Token Parser::RequireEitherToken(Lexer tokens, Token::Type type1, String text1, Token::Type type2, String text2) {
		Token got = (tokens.atEnd() ? Token::EOL : tokens.Dequeue());
		if ((got.type != type1 and got.type != type2)
				or ((!text1.empty() and got.text != text1) and (!text2.empty() and got.text != text2))) {
			Token expected1(type1, text1);
			Token expected2(type2, text2);
			throw new CompilerException(String("got ") + got.ToString() + " where " + expected1.ToString() + " or " + expected2.ToString() + " is required");
		}
		return got;
	}

	
	
	class TestParser : public UnitTest
	{
	public:
		TestParser() : UnitTest("Parser") {}
		virtual void Run();
	private:
		void TestValidParse(String src, bool dumpTac=false);
	};

	void TestParser::TestValidParse(String src, bool dumpTac) {
		Parser parser;
		try {
			parser.Parse(src);
		} catch (MiniscriptException e) {
			std::cerr << e.Description().c_str() << " while parsing:" << std::endl;
			std::cerr << src.c_str() << std::endl;
		}
//		if (dumpTac && parser.output != null) TAC.Dump(parser.output.code);
	}
	
	void TestParser::Run() {
		List<ParseState> list;
		list.Add(ParseState());
		ParseState &foo = list[0];
		ErrorIf(&foo != &list[0]);
		list[0].nextTempNum = 42;
		ErrorIf(foo.nextTempNum != 42);
		ParseState &bar = list.Last();
		bar.nextTempNum = 12;
		ErrorIf(foo.nextTempNum != 12);
		ErrorIf(list[0].nextTempNum != 12);
		ErrorIf(&foo != &bar);
		
		TestValidParse("pi < 4");
		TestValidParse("(pi < 4)");
		TestValidParse("if true then 20 else 30");
		TestValidParse("f = function(x)\nreturn x*3\nend function\nf(14)");
		TestValidParse("foo=\"bar\"\nindexes(foo*2)\nfoo.indexes");
		TestValidParse("x=[]\nx.push(42)");
		TestValidParse("list1=[10, 20, 30, 40, 50]; range(0, list1.len)");
		TestValidParse("f = function(x); print(\"foo\"); end function; print(false and f)");
		TestValidParse("print 42");
		TestValidParse("print true");
		TestValidParse("f = function(x)\nprint x\nend function\nf 42");
		TestValidParse("myList = [1, null, 3]");
		TestValidParse("x = 0 or\n1");
		TestValidParse("x = [1, 2, \n 3]", true);
	}

	RegisterUnitTest(TestParser);
}
