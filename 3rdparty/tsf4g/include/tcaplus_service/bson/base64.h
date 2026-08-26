// util/base64.h

/*    Copyright 2009 10gen Inc.
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

#pragma once

//#include <memory>
#if defined WIN32 || WIN64
#include <memory>
#else
#include <tr1/memory>
#endif
#include <cstring>
#include <iosfwd>
#include <string>
#include <vector>

namespace tcaplus {
namespace doc {
    namespace base64 {

        class Alphabet {
        public:
            Alphabet()
                : encode((unsigned char*)
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz"
                         "0123456789"
                         "+/")
                , decode(new std::vector<unsigned char>(257, 0)) {
                for ( int i=0; i<64; i++ ) {
                    (*decode)[ encode[i] ] = i;
                }

                test();
            }

            void test();

            char e( int x ) {
                return encode[x&0x3f];
            }

        private:
            const unsigned char * encode;
        public:
            std::tr1::shared_ptr< std::vector<unsigned char> > decode;
        };

        extern Alphabet alphabet;


        void encode( std::stringstream& ss , const char * data , int size );
        std::string encode( const char * data , int size );
        std::string encode( const std::string& s );

        void decode( std::stringstream& ss , const std::string& s );
        std::string decode( const std::string& s );

        extern const char* chars;

        void testAlphabet();
    }
} // namespace doc
} // namespace tcaplus
