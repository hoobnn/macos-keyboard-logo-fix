#include "keyboard.h"

#include <mach/mach_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define KEYBOARD_USAGE_PAGE 0x01
#define KEYBOARD_USAGE 0x06
#define LOGO_UNLOCK_REPORT_VALUE 0x01

/* The report is resent at this interval to outlast the green-indicator writes
   macOS makes while the keyboard initialises. */
#define REPORT_INTERVAL_MICROSECONDS 50000
#define REPORTS_PER_SECOND 20

void print_io_error(const char *operation, IOReturn result) {
    fprintf(stderr, "%s failed: 0x%08x (%s)\n",
            operation, result, mach_error_string(result));
}

bool ensure_input_monitoring_access(void) {
    return IOHIDCheckAccess(HID_REQUEST_LISTEN_EVENT) == 0 ||
           IOHIDRequestAccess(HID_REQUEST_LISTEN_EVENT);
}

static long number_property(IOHIDDeviceRef device, CFStringRef key) {
    CFTypeRef property = IOHIDDeviceGetProperty(device, key);
    long value = -1;
    if (property && CFGetTypeID(property) == CFNumberGetTypeID())
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongType, &value);
    return value;
}

keyboard_connection connection_for_device(IOHIDDeviceRef device) {
    long usage_page = number_property(device, CFSTR(kIOHIDPrimaryUsagePageKey));
    long usage = number_property(device, CFSTR(kIOHIDPrimaryUsageKey));
    long output_size = number_property(device, CFSTR(kIOHIDMaxOutputReportSizeKey));
    /* Only the keyboard interface that accepts output reports can carry the
       unlock report; the same hardware exposes several other interfaces. */
    if (usage_page != KEYBOARD_USAGE_PAGE || usage != KEYBOARD_USAGE ||
        output_size < 1)
        return KEYBOARD_CONNECTION_NONE;

    long vid = number_property(device, CFSTR(kIOHIDVendorIDKey));
    long pid = number_property(device, CFSTR(kIOHIDProductIDKey));
    if (vid == COMPAT_WIRED_VID && pid == COMPAT_WIRED_PID)
        return KEYBOARD_CONNECTION_WIRED;
    if (vid == COMPAT_BLE_VID && pid == COMPAT_BLE_PID)
        return KEYBOARD_CONNECTION_BLE;
    return KEYBOARD_CONNECTION_NONE;
}

bool connection_matches_preference(keyboard_connection connection,
                                   keyboard_connection preferred) {
    if (connection == KEYBOARD_CONNECTION_NONE) return false;
    return preferred == KEYBOARD_CONNECTION_NONE || connection == preferred;
}

IOHIDManagerRef open_hid_manager(void) {
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
    return manager;
}

static void close_hid_manager(IOHIDManagerRef manager) {
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
}

void for_each_device(IOHIDManagerRef manager,
                     void (*visitor)(IOHIDDeviceRef device, void *context),
                     void *context) {
    if (!manager) return;
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (!devices) return;
    CFIndex count = CFSetGetCount(devices);
    const void **items = calloc((size_t)count, sizeof(*items));
    if (items) {
        CFSetGetValues(devices, items);
        for (CFIndex i = 0; i < count; i++)
            visitor((IOHIDDeviceRef)items[i], context);
        free(items);
    }
    CFRelease(devices);
}

