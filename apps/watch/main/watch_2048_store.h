#pragma once
#include <stdint.h>

uint32_t watch_2048_store_load();
// Runs on the UI thread: queue improvements; force at game exit/reset/IDLE.
bool watch_2048_store_best(uint32_t best, bool force);
void watch_2048_store_tick();
