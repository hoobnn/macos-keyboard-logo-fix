#ifndef SETTINGS_H
#define SETTINGS_H

/* Which connection the background service is allowed to drive. */
typedef enum {
    KEYBOARD_CONNECTION_NONE, /* stored as "auto": every compatible keyboard */
    KEYBOARD_CONNECTION_WIRED,
    KEYBOARD_CONNECTION_BLE
} keyboard_connection;

/* Human-readable name, for log lines and dialogs. */
const char *connection_display_name(keyboard_connection connection);

/* Reads the saved preference, falling back to the v0.1.x location. Any
   unreadable or unrecognised value is reported as KEYBOARD_CONNECTION_NONE. */
keyboard_connection load_preferred_connection(void);

/* Persists the preference; returns 0 on success. */
int save_preferred_connection(keyboard_connection connection);

#endif
