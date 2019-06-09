//
//  MiniscriptKeywords.h
//  MiniScript
//
//  Created by Joe Strout on 5/30/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#ifndef MINISCRIPTKEYWORDS_H
#define MINISCRIPTKEYWORDS_H

#include "String.h"

namespace MiniScript {

	class Keywords {
	public:
		static const String all[];
		
		static const int count;
		
		static bool IsKeyword(String text) {
			for (int i=0; i<count; i++) if (all[i] == text) return true;
			return false;
		}
	};
		
}

#endif /* MINISCRIPTKEYWORDS_H */
