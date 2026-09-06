#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <mach-o/dyld.h>
#include <mach/mach_error.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define COMPAT_WIRED_VID 0x258a
#define COMPAT_WIRED_PID 0x010c
#define COMPAT_BLE_VID 0x3554
#define COMPAT_BLE_PID 0xfa07
#define KEYBOARD_USAGE 0x06
#define LOGO_UNLOCK_REPORT_VALUE 0x01

/* Public since macOS 10.15; declarations normally live in IOHIDLib.h's
   hidsystem variant, which conflicts with the modern HID header above. */
extern int IOHIDCheckAccess(int request_type);
extern bool IOHIDRequestAccess(int request_type);
#define HID_REQUEST_LISTEN_EVENT 1
#define SERVICE_LABEL "com.ikuyu.keyboard-logo-fix"
#define LEGACY_SERVICE_LABEL "local.codex.t100-logo-white"
#define SETTINGS_DIRECTORY "KeyboardLogoFix"
#define LEGACY_SETTINGS_DIRECTORY "T100Logo"

extern char **environ;

static int run_process(const char *path, char *const arguments[]) {
    pid_t child = fork();
    if (child < 0) return -1;
    if (child == 0) {
        execve(path, arguments, environ);
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int executable_path(char *output, size_t output_size) {
    uint32_t size = (uint32_t)output_size;
    if (_NSGetExecutablePath(output, &size) != 0) return -1;
    char resolved[4096];
    if (!realpath(output, resolved)) return -1;
    if (strlen(resolved) + 1 > output_size) return -1;
    strcpy(output, resolved);
    return 0;
}

static void xml_write_escaped(FILE *file, const char *text) {
    for (const char *p = text; *p; p++) {
        switch (*p) {
            case '&': fputs("&amp;", file); break;
            case '<': fputs("&lt;", file); break;
            case '>': fputs("&gt;", file); break;
            case '\"': fputs("&quot;", file); break;
            case '\'': fputs("&apos;", file); break;
            default: fputc(*p, file); break;
        }
    }
}

static int launch_agent_path(const char *label, char *path, size_t path_size) {
    struct passwd *account = getpwuid(getuid());
    if (!account || !account->pw_dir) return -1;
    if (snprintf(path, path_size,
                 "%s/Library/LaunchAgents/%s.plist",
                 account->pw_dir, label) >= (int)path_size)
        return -1;
    return 0;
}

static int service_paths(char *plist_path, size_t plist_size,
                         char *log_path, size_t log_size) {
    struct passwd *account = getpwuid(getuid());
    if (!account || !account->pw_dir ||
        launch_agent_path(SERVICE_LABEL, plist_path, plist_size) != 0)
        return -1;
    if (snprintf(log_path, log_size, "%s/Library/Logs/KeyboardLogoFix.log",
                 account->pw_dir) >= (int)log_size)
        return -1;
    return 0;
}

static int preference_path_for_directory(const char *directory,
                                         char *path, size_t path_size,
                                         bool create_directory) {
    struct passwd *account = getpwuid(getuid());
    if (!account || !account->pw_dir) return -1;
    char app_support[4096], settings_dir[4096];
    snprintf(app_support, sizeof(app_support), "%s/Library/Application Support",
             account->pw_dir);
    snprintf(settings_dir, sizeof(settings_dir), "%s/%s", app_support, directory);
    if (create_directory) {
        mkdir(app_support, 0755);
        mkdir(settings_dir, 0755);
    }
    if (snprintf(path, path_size, "%s/preferred-connection", settings_dir)
        >= (int)path_size)
        return -1;
    return 0;
}

static int preference_path(char *path, size_t path_size, bool create_directory) {
    return preference_path_for_directory(
        SETTINGS_DIRECTORY, path, path_size, create_directory);
}

static int remove_background_service_named(const char *label) {
    char plist_path[4096], domain[64];
    if (launch_agent_path(label, plist_path, sizeof(plist_path)) != 0)
        return 1;
    snprintf(domain, sizeof(domain), "gui/%u", getuid());
    char *bootout[] = {"launchctl", "bootout", domain, plist_path, NULL};
    run_process("/bin/launchctl", bootout);
    if (unlink(plist_path) != 0 && access(plist_path, F_OK) == 0) return 1;
    return 0;
}

static int install_background_service(void) {
    char executable[4096], plist_path[4096], log_path[4096];
    if (executable_path(executable, sizeof(executable)) != 0 ||
        service_paths(plist_path, sizeof(plist_path), log_path, sizeof(log_path)) != 0)
        return 1;

    struct passwd *account = getpwuid(getuid());
    char launch_agents[4096], logs[4096], domain[64];
    snprintf(launch_agents, sizeof(launch_agents), "%s/Library/LaunchAgents", account->pw_dir);
    snprintf(logs, sizeof(logs), "%s/Library/Logs", account->pw_dir);
    mkdir(launch_agents, 0755);
    mkdir(logs, 0755);

    /* Stop the v0.1.x service before installing the renamed service. */
    remove_background_service_named(LEGACY_SERVICE_LABEL);

    char temporary[4096];
    snprintf(temporary, sizeof(temporary), "%s.tmp", plist_path);
    FILE *file = fopen(temporary, "w");
    if (!file) return 1;
    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
          "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
          "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
          "<plist version=\"1.0\"><dict>\n"
          "<key>Label</key><string>" SERVICE_LABEL "</string>\n"
          "<key>ProgramArguments</key><array><string>", file);
    xml_write_escaped(file, executable);
    fputs("</string><string>--daemon</string></array>\n"
          "<key>RunAtLoad</key><true/>\n"
          "<key>KeepAlive</key><true/>\n"
          "<key>ThrottleInterval</key><integer>10</integer>\n"
          "<key>StandardOutPath</key><string>", file);
    xml_write_escaped(file, log_path);
    fputs("</string>\n<key>StandardErrorPath</key><string>", file);
    xml_write_escaped(file, log_path);
    fputs("</string>\n</dict></plist>\n", file);
    if (fclose(file) != 0 || rename(temporary, plist_path) != 0) return 1;

    snprintf(domain, sizeof(domain), "gui/%u", getuid());
    char *bootout[] = {"launchctl", "bootout", domain, plist_path, NULL};
    char *bootstrap[] = {"launchctl", "bootstrap", domain, plist_path, NULL};
    char service_target[128];
    snprintf(service_target, sizeof(service_target), "%s/%s", domain, SERVICE_LABEL);
    char *enable[] = {"launchctl", "enable", service_target, NULL};
    run_process("/bin/launchctl", bootout);
    if (run_process("/bin/launchctl", bootstrap) != 0) return 1;
    run_process("/bin/launchctl", enable);
    fprintf(stderr, "Background service installed: %s\n", plist_path);
    return 0;
}

static int uninstall_background_service(void) {
    int current_result = remove_background_service_named(SERVICE_LABEL);
    int legacy_result = remove_background_service_named(LEGACY_SERVICE_LABEL);
    fprintf(stderr, "Keyboard Logo Fix background service removed\n");
    return current_result == 0 && legacy_result == 0 ? 0 : 1;
}

static long number_property(IOHIDDeviceRef device, CFStringRef key) {
    CFTypeRef property = IOHIDDeviceGetProperty(device, key);
    long value = -1;
    if (property && CFGetTypeID(property) == CFNumberGetTypeID())
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongType, &value);
    return value;
}

