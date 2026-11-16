#ifndef TEOPCE_STORAGE_H
#define TEOPCE_STORAGE_H

#include "preferences.h"

#define STORAGE_BAD_DATA      0xe2
#define STORAGE_VERIFY_FAILED 0xe3

/*
 * Load newest valid preference slot, or zero defaults if none is usable.
 * Missing files are normal. A corrupt slot is recoverable when its sibling
 * validates. I/O errors are always returned, even with one readable slot;
 * callers must disable saves until load succeeds, since the unread slot may
 * contain newer data. With no valid slot, defaults remain available to browse.
 */
unsigned char storage_load(Preferences *prefs);

/*
 * Write and verify the next alternating slot. Only a successful readback
 * advances sequence; an error leaves in-memory preferences unchanged.
 * The other slot is never written. An I/O error may still leave a complete
 * new slot on disk, so a later boot can recover it if it validates.
 */
unsigned char storage_save(Preferences *prefs);

#endif
