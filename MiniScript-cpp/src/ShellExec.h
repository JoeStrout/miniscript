//
//  ShellExec.hpp
//  MiniScript
//
//  Created by Joe Strout on 2/9/24.
//  Copyright © 2024 Joe Strout. All rights reserved.
//

#ifndef SHELLEXEC_H
#define SHELLEXEC_H

#include <stdio.h>
#include "SimpleString.h"
#include "MiniscriptTypes.h"

namespace MiniScript {

// Fork a subprocess to execute the given command.  Return a ValueList
// of whatever data we need to continue.
// Return true on success, false on failure.
bool BeginExec(String cmd, double timeout, double currentTime, ValueList* outResult);

// Check the subprocess to see if it's done.  If so, stuff results
// into output parameters and return true.  If not, return false.
bool FinishExec(ValueList data, double currentTime, String* outStdout, String* outStderr, int* outStatus);



}

#endif /* SHELLEXEC_H */
