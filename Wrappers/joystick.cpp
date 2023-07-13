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
 * @file joystick.cpp
 * @author Serge Ayer <serge.ayer@hefr.ch>
 *
 * @brief Joystick interface (STM32)
 *
 * @date 2023-07-10
 * @version 0.0.1
 ***************************************************************************/

#include "joystick.hpp"

// from DISCO_H747I/Drivers/BSP/STM32H747I-DISCO
#include "mbed_trace.h"
#include "stm32h747i_discovery.h"
#if MBED_CONF_MBED_TRACE_ENABLE
#define TRACE_GROUP "Joystick"
#endif  // MBED_CONF_MBED_TRACE_ENABLE

namespace disco {

bool Joystick::_isInitialized = false;

Joystick& Joystick::getInstance() {
    static Joystick instance;
    if (!_isInitialized) {
        // initialize the joystick
        int32_t rc = BSP_JOY_Init(JOY1, JOY_MODE_GPIO, JOY_ALL);
        // BSP_JOY_INIT always returns BSP_ERROR_NONE
        MBED_ASSERT(rc == BSP_ERROR_NONE);
        tr_debug("Joystick initialized");
        _isInitialized = true;
    }
    return instance;
}

Joystick::Joystick()
    : _joyUpPin(kJoyUpPin),
      _joyDownPin(kJoyDownPin),
      _joyLeftPin(kJoyLeftPin),
      _joyRightPin(kJoyRightPin),
      _joySelPin(kJoySelPin) {}

Joystick::State Joystick::getState() {
    int32_t joystickState = BSP_JOY_GetState(JOY1, 0);
    tr_debug("joystick state is %d", joystickState);
    State state = State::NonePressed;
    switch (joystickState) {
        case JOY_SEL:
            state = State::SelPressed;
            break;
        case JOY_DOWN:
            state = State::DownPressed;
            break;
        case JOY_LEFT:
            state = State::LeftPressed;
            break;
        case JOY_RIGHT:
            state = State::RightPressed;
            break;
        case JOY_UP:
            state = State::UpPressed;
            break;
        default:
            tr_error("case not handled");
    }
    return state;
}

void Joystick::setCallback(JoystickCallback clientCallback) {
    _joyUpPin.fall(callback(this, &Joystick::onJoyUp));
    _joyDownPin.fall(callback(this, &Joystick::onJoyDown));
    _joyLeftPin.fall(callback(this, &Joystick::onJoyLeft));
    _joyRightPin.fall(callback(this, &Joystick::onJoyRight));
    _joySelPin.fall(callback(this, &Joystick::onJoySel));

    // register the callback
    _clientCallback = clientCallback;
    tr_debug("Callback set");
}

void Joystick::onJoyUp() {
    if (_clientCallback != nullptr) {
        _clientCallback(Event::UpPressed);
    }
}

void Joystick::onJoyDown() {
    if (_clientCallback != nullptr) {
        _clientCallback(Event::DownPressed);
    }
}

void Joystick::onJoyLeft() {
    if (_clientCallback != nullptr) {
        _clientCallback(Event::LeftPressed);
    }
}

void Joystick::onJoyRight() {
    if (_clientCallback != nullptr) {
        _clientCallback(Event::RightPressed);
    }
}

void Joystick::onJoySel() {
    if (_clientCallback != nullptr) {
        _clientCallback(Event::SelPressed);
    }
}

}  // namespace disco