static void print_io_error(const char *operation, IOReturn result) {
    fprintf(stderr, "%s failed: 0x%08x (%s)\n",
            operation, result, mach_error_string(result));
}

typedef enum {
    KEYBOARD_CONNECTION_NONE,
    KEYBOARD_CONNECTION_WIRED,
    KEYBOARD_CONNECTION_BLE
} keyboard_connection;

static keyboard_connection load_preferred_connection(void) {
    char path[4096], value[32] = {0};
    if (preference_path(path, sizeof(path), false) != 0)
        return KEYBOARD_CONNECTION_NONE;
    FILE *file = fopen(path, "r");
    if (!file) {
        if (preference_path_for_directory(
                LEGACY_SETTINGS_DIRECTORY, path, sizeof(path), false) != 0)
            return KEYBOARD_CONNECTION_NONE;
        file = fopen(path, "r");
    }
    if (!file) return KEYBOARD_CONNECTION_NONE;
    fgets(value, sizeof(value), file);
    fclose(file);
    if (strncmp(value, "wired", 5) == 0) return KEYBOARD_CONNECTION_WIRED;
    if (strncmp(value, "ble", 3) == 0) return KEYBOARD_CONNECTION_BLE;
    return KEYBOARD_CONNECTION_NONE;
}

static int save_preferred_connection(keyboard_connection connection) {
    char path[4096];
    if (preference_path(path, sizeof(path), true) != 0) return 1;
    FILE *file = fopen(path, "w");
    if (!file) return 1;
    const char *value = connection == KEYBOARD_CONNECTION_WIRED ? "wired\n"
        : connection == KEYBOARD_CONNECTION_BLE ? "ble\n" : "auto\n";
    int failed = fputs(value, file) == EOF || fclose(file) != 0;
    return failed ? 1 : 0;
}

