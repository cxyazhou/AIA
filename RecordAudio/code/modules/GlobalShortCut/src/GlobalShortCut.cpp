#if defined(WIN32) || defined(WIN64)
#define _CRT_SECURE_NO_WARNINGS
#include "GlobalShortCut.h"
#include <windows.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    unsigned int keycode;
    unsigned int modifiers;
    HotKeyCallback pressCallback;
    HotKeyCallback releaseCallback;
    int pressed;
} HotKeyItem;

static HotKeyItem* g_hotKeys = NULL;
static int g_hotKeyCount = 0;
static HHOOK g_keyboardHook = NULL;

static unsigned int strToKeycode(const char* str)
{
    if (strcmp(str, "Ctrl") == 0) return VK_CONTROL;
    if (strcmp(str, "Alt") == 0) return VK_MENU;
    if (strcmp(str, "Shift") == 0) return VK_SHIFT;
    if (strcmp(str, "Win") == 0) return VK_LWIN;
    if (strcmp(str, "Space") == 0) return VK_SPACE;
    if (strcmp(str, "Enter") == 0) return VK_RETURN;
    if (strcmp(str, "Esc") == 0) return VK_ESCAPE;
    if (strcmp(str, "Tab") == 0) return VK_TAB;
    if (strcmp(str, "Backspace") == 0) return VK_BACK;
    if (strcmp(str, "Delete") == 0) return VK_DELETE;
    if (strcmp(str, "Insert") == 0) return VK_INSERT;
    if (strcmp(str, "Home") == 0) return VK_HOME;
    if (strcmp(str, "End") == 0) return VK_END;
    if (strcmp(str, "PageUp") == 0) return VK_PRIOR;
    if (strcmp(str, "PageDown") == 0) return VK_NEXT;
    if (strcmp(str, "Left") == 0) return VK_LEFT;
    if (strcmp(str, "Up") == 0) return VK_UP;
    if (strcmp(str, "Right") == 0) return VK_RIGHT;
    if (strcmp(str, "Down") == 0) return VK_DOWN;
    
    if (strncmp(str, "F", 1) == 0 && strlen(str) <= 3) {
        int fkey = atoi(str + 1);
        if (fkey >= 1 && fkey <= 24) return VK_F1 + fkey - 1;
    }
    
    if (strlen(str) == 1) {
        char c = str[0];
        if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
        if (c >= 'A' && c <= 'Z') return c;
        if (c >= '0' && c <= '9') return c;
    }
    
    return 0;
}

static int parseHotKey(const char* key, unsigned int* outKeycode, unsigned int* outModifiers)
{
    *outKeycode = 0;
    *outModifiers = 0;
    
    char* buf = _strdup(key);
    char* token = strtok(buf, "+");
    while (token != NULL) {
        unsigned int code = strToKeycode(token);
        if (code == VK_CONTROL) *outModifiers |= MOD_CONTROL;
        else if (code == VK_MENU) *outModifiers |= MOD_ALT;
        else if (code == VK_SHIFT) *outModifiers |= MOD_SHIFT;
        else if (code == VK_LWIN || code == VK_RWIN) *outModifiers |= MOD_WIN;
        else *outKeycode = code;
        token = strtok(NULL, "+");
    }
    free(buf);
    
    return (*outKeycode != 0);
}

static unsigned int getCurrentModifiers()
{
    unsigned int modifiers = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) modifiers |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= MOD_SHIFT;
    if (GetKeyState(VK_LWIN) & 0x8000 || GetKeyState(VK_RWIN) & 0x8000) modifiers |= MOD_WIN;
    return modifiers;
}

static int matchHotKey(unsigned int keycode, unsigned int modifiers)
{
    for (int i = 0; i < g_hotKeyCount; i++) {
        if (g_hotKeys[i].keycode == keycode && g_hotKeys[i].modifiers == modifiers) {
            return i;
        }
    }
    return -1;
}

static LRESULT CALLBACK KeyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        unsigned int keycode = pKeyBoard->vkCode;
        unsigned int modifiers = getCurrentModifiers();
        
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            int idx = matchHotKey(keycode, modifiers);
            if (idx >= 0 && !g_hotKeys[idx].pressed) {
                g_hotKeys[idx].pressed = 1;
                if (g_hotKeys[idx].pressCallback) {
                    g_hotKeys[idx].pressCallback();
                }
            }
        }
        else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            int idx = matchHotKey(keycode, modifiers);
            if (idx >= 0 && g_hotKeys[idx].pressed) {
                g_hotKeys[idx].pressed = 0;
                if (g_hotKeys[idx].releaseCallback) {
                    g_hotKeys[idx].releaseCallback();
                }
                return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
            }
            
            for (int i = 0; i < g_hotKeyCount; i++) {
                if (g_hotKeys[i].pressed && g_hotKeys[i].keycode == keycode) {
                    g_hotKeys[i].pressed = 0;
                    if (g_hotKeys[i].releaseCallback) {
                        g_hotKeys[i].releaseCallback();
                    }
                    break;
                }
            }
        }
    }
    
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

