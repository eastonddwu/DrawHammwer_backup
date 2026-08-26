/*
 *    Copyright 2009 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#include <cmath>

#pragma once

const float EPSINON = 0.00001; // 主要是用于double变量与0的比较

namespace tcaplus {
namespace doc {

    inline bool isNaN(double d) { 
		return (std::abs(d-d) <= 1e-6);
        //return d != d;
    }

    inline bool isInf(double d, int* sign = 0) {
        volatile double tmp = d;

		bool is_zero = (tmp - d >= -EPSINON && tmp - d <= EPSINON );
        //if ((tmp == d) && ((tmp - d) != 0.0)) {
		if(!is_zero)
		{
            if ( sign ) {
                *sign = (d < 0.0 ? -1 : 1);
            }
            return true;
        }
        
        if ( sign ) {
            *sign = 0;
        }

        return false;
    }

} // namespace doc
} // namespace tcaplus