static IOHIDManagerRef daemon_manager;
static io_connect_t power_root_port;
static IONotificationPortRef power_notify_port;
static io_object_t power_notifier;

static keyboard_connection connection_for_device(IOHIDDeviceRef device) {
    long vid = number_property(device, CFSTR(kIOHIDVendorIDKey));
    long pid = number_property(device, CFSTR(kIOHIDProductIDKey));
    long usage_page = number_property(device, CFSTR(kIOHIDPrimaryUsagePageKey));
    long usage = number_property(device, CFSTR(kIOHIDPrimaryUsageKey));
    long output_size = number_property(device, CFSTR(kIOHIDMaxOutputReportSizeKey));
    if (usage_page != 1 || usage != KEYBOARD_USAGE || output_size < 1)
        return KEYBOARD_CONNECTION_NONE;
    if (vid == COMPAT_WIRED_VID && pid == COMPAT_WIRED_PID)
        return KEYBOARD_CONNECTION_WIRED;
    if (vid == COMPAT_BLE_VID && pid == COMPAT_BLE_PID)
        return KEYBOARD_CONNECTION_BLE;
    return KEYBOARD_CONNECTION_NONE;
}

static int apply_to_device(IOHIDDeviceRef device, int seconds) {
    keyboard_connection connection = connection_for_device(device);
    if (connection == KEYBOARD_CONNECTION_NONE) return 1;
    IOReturn result = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    if (result != kIOReturnSuccess && result != kIOReturnExclusiveAccess) {
        print_io_error("IOHIDDeviceOpen", result);
        return 1;
    }
    uint8_t wired_report[] = {LOGO_UNLOCK_REPORT_VALUE};
    uint8_t ble_report[] = {0x01, LOGO_UNLOCK_REPORT_VALUE};
    const uint8_t *report = connection == KEYBOARD_CONNECTION_BLE ? ble_report : wired_report;
    CFIndex report_length = connection == KEYBOARD_CONNECTION_BLE
        ? (CFIndex)sizeof(ble_report) : (CFIndex)sizeof(wired_report);
    CFIndex report_id = connection == KEYBOARD_CONNECTION_BLE ? 0x01 : 0x00;
    fprintf(stderr, "Restoring saved logo effect over %s for %d seconds\n",
            connection == KEYBOARD_CONNECTION_BLE ? "Bluetooth LE" : "USB", seconds);
    for (int i = 0; i < seconds * 20; i++) {
        result = IOHIDDeviceSetReport(
            device, kIOHIDReportTypeOutput, report_id, report, report_length);
        if (result != kIOReturnSuccess) {
            print_io_error("IOHIDDeviceSetReport", result);
            break;
        }
        usleep(50000);
    }
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    return result == kIOReturnSuccess ? 0 : 1;
}

static IOHIDDeviceRef find_compatible_keyboard(IOHIDManagerRef *manager_out,
                                                keyboard_connection *connection_out,
                                                keyboard_connection preferred) {
    IOHIDManagerRef manager =
        IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) return NULL;

    IOHIDManagerSetDeviceMatching(manager, NULL);

    IOReturn result = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (result != kIOReturnSuccess) {
        print_io_error("IOHIDManagerOpen", result);
        CFRelease(manager);
        return NULL;
    }

    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    IOHIDDeviceRef found = NULL;
    if (devices) {
        CFIndex count = CFSetGetCount(devices);
        const void **items = calloc((size_t)count, sizeof(*items));
        CFSetGetValues(devices, items);
        for (CFIndex i = 0; i < count; i++) {
            IOHIDDeviceRef candidate = (IOHIDDeviceRef)items[i];
            keyboard_connection connection = connection_for_device(candidate);
            if (connection != KEYBOARD_CONNECTION_NONE &&
                (preferred == KEYBOARD_CONNECTION_NONE || connection == preferred)) {
                found = candidate;
                CFRetain(found);
                *connection_out = connection;
                break;
            }
        }
        free(items);
        CFRelease(devices);
    }

    if (!found) {
        fprintf(stderr,
                "Compatible keyboard not found (wired %04X:%04X or Bluetooth %04X:%04X)\n",
                COMPAT_WIRED_VID, COMPAT_WIRED_PID,
                COMPAT_BLE_VID, COMPAT_BLE_PID);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return NULL;
    }

    *manager_out = manager;
    return found;
}

