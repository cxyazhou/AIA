#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*HotKeyCallback)(void);

int RegisterHotKeyCallback(const char* key, HotKeyCallback pressCallback, HotKeyCallback releaseCallback);

void UnregisterAllHotKeys();

#ifdef __cplusplus
}
#endif