int apply_to_device(IOHIDDeviceRef device, int seconds) {
    keyboard_connection connection = connection_for_device(device);
    if (connection == KEYBOARD_CONNECTION_NONE) return 1;

    IOReturn result = IOHIDDeviceOpen(device, kIOHIDOptionsTypeNone);
    if (result != kIOReturnSuccess && result != kIOReturnExclusiveAccess) {
        print_io_error("IOHIDDeviceOpen", result);
        return 1;
    }

    /* Over BLE the report is numbered and carries its ID as the first byte;
       the wired interface uses an unnumbered single-byte report. */
    bool is_ble = connection == KEYBOARD_CONNECTION_BLE;
    uint8_t wired_report[] = {LOGO_UNLOCK_REPORT_VALUE};
    uint8_t ble_report[] = {0x01, LOGO_UNLOCK_REPORT_VALUE};
    const uint8_t *report = is_ble ? ble_report : wired_report;
    CFIndex report_length =
        is_ble ? (CFIndex)sizeof(ble_report) : (CFIndex)sizeof(wired_report);
    CFIndex report_id = is_ble ? 0x01 : 0x00;

    fprintf(stderr, "Restoring saved logo effect over %s for %d seconds\n",
            connection_display_name(connection), seconds);
    for (int i = 0; i < seconds * REPORTS_PER_SECOND; i++) {
        result = IOHIDDeviceSetReport(
            device, kIOHIDReportTypeOutput, report_id, report, report_length);
        if (result != kIOReturnSuccess) {
            print_io_error("IOHIDDeviceSetReport", result);
            break;
        }
        usleep(REPORT_INTERVAL_MICROSECONDS);
    }
    IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
    return result == kIOReturnSuccess ? 0 : 1;
}

typedef struct {
    keyboard_connection preferred;
    int seconds;
} apply_context;

static void apply_visitor(IOHIDDeviceRef device, void *context) {
    apply_context *state = context;
    if (connection_matches_preference(connection_for_device(device),
                                      state->preferred))
        apply_to_device(device, state->seconds);
}

void apply_to_all_connected(IOHIDManagerRef manager, int seconds) {
    apply_context state = {load_preferred_connection(), seconds};
    for_each_device(manager, apply_visitor, &state);
}

typedef struct {
    keyboard_connection preferred;
    IOHIDDeviceRef found;
    keyboard_connection connection;
} search_context;

static void search_visitor(IOHIDDeviceRef device, void *context) {
    search_context *state = context;
    if (state->found) return;
    keyboard_connection connection = connection_for_device(device);
    if (connection_matches_preference(connection, state->preferred)) {
        state->found = (IOHIDDeviceRef)CFRetain(device);
        state->connection = connection;
    }
}

int restore_saved_logo_effect(int seconds) {
    IOHIDManagerRef manager = open_hid_manager();
    if (!manager) return 1;

    search_context state = {load_preferred_connection(), NULL,
                            KEYBOARD_CONNECTION_NONE};
    for_each_device(manager, search_visitor, &state);
    if (!state.found) {
        fprintf(stderr,
                "Compatible keyboard not found "
                "(wired %04X:%04X or Bluetooth %04X:%04X)\n",
                COMPAT_WIRED_VID, COMPAT_WIRED_PID,
                COMPAT_BLE_VID, COMPAT_BLE_PID);
        close_hid_manager(manager);
        return 1;
    }

    printf("Connection: %s; sending LOGO unlock for %d seconds...\n",
           connection_display_name(state.connection), seconds);
    fflush(stdout);
    int result = apply_to_device(state.found, seconds);
    CFRelease(state.found);
    close_hid_manager(manager);
    return result;
}

typedef struct {
    bool *wired_present;
    bool *ble_present;
} status_context;

static void status_visitor(IOHIDDeviceRef device, void *context) {
    status_context *state = context;
    switch (connection_for_device(device)) {
        case KEYBOARD_CONNECTION_WIRED: *state->wired_present = true; break;
        case KEYBOARD_CONNECTION_BLE: *state->ble_present = true; break;
        default: break;
    }
}

void scan_connection_status(bool *wired_present, bool *ble_present) {
    *wired_present = false;
    *ble_present = false;
    IOHIDManagerRef manager = open_hid_manager();
    if (!manager) return;
    status_context state = {wired_present, ble_present};
    for_each_device(manager, status_visitor, &state);
    close_hid_manager(manager);
}
