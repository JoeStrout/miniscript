//
//  MSQA.hpp
//  MiniScript
//
//  Created by Joe Strout on 3/10/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MSQA_H
#define MSQA_H

#include <ciso646>  // (force non-conforming compilers to join the 21st century)

namespace MiniScript {
	
	extern void _Error(const char *msg, const char *filename, const int linenum);
	inline void _ErrorIf(int condition, const char *msg, const char *filename, int linenum)
		{ if (condition) _Error(msg, filename, linenum); }

}

#define Error(msg) MiniScript::_Error(msg, __FILE__, __LINE__)
#define ErrorIf(condition) MiniScript::_ErrorIf(condition, "Error: " # condition, __FILE__, __LINE__)
#ifdef Assert
#undef Assert
#endif
#define Assert(condition) MiniScript::_ErrorIf(!(condition), "Assert failed: " # condition, __FILE__, __LINE__)

#endif // MSQA_H
