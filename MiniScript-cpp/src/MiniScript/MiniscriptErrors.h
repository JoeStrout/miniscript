//
//  MiniscriptErrors.hpp
//  MiniScript
//
//  Created by Joe Strout on 5/30/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MINISCRIPTERRORS_H
#define MINISCRIPTERRORS_H

#include <exception>
#include "SimpleString.h"
#include "MiniscriptTypes.h"

namespace MiniScript {

	class SourceLoc {
	public:
		String context;		// file name, etc. (optional)
		int lineNum;
		
		SourceLoc() : lineNum(0) {}
		SourceLoc(String context, int lineNum) : context(context), lineNum(lineNum) {}
		
		String ToString() const {
			if (context.empty()) return String("[line ") + String::Format(lineNum) + "]";
		   return String("[") + context + " line " + String::Format(lineNum) + "]";
		}
		
		bool IsEmpty() const {
			return lineNum == 0 && context.empty();
		}
	};
	
	class MiniscriptException : public std::exception {
	public:
		SourceLoc location;
		String message;
		
		MiniscriptException(String msg) : message(msg) {}
		MiniscriptException(String context, int lineNum, String msg) : location(SourceLoc(context, lineNum)), message(msg) {}

		virtual void raise() { throw *this; }
		
		virtual const char* what() const throw() {
			return message.c_str();
		}
		
		virtual String Type() const { return "Error"; }
		
		/// <summary>
		/// Get a standard description of this error, including type and location.
		/// </summary>
		String Description() const {
			String desc = this->Type() + ": " + message;
			if (location.lineNum > 0) desc += String(" ") + location.ToString();
			return desc;
		}
	};
	
	class LexerException : public MiniscriptException {
	public:
		LexerException(String msg="Lexer Error") : MiniscriptException(msg) {}		
		LexerException(String context, int lineNum, String msg) : MiniscriptException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
		virtual String Type() const { return "Lexer Error"; }
	};

	class CompilerException : public MiniscriptException {
	public:
		CompilerException(String msg="Compiler Error") : MiniscriptException(msg) {}
		CompilerException(String context, int lineNum, String msg) : MiniscriptException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
		virtual String Type() const { return "Compiler Error"; }
	};
	
	class RuntimeException : public MiniscriptException {
	public:
		RuntimeException(String msg="Runtime Error") : MiniscriptException(msg) {}
		RuntimeException(String context, int lineNum, String msg) : MiniscriptException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
		virtual String Type() const { return "Runtime Error"; }
	};
	
	class IndexException : public RuntimeException {
	public:
		IndexException(String msg="Index Error") : RuntimeException(msg) {}
		IndexException(String context, int lineNum, String msg) : RuntimeException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
	};

	class TypeException : public RuntimeException {
	public:
		TypeException(String msg="Type Error") : RuntimeException(msg) {}
		TypeException(String context, int lineNum, String msg) : RuntimeException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
	};
	
	class KeyException : public RuntimeException {
	public:
		KeyException(String msg="Key Error") : RuntimeException(msg) {}
		KeyException(String context, int lineNum, String msg) : RuntimeException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
	};
	
	class UndefinedIdentifierException : public RuntimeException {
	public:
		UndefinedIdentifierException(String ident) : RuntimeException(
			 String("Undefined Identifier: '") + ident + "' is unknown in this context") {}
		virtual void raise() { throw *this; }
	};

	class UndefinedLocalException : public UndefinedIdentifierException {
	public:
		UndefinedLocalException(String ident) : UndefinedIdentifierException("") {
			message = String("Undefined Local Identifier: '") + ident + "' is unknown in this context";
		}
		virtual void raise() { throw *this; }
	};

	class TooManyArgumentsException : public RuntimeException {
	public:
		TooManyArgumentsException(String msg="Too Many Arguments") : RuntimeException(msg) {}
		TooManyArgumentsException(String context, int lineNum, String msg="Too Many Arguments") : RuntimeException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
	};

	class LimitExceededException : public RuntimeException {
	public:
		LimitExceededException(String msg="Runtime Limit Exceeded") : RuntimeException(msg) {}
		LimitExceededException(String context, int lineNum, String msg="Runtime Limit Exceeded") : RuntimeException(context, lineNum, msg) {}
		virtual void raise() { throw *this; }
	};
	
	static inline void CheckType(Value val, ValueType requiredType, String desc) {
		if (val.type != requiredType) {
			TypeException(desc + ": got a " + ToString(val.type) + " where a " + ToString(requiredType) + " was required").raise();
		}
	}
	
	static inline void CheckType(Value val, ValueType requiredType) {
		if (val.type != requiredType) {
			TypeException(String("got a ") + ToString(val.type) + " where a " + ToString(requiredType) + " was required").raise();
		}
	}

	static inline void CheckRange(long i, long min, long max, String desc) {
		if (i < min or i > max) {
			IndexException(String("Index Error: ") + desc + " (" + String::Format(i)
				+ " out of range (" + String::Format(min) + " to " + String::Format(max) + "))").raise();
		}
	}

	static inline void CheckRange(long i, long min, long max) {
		if (i < min or i > max) {
			IndexException(String("Index Error: index (") + String::Format(i)
				+ " out of range (" + String::Format(min) + " to " + String::Format(max) + "))").raise();
		}
	}
}

#endif /* MINISCRIPTERRORS_H */
