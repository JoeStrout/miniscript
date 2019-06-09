/*
 *  SplitJoin.h
 *
 *  Created by Joe Strout on 12/21/10.
 *  Copyright 2010 Luminary Apps. All rights reserved.
 *
 */

#ifndef SPLITJOIN_H
#define SPLITJOIN_H

#include "List.h"
#include "String.h"

namespace MiniScript {

	typedef List<String> StringList;

	StringList Split(const String& s, const String& delimiter=" ", int maxSplits=-1);
	StringList Split(const String& s, char delimiter, int maxSplits=-1);

	String Join(const String& delimiter, const StringList& parts);
	String Join(char delimiter, const String& parts);

}

#endif
