#pragma once
// Pure policy shared with host tests; no NimBLE, NVS or UI dependencies.
inline bool watch_ble_key_is_strong(bool encrypted, bool authenticated, bool bonded, unsigned key_bytes, bool sc) {
    return encrypted && authenticated && bonded && key_bytes == 16 && sc;
}
inline bool watch_ble_can_pair(unsigned trust_phase, bool storage_failed, bool forgetting) {
    return trust_phase == 0 && !storage_failed && !forgetting;
}
