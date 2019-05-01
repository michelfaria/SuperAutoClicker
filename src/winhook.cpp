#include "hook.h"

#include <Windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <QTextStream>

#include "mainwindow.h"
#include "autoclicker.h"
#include "winkeys.h"

#define VK_LEN 0xFF // Assume there are at most 0xFF keys

namespace sac {
namespace hook {


LRESULT __stdcall _keyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall _disableMouseButtonProc(int code, WPARAM wParam, LPARAM lParam);


kb::keycomb_t bindings[3] = {
    { VK_ADD,      false, false, false, false }, // TOGGLE_CLICK
    { VK_SUBTRACT, false, false, false, true  }, // TOGGLE_LISTEN
    { VK_DIVIDE,   false, false, false, false }, // TOGGLE_MOUSE
    // { VK_MULTIPLY, false, false, false, false }  // TOGGLE_HOLD
};
bool       shouldDisableMouseBtn = false;
mousebtn_t mouseBtnToDisable     = MOUSE1;


static HHOOK _hKbdHook;
static HHOOK _hMouseHook;


void createKbdHook() {
    assert(_hKbdHook == nullptr);
    _hKbdHook = SetWindowsHookEx(
                WH_KEYBOARD_LL, // int       idHook
                _keyboardHookProc,      // HOOKPROC  lpfn
                nullptr,        // HINSTANCE hmod
                0               // DWORD     dwThreadId
              );
    if (_hKbdHook == nullptr) {
        throw std::runtime_error("Could not create keyboard hook");
    }
}


void releaseKbdHook() {
    assert(_hKbdHook != nullptr);
    BOOL ret = UnhookWindowsHookEx(_hKbdHook);
    if (!ret) {
        throw std::runtime_error("Could not release hook");
    }
}


void setBind(action_t actionkey, kb::keycomb_t keycomb) {
    bindings[actionkey] = keycomb;
}


QString getLastError() {
    DWORD err = GetLastError();
    if (err == 0) { return nullptr; }
    else {
        wchar_t* buf;
        FormatMessageA(
                       FORMAT_MESSAGE_ALLOCATE_BUFFER             // DWORD    dFlags
                            | FORMAT_MESSAGE_FROM_SYSTEM          //
                            | FORMAT_MESSAGE_IGNORE_INSERTS,      //
                       nullptr,                                   // LPCVOID  source
                       err,                                       // DWORD    dwMessageId
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // DWORD    dwLanguageId
                       reinterpret_cast<LPSTR>(&buf),             // LPSTR    lpBuffer
                       0,                                         // DWORD    nSize
                       nullptr                                    // va_list *Arguments
                   );
        QString str = QString::fromWCharArray(buf);
        LocalFree(buf);
        return str;
    }
}


kb::keycomb_t getKeyCombinationFor(action_t action) {
    return bindings[action];
}


void createMouseHook() {
    assert(_hMouseHook == nullptr);
    _hMouseHook = SetWindowsHookEx(
                WH_MOUSE_LL,             // int       idHook
                _disableMouseButtonProc, // HOOKPROC  lpfn
                nullptr,                 // HINSTANCE hmod
                0                        // DWORD     dwThreadId
             );
    if (_hMouseHook == nullptr) {
        throw std::runtime_error("Failed to create mouse hook");
    }
}


void releaseMouseHook() {
    assert(_hMouseHook != nullptr);
    BOOL ret = UnhookWindowsHookEx(_hMouseHook);
    if (!ret) {
        throw std::runtime_error("Failed to release mouse hook");
    }
    _hMouseHook = nullptr;
}


LRESULT __stdcall _keyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    static KBDLLHOOKSTRUCT kbdStruct;           // Stores the keyboard details
    static BOOL keysDown[VK_LEN];               // Stores the press-state of virtual keys

    kbdStruct = *(reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam));  // Store updated kbdStruct

    const DWORD vkCode = kbdStruct.vkCode;      // Virtual key code
    assert(vkCode < VK_LEN);                    // Ensure it is within range

    if (code >= 0) {
        if (wParam == WM_KEYDOWN && !keysDown[vkCode]) {
            keysDown[vkCode] = TRUE; // Mark key as down

            qDebug("Key pressed: %s", qUtf8Printable(
                 kb::keycombstr({vkCode, false, false, false, false})
                )
              );

            AutoClicker* _ac = autoClicker();

            if      (vkCode == bindings[TOGGLE_LISTEN].vkCode) {
                _ac->toggleListenMode();
            }
            else if (vkCode == bindings[TOGGLE_CLICK ].vkCode) {
                _ac->toggleClickMode();
            }
            else if (vkCode == bindings[TOGGLE_MOUSE ].vkCode) {
                _ac->toggleMouseButton();
            }
            // else if (vkCode == bindings[TOGGLE_HOLD  ].vkCode) {
            //    _ac->toggleHoldButtonMode();
            // }
            else {
                bool isNumpadPress    = (vkCode >= VK_NUMPAD0 && vkCode <= VK_NUMPAD9);
                bool isNumberRowPress = (vkCode >= VK_KEY_0   && vkCode <= VK_KEY_9);

                if (isNumpadPress || isNumberRowPress) {
                    uint number = 10UL;
                    if (isNumpadPress) {
                        number = vkCode - VK_NUMPAD0;
                    }
                    else {
                        assert(isNumberRowPress);
                        number = vkCode - VK_KEY_0;
                    }
                    assert( (number == 0 || number > 0) && number <= 9); // >= expansion prevents -Wtype-limits
                    _ac->typeNumber(number);
                }
            }
        }
        else if (wParam == WM_KEYUP) {
            keysDown[vkCode] = FALSE; // Mark key as up
        }
    }
    return CallNextHookEx(_hKbdHook, code, wParam, lParam);
}


LRESULT __stdcall _disableMouseButtonProc(int code, WPARAM wParam, LPARAM lParam) {
    if (shouldDisableMouseBtn) {
        if (code >= 0) {
            switch (wParam) {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
                if (mouseBtnToDisable == MOUSE1) return -1;
                break;
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
                if (mouseBtnToDisable == MOUSE2) return -1;
                break;
            }
        }
    }
    return CallNextHookEx(_hMouseHook, code, wParam, lParam);
}


} // namespace hook
} // namespace sac
