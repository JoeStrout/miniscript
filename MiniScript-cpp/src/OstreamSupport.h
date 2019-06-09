//  OstreamSupport.h
//
//	Basic support for outputting a MiniScript string or list to a std::ostream.
//	Not actually required by MiniScript, but used by some of the test code.

#ifndef OSTREAMSUPPORT_H
#define OSTREAMSUPPORT_H

#include "MiniScript/String.h"
#include "MiniScript/List.h"

#include <iostream>
using std::ostream;

ostream & operator<< (ostream &out, const MiniScript::String &rhs);
ostream & operator<< (ostream &out, const MiniScript::List<int> &rhs);
ostream & operator<< (ostream &out, const MiniScript::List<MiniScript::String> &rhs);


#endif  // OSTREAMSUPPORT_H
