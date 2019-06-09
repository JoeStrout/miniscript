
#include "OstreamSupport.h"

using namespace MiniScript;

ostream & operator<< (ostream &out, const String &rhs){
	out << rhs.c_str();
	return out;
}

ostream & operator<< (ostream &out, const List<int> &rhs) {
	out << '[';
	for (int i=0; i<rhs.Count(); i++) {
		if (i > 0) out << ", ";
		out << rhs[i];
	}
	out << ']';
	return out;
}

ostream & operator<< (ostream &out, const List<String> &rhs) {
	out << '[';
	for (int i=0; i<rhs.Count(); i++) {
		if (i > 0) out << ", ";
		out << rhs[i];
	}
	out << ']';
	return out;
}

