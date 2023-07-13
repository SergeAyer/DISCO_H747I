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
 * @file joystick.hpp
 * @author Serge Ayer <serge.ayer@hefr.ch>
 *
 * @brief Joystick interface (STM32)
 *
 * @date 2023-07-10
 * @version 0.0.1
 ***************************************************************************/

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mbed.h"
#include "return_code.hpp"

namespace disco {

class Joystick {
   public:
    Joystick& getInstance();

    // prevent copy and assignment
    Joystick(const Joystick&)            = delete;
    Joystick& operator=(const Joystick&) = delete;

    // public methods
    ReturnCode init();
    enum class State {
        UpPressed,
        DownPressed,
        LeftPressed,
        RightPressed,
        SelPressed,
        NonePressed
    };
    State getState();
    void setCallback(mbed::Callback<void(State)> callback);

   private:
    // private methods
    Joystick() = default;

    // constants
    static constexpr PinName kJoyUpPin    = PK_6;
    static constexpr PinName kJoyDownPin  = PK_3;
    static constexpr PinName kJoyLeftPin  = PK_4;
    static constexpr PinName kJoyRightPin = PK_5;
    static constexpr PinName kJoySelPin   = PK_2;

    // data members
    static bool _isInitialized;
};

}  // namespace disco