static int restore_saved_logo_effect(int seconds) {
    IOHIDManagerRef manager = NULL;
    keyboard_connection connection = KEYBOARD_CONNECTION_NONE;
    keyboard_connection preferred = load_preferred_connection();
    IOHIDDeviceRef device = find_compatible_keyboard(&manager, &connection, preferred);
    if (!device) return 1;

    printf("Connection: %s; sending LOGO unlock for %d seconds...\n",
           connection == KEYBOARD_CONNECTION_BLE ? "Bluetooth LE" : "USB wired",
           seconds);
    fflush(stdout);
    int result = apply_to_device(device, seconds);
    CFRelease(device);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return result;
}

static void apply_all_connected(void) {
    if (!daemon_manager) return;
    CFSetRef devices = IOHIDManagerCopyDevices(daemon_manager);
    if (!devices) return;
    CFIndex count = CFSetGetCount(devices);
    const void **items = calloc((size_t)count, sizeof(*items));
    CFSetGetValues(devices, items);
    keyboard_connection preferred = load_preferred_connection();
    for (CFIndex i = 0; i < count; i++) {
        IOHIDDeviceRef device = (IOHIDDeviceRef)items[i];
        keyboard_connection connection = connection_for_device(device);
        if (connection != KEYBOARD_CONNECTION_NONE &&
            (preferred == KEYBOARD_CONNECTION_NONE || connection == preferred))
            apply_to_device(device, 3);
    }
    free(items);
    CFRelease(devices);
}

static void apply_timer_callback(CFRunLoopTimerRef timer, void *context) {
    (void)timer; (void)context;
    apply_all_connected();
}

static void schedule_apply(double delay_seconds) {
    CFRunLoopTimerContext context = {0, NULL, NULL, NULL, NULL};
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(
        NULL, CFAbsoluteTimeGetCurrent() + delay_seconds, 0, 0, 0,
        apply_timer_callback, &context);
    if (timer) {
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);
        CFRelease(timer);
    }
}

static void device_matched(void *context, IOReturn result, void *sender,
                           IOHIDDeviceRef device) {
    (void)context; (void)result; (void)sender;
    keyboard_connection connection = connection_for_device(device);
    keyboard_connection preferred = load_preferred_connection();
    if (connection != KEYBOARD_CONNECTION_NONE &&
        (preferred == KEYBOARD_CONNECTION_NONE || connection == preferred)) {
        fprintf(stderr, "Compatible keyboard connected; scheduling logo restore\n");
        schedule_apply(1.0);
        schedule_apply(5.0);
    }
}

static void power_callback(void *context, io_service_t service,
                           natural_t message_type, void *message_argument) {
    (void)context; (void)service;
    switch (message_type) {
        case kIOMessageCanSystemSleep:
        case kIOMessageSystemWillSleep:
            IOAllowPowerChange(power_root_port, (long)message_argument);
            break;
        case kIOMessageSystemHasPoweredOn:
            fprintf(stderr, "Mac woke; scheduling LOGO restore\n");
            schedule_apply(2.0);
            schedule_apply(8.0);
            break;
        default:
            break;
    }
}

static int run_daemon(void) {
    while (IOHIDCheckAccess(HID_REQUEST_LISTEN_EVENT) != 0) {
        fprintf(stderr, "Waiting for Input Monitoring permission...\n");
        sleep(5);
    }
    daemon_manager = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);
    if (!daemon_manager) return 1;
    IOHIDManagerSetDeviceMatching(daemon_manager, NULL);
    IOHIDManagerRegisterDeviceMatchingCallback(daemon_manager, device_matched, NULL);
    IOHIDManagerScheduleWithRunLoop(
        daemon_manager, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
    IOReturn result = IOHIDManagerOpen(daemon_manager, kIOHIDOptionsTypeNone);
    if (result != kIOReturnSuccess) {
        print_io_error("IOHIDManagerOpen", result);
        return 1;
    }
    power_root_port = IORegisterForSystemPower(
        NULL, &power_notify_port, power_callback, &power_notifier);
    if (power_root_port != IO_OBJECT_NULL && power_notify_port) {
        CFRunLoopSourceRef source =
            IONotificationPortGetRunLoopSource(power_notify_port);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), source, kCFRunLoopCommonModes);
    } else {
        fprintf(stderr, "Warning: system wake notifications unavailable\n");
    }
    schedule_apply(1.0);
    fprintf(stderr, "Keyboard Logo Fix background service started\n");
    CFRunLoopRun();
    return 0;
}

