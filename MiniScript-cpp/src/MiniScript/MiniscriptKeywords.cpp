//
//  MiniscriptKeywords.cpp
//  MiniScript
//
//  Created by Joe Strout on 5/30/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "MiniscriptKeywords.h"

namespace MiniScript {
	
	const String Keywords::all[] = {
		"break",
		"continue",
		"else",
		"end",
		"for",
		"function",
		"if",
		"in",
		"isa",
		"new",
		"null",
		"then",
		"repeat",
		"return",
		"while",
		"and",
		"or",
		"not",
		"true",
		"false"
	};
	
	const int Keywords::count = sizeof(all) / sizeof(String);
	
}
