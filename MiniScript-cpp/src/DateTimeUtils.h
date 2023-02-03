//
//  DateTimeUtils.hpp
//  MiniScript
//
//  Created by Joe Strout on 2/1/23.
//  Copyright © 2023 Joe Strout. All rights reserved.
//

#ifndef DateTimeUtils_hpp
#define DateTimeUtils_hpp

#include <stdio.h>
#include <time.h>
#include "SimpleString.h"

namespace MiniScript {

String FormatDate(time_t dateTime, const String formatSpec);

inline String FormatDate(time_t dateTime) {
	return FormatDate(dateTime, "yyyy-MM-dd HH:mm:ss");
}

time_t ParseDate(const String dateStr);

}

#endif /* DateTimeUtils_hpp */
