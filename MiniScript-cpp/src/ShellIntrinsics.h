//
//  ShellIntrinsics.hpp
//  MiniScript
//
//  Created by Joe Strout on 3/9/19.
//  Copyright © 2019 Joe Strout. All rights reserved.
//

#ifndef SHELLINTRINSICS_H
#define SHELLINTRINSICS_H

#include "MiniscriptTypes.h"

extern bool exitASAP;
extern int exitResult;

extern MiniScript::ValueList shellArgs;

void AddShellIntrinsics();

#endif // SHELLINTRINSICS_H
