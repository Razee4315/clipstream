#include "platform/PasteSimulator.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace platform {

void simulatePaste() {
    INPUT inputs[4] = {};

    inputs[0].type = INPUT_KEYBOARD;            // Ctrl down
    inputs[0].ki.wVk = VK_CONTROL;

    inputs[1].type = INPUT_KEYBOARD;            // V down
    inputs[1].ki.wVk = 'V';

    inputs[2].type = INPUT_KEYBOARD;            // V up
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    inputs[3].type = INPUT_KEYBOARD;            // Ctrl up
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    ::SendInput(4, inputs, sizeof(INPUT));
}

} // namespace platform
