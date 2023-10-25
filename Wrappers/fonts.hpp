// Copyright 2022 Haute école d'ingénierie et d'architecture de Fribourg
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/****************************************************************************
 * @file fonts.hpp
 * @author Serge Ayer <serge.ayer@hefr.ch>
 *
 * @brief Fonts interface (STM32)
 *
 * @date 2023-07-10
 * @version 0.0.1
 ***************************************************************************/

#pragma once

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>

namespace disco {

struct Font {
    // cppcheck-suppress unusedStructMember
    const uint8_t* table;
    // cppcheck-suppress unusedStructMember
    uint16_t width;
    // cppcheck-suppress unusedStructMember
    uint16_t height;
};

extern Font* createFont18();
extern Font* createFont24();
extern Font* createFont26b();
extern Font* createFont36();
extern Font* createFont36b();

}  // namespace disco
