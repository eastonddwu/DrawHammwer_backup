/**
 *    Copyright (C) 2014 MongoDB Inc.
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

#if defined WIN32 || WIN64
#include <memory>
#else
#include <tr1/memory>
#endif

namespace tcaplus {
    namespace doc {

    class SharedBuffer {
    public:
        SharedBuffer() {}

        char* get() const {
            return m_buf.get();
        }

        operator bool() { return (bool)m_buf; }

        template <typename T>
        explicit SharedBuffer(char* buffer, const T& deleter) : m_buf(buffer, deleter) {}

    private:
        std::tr1::shared_ptr<char> m_buf;
    };
} // namespace doc
} // namespace tcaplus
