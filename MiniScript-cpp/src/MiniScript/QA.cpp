//
//  MSQA.cpp
//  MiniScript
//
//  Created by Joe Strout on 3/10/18.
//  Copyright © 2018 Joe Strout. All rights reserved.
//

#include "QA.h"
#include <stdio.h>

namespace MiniScript {
	// _Error
	//
	//	Report an error.  This should be used only for conditions that indicate
	//	actual programming bugs, i.e. conditions that should never occur.
	//
	// Author: JJS
	// Used in: Error, ErrorIf, and Assert macros
	// Gets: msg -- error message
	//		 filename -- name of source file where error occurred
	//		 linenum -- line number where error occurred
	// Returns: <nothing>
	void _Error(const char *msg, const char *filename, int linenum)
	{
		#if WINDOWS
			OutputDebugString(String(msg) + " at " + filename + ":" + ultoa(linenum));
		#else
			fprintf( stderr, "%s at %s:%d\n", msg, filename, linenum);
			fflush( stderr );
		#endif
	}

}