int RegisterHotKeyCallback(const char* key, HotKeyCallback pressCallback, HotKeyCallback releaseCallback)
{
    unsigned int keycode, modifiers;
    if (!parseHotKey(key, &keycode, &modifiers)) {
        return -1;
    }
    
    for (int i = 0; i < g_hotKeyCount; i++) {
        if (g_hotKeys[i].keycode == keycode && g_hotKeys[i].modifiers == modifiers) {
            g_hotKeys[i].pressCallback = pressCallback;
            g_hotKeys[i].releaseCallback = releaseCallback;
            return 0;
        }
    }
    
    HotKeyItem* newHotKeys = (HotKeyItem*)realloc(g_hotKeys, (g_hotKeyCount + 1) * sizeof(HotKeyItem));
    if (!newHotKeys) return -1;
    
    g_hotKeys = newHotKeys;
    g_hotKeys[g_hotKeyCount].keycode = keycode;
    g_hotKeys[g_hotKeyCount].modifiers = modifiers;
    g_hotKeys[g_hotKeyCount].pressCallback = pressCallback;
    g_hotKeys[g_hotKeyCount].releaseCallback = releaseCallback;
    g_hotKeys[g_hotKeyCount].pressed = 0;
    g_hotKeyCount++;
    
    if (g_keyboardHook == NULL) {
        g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookProc, GetModuleHandle(NULL), 0);
        if (!g_keyboardHook) return -1;
    }
    
    return 0;
}

void UnregisterAllHotKeys()
{
    if (g_keyboardHook != NULL) {
        UnhookWindowsHookEx(g_keyboardHook);
        g_keyboardHook = NULL;
    }
    
    if (g_hotKeys != NULL) {
        free(g_hotKeys);
        g_hotKeys = NULL;
    }
    g_hotKeyCount = 0;
}

#else
#include <linux/input.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct {
    unsigned int keycode;
    unsigned int modifiers;
    HotKeyCallback pressCallback;
    HotKeyCallback releaseCallback;
    int pressed;
} HotKeyItem;

static HotKeyItem* g_hotKeys = NULL;
static int g_hotKeyCount = 0;
static int g_kbFd = -1;
static pthread_t g_threadId;
static int g_running = 0;

static unsigned int strToKeycode(const char* str)
{
    if (strcmp(str, "Ctrl") == 0) return KEY_LEFTCTRL;
    if (strcmp(str, "Alt") == 0) return KEY_LEFTALT;
    if (strcmp(str, "Shift") == 0) return KEY_LEFTSHIFT;
    if (strcmp(str, "Win") == 0) return KEY_LEFTMETA;
    if (strcmp(str, "Space") == 0) return KEY_SPACE;
    if (strcmp(str, "Enter") == 0) return KEY_ENTER;
    if (strcmp(str, "Esc") == 0) return KEY_ESC;
    if (strcmp(str, "Tab") == 0) return KEY_TAB;
    if (strcmp(str, "Backspace") == 0) return KEY_BACKSPACE;
    if (strcmp(str, "Delete") == 0) return KEY_DELETE;
    if (strcmp(str, "Insert") == 0) return KEY_INSERT;
    if (strcmp(str, "Home") == 0) return KEY_HOME;
    if (strcmp(str, "End") == 0) return KEY_END;
    if (strcmp(str, "PageUp") == 0) return KEY_PAGEUP;
    if (strcmp(str, "PageDown") == 0) return KEY_PAGEDOWN;
    if (strcmp(str, "Left") == 0) return KEY_LEFT;
    if (strcmp(str, "Up") == 0) return KEY_UP;
    if (strcmp(str, "Right") == 0) return KEY_RIGHT;
    if (strcmp(str, "Down") == 0) return KEY_DOWN;
    
    if (strncmp(str, "F", 1) == 0 && strlen(str) <= 3) {
        int fkey = atoi(str + 1);
        if (fkey >= 1 && fkey <= 24) return KEY_F1 + fkey - 1;
    }
    
    if (strlen(str) == 1) {
        char c = str[0];
        if (c >= 'a' && c <= 'z') return KEY_A + c - 'a';
        if (c >= 'A' && c <= 'Z') return KEY_A + c - 'A';
        if (c >= '0' && c <= '9') return KEY_0 + c - '0';
    }
    
    return 0;
}

static unsigned int linuxKeyToModifiers(unsigned int code)
{
    switch (code) {
        case KEY_LEFTCTRL:
        case KEY_RIGHTCTRL: return 1;
        case KEY_LEFTALT:
        case KEY_RIGHTALT: return 2;
        case KEY_LEFTSHIFT:
        case KEY_RIGHTSHIFT: return 4;
        case KEY_LEFTMETA:
        case KEY_RIGHTMETA: return 8;
        default: return 0;
    }
}

