//
//  ShellIntrinsics.hpp
//  MiniScript
//
//  Created by Joe Strout on 3/9/19.
//  Copyright © 2019 Joe Strout. All rights reserved.
//

#ifndef SHELLINTRINSICS_H
#define SHELLINTRINSICS_H

#include "MiniScript/MiniscriptTypes.h"
#if _WIN32 || _WIN64
	#define useEditline 0
#else
	#include "editline.h"
	#define useEditline 1
#endif

extern bool exitASAP;
extern int exitResult;

extern MiniScript::ValueList shellArgs;

void AddShellIntrinsics();

#endif // SHELLINTRINSICS_H