static void scan_connection_status(bool *wired_present, bool *ble_present) {
    *wired_present = false;
    *ble_present = false;
    IOHIDManagerRef manager = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);
    if (!manager) return;
    IOHIDManagerSetDeviceMatching(manager, NULL);
    if (IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        CFRelease(manager);
        return;
    }
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (devices) {
        CFIndex count = CFSetGetCount(devices);
        const void **items = calloc((size_t)count, sizeof(*items));
        CFSetGetValues(devices, items);
        for (CFIndex i = 0; i < count; i++) {
            keyboard_connection connection = connection_for_device((IOHIDDeviceRef)items[i]);
            if (connection == KEYBOARD_CONNECTION_WIRED) *wired_present = true;
            if (connection == KEYBOARD_CONNECTION_BLE) *ble_present = true;
        }
        free(items);
        CFRelease(devices);
    }
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
}

static void show_result_message(bool success) {
    char *success_args[] = {
        "osascript", "-e",
        "display notification \"已恢复键盘保存的 LOGO 灯效，并保存后台连接偏好\" with title \"Keyboard Logo Fix\"",
        NULL
    };
    char *failure_args[] = {
        "osascript", "-e",
        "display dialog \"未找到所选连接的兼容键盘，请确认键盘已连接且未休眠。\" with title \"Keyboard Logo Fix\" buttons {\"好\"} default button \"好\" with icon caution",
        NULL
    };
    run_process("/usr/bin/osascript", success ? success_args : failure_args);
}

static int run_selection_interface(void) {
    bool wired_present, ble_present;
    scan_connection_status(&wired_present, &ble_present);
    char wired_label[128], ble_label[128], command[4096], selected[256] = {0};
    snprintf(wired_label, sizeof(wired_label), "USB 有线 258A:010C — %s",
             wired_present ? "已连接" : "未连接");
    snprintf(ble_label, sizeof(ble_label), "蓝牙 5.0 3554:FA07 — %s",
             ble_present ? "已连接" : "未连接");
    const char *auto_label = "自动选择所有已连接的兼容键盘";
    keyboard_connection preferred = load_preferred_connection();
    const char *default_label = preferred == KEYBOARD_CONNECTION_WIRED ? wired_label
        : preferred == KEYBOARD_CONNECTION_BLE ? ble_label : auto_label;
    snprintf(command, sizeof(command),
        "/usr/bin/osascript "
        "-e 'set picked to choose from list {\"%s\", \"%s\", \"%s\"} "
        "with title \"Keyboard Logo Fix\" "
        "with prompt \"请选择后台自动控制的连接方式：\" "
        "default items {\"%s\"} OK button name \"应用并保存\" cancel button name \"取消\"' "
        "-e 'if picked is false then return \"cancel\"' "
        "-e 'return item 1 of picked'",
        auto_label, wired_label, ble_label, default_label);
    FILE *pipe = popen(command, "r");
    if (!pipe) return 1;
    fgets(selected, sizeof(selected), pipe);
    int status = pclose(pipe);
    if (status != 0 || strncmp(selected, "cancel", 6) == 0) return 0;

    keyboard_connection choice = KEYBOARD_CONNECTION_NONE;
    if (strncmp(selected, "USB", 3) == 0) choice = KEYBOARD_CONNECTION_WIRED;
    else if (strncmp(selected, "蓝牙", strlen("蓝牙")) == 0)
        choice = KEYBOARD_CONNECTION_BLE;
    if (save_preferred_connection(choice) != 0) return 1;
    int result = restore_saved_logo_effect(3);
    show_result_message(result == 0);
    return result;
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--daemon") == 0)
        return run_daemon();
    if (argc == 2 && strcmp(argv[1], "--uninstall") == 0)
        return uninstall_background_service();

    if (argc == 1) {
        if (install_background_service() != 0) {
            fprintf(stderr, "Failed to install background service\n");
            return 4;
        }
        if (IOHIDCheckAccess(HID_REQUEST_LISTEN_EVENT) != 0 &&
            !IOHIDRequestAccess(HID_REQUEST_LISTEN_EVENT)) {
            fprintf(stderr, "Input Monitoring permission is required\n");
            return 3;
        }
        return run_selection_interface();
    }

    int seconds = 30;
    if (argc == 2) {
        seconds = atoi(argv[1]);
        if (seconds < 1 || seconds > 300) {
            fprintf(stderr, "duration must be between 1 and 300 seconds\n");
            return 2;
        }
    } else if (argc > 2) {
        fprintf(stderr, "usage: %s [duration_seconds|--daemon|--uninstall]\n", argv[0]);
        return 2;
    }

    if (IOHIDCheckAccess(HID_REQUEST_LISTEN_EVENT) != 0 &&
        !IOHIDRequestAccess(HID_REQUEST_LISTEN_EVENT)) {
        fprintf(stderr, "Input Monitoring permission is required\n");
        return 3;
    }

    return restore_saved_logo_effect(seconds);
}