static int parseHotKey(const char* key, unsigned int* outKeycode, unsigned int* outModifiers)
{
    *outKeycode = 0;
    *outModifiers = 0;
    
    char* buf = _strdup(key);
    char* token = strtok(buf, "+");
    while (token != NULL) {
        unsigned int code = strToKeycode(token);
        unsigned int mod = linuxKeyToModifiers(code);
        if (mod != 0) *outModifiers |= mod;
        else *outKeycode = code;
        token = strtok(NULL, "+");
    }
    free(buf);
    
    return (*outKeycode != 0);
}

static int matchHotKey(unsigned int keycode, unsigned int modifiers)
{
    for (int i = 0; i < g_hotKeyCount; i++) {
        if (g_hotKeys[i].keycode == keycode && g_hotKeys[i].modifiers == modifiers) {
            return i;
        }
    }
    return -1;
}

static void* keyboardThread(void* arg)
{
    struct input_event event;
    unsigned int currentModifiers = 0;
    
    while (g_running && g_kbFd >= 0) {
        if (read(g_kbFd, &event, sizeof(event)) == sizeof(event)) {
            if (event.type == EV_KEY) {
                unsigned int mod = linuxKeyToModifiers(event.code);
                if (event.value == 1) {
                    if (mod != 0) {
                        currentModifiers |= mod;
                    } else {
                        int idx = matchHotKey(event.code, currentModifiers);
                        if (idx >= 0 && !g_hotKeys[idx].pressed) {
                            g_hotKeys[idx].pressed = 1;
                            if (g_hotKeys[idx].pressCallback) {
                                g_hotKeys[idx].pressCallback();
                            }
                        }
                    }
                } else if (event.value == 0) {
                    if (mod != 0) {
                        currentModifiers &= ~mod;
                    } else {
                        int idx = matchHotKey(event.code, currentModifiers);
                        if (idx >= 0 && g_hotKeys[idx].pressed) {
                            g_hotKeys[idx].pressed = 0;
                            if (g_hotKeys[idx].releaseCallback) {
                                g_hotKeys[idx].releaseCallback();
                            }
                        } else {
                            for (int i = 0; i < g_hotKeyCount; i++) {
                                if (g_hotKeys[i].pressed && g_hotKeys[i].keycode == event.code) {
                                    g_hotKeys[i].pressed = 0;
                                    if (g_hotKeys[i].releaseCallback) {
                                        g_hotKeys[i].releaseCallback();
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    return NULL;
}

int RegisterHotKeyCallback(const char* key, HotKeyCallback pressCallback, HotKeyCallback releaseCallback)
{
    unsigned int keycode, modifiers;
    if (!parseHotKey(key, &keycode, &modifiers)) {
        return -1;
    }
    
    for (int i = 0; i < g_hotKeyCount; i++) {
        if (g_hotKeys[i].keycode == keycode && g_hotKeys[i].modifiers == modifiers) {
            g_hotKeys[i].pressCallback = pressCallback;
            g_hotKeys[i].releaseCallback = releaseCallback;
            return 0;
        }
    }
    
    HotKeyItem* newHotKeys = (HotKeyItem*)realloc(g_hotKeys, (g_hotKeyCount + 1) * sizeof(HotKeyItem));
    if (!newHotKeys) return -1;
    
    g_hotKeys = newHotKeys;
    g_hotKeys[g_hotKeyCount].keycode = keycode;
    g_hotKeys[g_hotKeyCount].modifiers = modifiers;
    g_hotKeys[g_hotKeyCount].pressCallback = pressCallback;
    g_hotKeys[g_hotKeyCount].releaseCallback = releaseCallback;
    g_hotKeys[g_hotKeyCount].pressed = 0;
    g_hotKeyCount++;
    
    if (g_kbFd < 0) {
        for (int i = 0; i < 10; i++) {
            char path[32];
            sprintf(path, "/dev/input/event%d", i);
            g_kbFd = open(path, O_RDONLY);
            if (g_kbFd >= 0) break;
        }
        
        if (g_kbFd >= 0) {
            g_running = 1;
            pthread_create(&g_threadId, NULL, keyboardThread, NULL);
        }
    }
    
    return (g_kbFd >= 0) ? 0 : -1;
}

void UnregisterAllHotKeys()
{
    g_running = 0;
    
    if (g_kbFd >= 0) {
        close(g_kbFd);
        g_kbFd = -1;
    }
    
    if (g_threadId != 0) {
        pthread_join(g_threadId, NULL);
        g_threadId = 0;
    }
    
    if (g_hotKeys != NULL) {
        free(g_hotKeys);
        g_hotKeys = NULL;
    }
    g_hotKeyCount = 0;
}

#endif
