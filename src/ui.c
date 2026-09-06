#include "ui.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "keyboard.h"
#include "platform.h"
#include "settings.h"

#define OSASCRIPT_PATH "/usr/bin/osascript"

/* User-facing strings are Chinese to match the shipped app's audience. */
#define AUTO_LABEL "自动选择所有已连接的兼容键盘"
#define CONNECTED_TEXT "已连接"
#define DISCONNECTED_TEXT "未连接"

/* How long the one-off restore after the picker runs. */
#define APPLY_SECONDS_INTERACTIVE 3

static void show_result_message(bool success) {
    char *success_args[] = {
        "osascript", "-e",
        "display notification \"已恢复键盘保存的 LOGO 灯效，并保存后台连接偏好\" "
        "with title \"" APP_DISPLAY_NAME "\"",
        NULL
    };
    char *failure_args[] = {
        "osascript", "-e",
        "display dialog \"未找到所选连接的兼容键盘，请确认键盘已连接且未休眠。\" "
        "with title \"" APP_DISPLAY_NAME "\" buttons {\"好\"} "
        "default button \"好\" with icon caution",
        NULL
    };
    run_process(OSASCRIPT_PATH, success ? success_args : failure_args);
}

/* Presents the three choices and returns the selected label in `selected`.
   Returns false if the picker failed or the user cancelled. */
static bool ask_for_connection(const char *auto_label, const char *wired_label,
                               const char *ble_label, const char *default_label,
                               char *selected, size_t selected_size) {
    char command[PATH_BUFFER_SIZE];
    snprintf(command, sizeof(command),
        OSASCRIPT_PATH " "
        "-e 'set picked to choose from list {\"%s\", \"%s\", \"%s\"} "
        "with title \"" APP_DISPLAY_NAME "\" "
        "with prompt \"请选择后台自动控制的连接方式：\" "
        "default items {\"%s\"} OK button name \"应用并保存\" "
        "cancel button name \"取消\"' "
        "-e 'if picked is false then return \"cancel\"' "
        "-e 'return item 1 of picked'",
        auto_label, wired_label, ble_label, default_label);

    FILE *pipe = popen(command, "r");
    if (!pipe) return false;
    char *line = fgets(selected, selected_size, pipe);
    int status = pclose(pipe);
    return line != NULL && status == 0 &&
           strncmp(selected, "cancel", 6) != 0;
}

int run_selection_interface(void) {
    bool wired_present, ble_present;
    scan_connection_status(&wired_present, &ble_present);

    char wired_label[128], ble_label[128];
    snprintf(wired_label, sizeof(wired_label), "USB 有线 %04X:%04X — %s",
             COMPAT_WIRED_VID, COMPAT_WIRED_PID,
             wired_present ? CONNECTED_TEXT : DISCONNECTED_TEXT);
    snprintf(ble_label, sizeof(ble_label), "蓝牙 5.0 %04X:%04X — %s",
             COMPAT_BLE_VID, COMPAT_BLE_PID,
             ble_present ? CONNECTED_TEXT : DISCONNECTED_TEXT);

    keyboard_connection preferred = load_preferred_connection();
    const char *default_label = preferred == KEYBOARD_CONNECTION_WIRED ? wired_label
        : preferred == KEYBOARD_CONNECTION_BLE ? ble_label : AUTO_LABEL;

    char selected[256] = {0};
    if (!ask_for_connection(AUTO_LABEL, wired_label, ble_label, default_label,
                            selected, sizeof(selected)))
        return 0;

    keyboard_connection choice = KEYBOARD_CONNECTION_NONE;
    if (strncmp(selected, "USB", 3) == 0)
        choice = KEYBOARD_CONNECTION_WIRED;
    else if (strncmp(selected, "蓝牙", strlen("蓝牙")) == 0)
        choice = KEYBOARD_CONNECTION_BLE;
    if (save_preferred_connection(choice) != 0) return 1;

    int result = restore_saved_logo_effect(APPLY_SECONDS_INTERACTIVE);
    show_result_message(result == 0);
    return result;
}
