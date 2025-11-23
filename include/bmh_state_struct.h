#pragma once
#include <stdint.h>

// Lightweight MultiState definition for public API.
// This header intentionally avoids including Arduino.h or project config
// to reduce coupling. If MAX_PACKAGES is not defined by the project
// config, use a sensible default.

#ifndef MAX_PACKAGES
#define MAX_PACKAGES 10
#endif

typedef struct {
    bool active;
    uint8_t total;
    uint8_t received;
    uint8_t* pkgs[MAX_PACKAGES];
    int pkglens[MAX_PACKAGES];
    unsigned long last_ms;
} MultiState;
