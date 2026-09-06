#include "settings.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "platform.h"

/* Settings live beside the app's other per-user state:
   ~/Library/Application Support/<directory>/preferred-connection */
static int preference_path_in(const char *directory,
                              char *path, size_t path_size,
                              bool create_directory) {
    if (create_directory) {
        char settings_dir[PATH_BUFFER_SIZE];
        if (home_relative_path(settings_dir, sizeof(settings_dir),
                               "Library/Application Support") != 0)
            return -1;
        make_directory(settings_dir);
        if (home_relative_path(settings_dir, sizeof(settings_dir),
                               "Library/Application Support/%s", directory) != 0)
            return -1;
        make_directory(settings_dir);
    }
    return home_relative_path(
        path, path_size,
        "Library/Application Support/%s/preferred-connection", directory);
}

const char *connection_display_name(keyboard_connection connection) {
    switch (connection) {
        case KEYBOARD_CONNECTION_WIRED: return "USB wired";
        case KEYBOARD_CONNECTION_BLE: return "Bluetooth LE";
        default: return "auto";
    }
}

keyboard_connection load_preferred_connection(void) {
    char path[PATH_BUFFER_SIZE], value[32] = {0};
    if (preference_path_in(SETTINGS_DIRECTORY, path, sizeof(path), false) != 0)
        return KEYBOARD_CONNECTION_NONE;

    FILE *file = fopen(path, "r");
    if (!file) {
        /* Upgrades from v0.1.x keep the connection chosen under the old name. */
        if (preference_path_in(LEGACY_SETTINGS_DIRECTORY,
                               path, sizeof(path), false) != 0)
            return KEYBOARD_CONNECTION_NONE;
        file = fopen(path, "r");
    }
    if (!file) return KEYBOARD_CONNECTION_NONE;

    char *line = fgets(value, sizeof(value), file);
    fclose(file);
    if (!line) return KEYBOARD_CONNECTION_NONE;
    if (strncmp(value, "wired", 5) == 0) return KEYBOARD_CONNECTION_WIRED;
    if (strncmp(value, "ble", 3) == 0) return KEYBOARD_CONNECTION_BLE;
    return KEYBOARD_CONNECTION_NONE;
}

int save_preferred_connection(keyboard_connection connection) {
    char path[PATH_BUFFER_SIZE];
    if (preference_path_in(SETTINGS_DIRECTORY, path, sizeof(path), true) != 0)
        return 1;
    FILE *file = fopen(path, "w");
    if (!file) return 1;
    const char *value = connection == KEYBOARD_CONNECTION_WIRED ? "wired\n"
        : connection == KEYBOARD_CONNECTION_BLE ? "ble\n" : "auto\n";
    int failed = fputs(value, file) == EOF;
    if (fclose(file) != 0) failed = 1;
    return failed ? 1 : 0;
}
