#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <IOKit/hid/IOHIDLib.h>
#include <stdbool.h>

#include "settings.h"

/* HID identifiers of the keyboards this tool knows how to address. */
#define COMPAT_WIRED_VID 0x258a
#define COMPAT_WIRED_PID 0x010c
#define COMPAT_BLE_VID 0x3554
#define COMPAT_BLE_PID 0xfa07

/* Input Monitoring access. Public since macOS 10.15, but the declarations live
   in IOHIDLib.h's hidsystem variant, which conflicts with the modern HID
   header, so they are redeclared here. */
extern int IOHIDCheckAccess(int request_type);
extern bool IOHIDRequestAccess(int request_type);
#define HID_REQUEST_LISTEN_EVENT 1

/* True once Input Monitoring is granted, prompting the user if needed. */
bool ensure_input_monitoring_access(void);

/* Classifies a HID device, or KEYBOARD_CONNECTION_NONE if it is not one of
   the supported keyboards. */
keyboard_connection connection_for_device(IOHIDDeviceRef device);

/* True when `connection` is a supported keyboard the `preferred` setting
   allows the program to drive. */
bool connection_matches_preference(keyboard_connection connection,
                                   keyboard_connection preferred);

/* Creates an open HID manager matching every device, or NULL on failure. */
IOHIDManagerRef open_hid_manager(void);

/* Calls `visitor` once per device currently known to `manager`. */
void for_each_device(IOHIDManagerRef manager,
                     void (*visitor)(IOHIDDeviceRef device, void *context),
                     void *context);

/* Sends the LOGO unlock report to `device` for `seconds`; returns 0 on
   success. Does nothing for devices that are not supported keyboards. */
int apply_to_device(IOHIDDeviceRef device, int seconds);

/* Applies to every connected keyboard the preference allows. */
void apply_to_all_connected(IOHIDManagerRef manager, int seconds);

/* Reports which supported keyboards are currently connected. */
void scan_connection_status(bool *wired_present, bool *ble_present);

/* Opens its own manager, applies to the first keyboard matching the saved
   preference, and reports the outcome on stdout. Returns 0 on success. */
int restore_saved_logo_effect(int seconds);

/* Formats an IOKit failure on stderr. */
void print_io_error(const char *operation, IOReturn result);

#endif
