#include "daemon.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <stdio.h>
#include <unistd.h>

#include "app_config.h"
#include "keyboard.h"
#include "settings.h"

/* Each trigger sends for this long; the pair of delays after an event covers
   both a keyboard that is ready immediately and one still initialising. */
#define APPLY_SECONDS 3
#define DEVICE_RETRY_DELAYS {1.0, 5.0}
#define WAKE_RETRY_DELAYS {2.0, 8.0}
#define STARTUP_DELAY 1.0
#define PERMISSION_POLL_SECONDS 5

static IOHIDManagerRef daemon_manager;
static io_connect_t power_root_port;
static IONotificationPortRef power_notify_port;
static io_object_t power_notifier;

static void apply_timer_callback(CFRunLoopTimerRef timer, void *context) {
    (void)timer; (void)context;
    apply_to_all_connected(daemon_manager, APPLY_SECONDS);
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

static void schedule_apply_series(const double *delays, size_t count) {
    for (size_t i = 0; i < count; i++) schedule_apply(delays[i]);
}

static void device_matched(void *context, IOReturn result, void *sender,
                           IOHIDDeviceRef device) {
    (void)context; (void)result; (void)sender;
    if (!connection_matches_preference(connection_for_device(device),
                                       load_preferred_connection()))
        return;
    fprintf(stderr, "Compatible keyboard connected; scheduling logo restore\n");
    static const double delays[] = DEVICE_RETRY_DELAYS;
    schedule_apply_series(delays, sizeof(delays) / sizeof(*delays));
}

static void power_callback(void *context, io_service_t service,
                           natural_t message_type, void *message_argument) {
    (void)context; (void)service;
    switch (message_type) {
        case kIOMessageCanSystemSleep:
        case kIOMessageSystemWillSleep:
            /* Never hold up sleep; the effect is restored again on wake. */
            IOAllowPowerChange(power_root_port, (long)message_argument);
            break;
        case kIOMessageSystemHasPoweredOn: {
            fprintf(stderr, "Mac woke; scheduling LOGO restore\n");
            static const double delays[] = WAKE_RETRY_DELAYS;
            schedule_apply_series(delays, sizeof(delays) / sizeof(*delays));
            break;
        }
        default:
            break;
    }
}

static void register_for_power_notifications(void) {
    power_root_port = IORegisterForSystemPower(
        NULL, &power_notify_port, power_callback, &power_notifier);
    if (power_root_port != IO_OBJECT_NULL && power_notify_port) {
        CFRunLoopAddSource(CFRunLoopGetCurrent(),
                           IONotificationPortGetRunLoopSource(power_notify_port),
                           kCFRunLoopCommonModes);
    } else {
        fprintf(stderr, "Warning: system wake notifications unavailable\n");
    }
}

int run_daemon(void) {
    /* launchd starts the agent at login, which can precede the user granting
       Input Monitoring, so wait rather than exiting. */
    while (IOHIDCheckAccess(HID_REQUEST_LISTEN_EVENT) != 0) {
        fprintf(stderr, "Waiting for Input Monitoring permission...\n");
        sleep(PERMISSION_POLL_SECONDS);
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

    register_for_power_notifications();
    schedule_apply(STARTUP_DELAY);
    fprintf(stderr, APP_DISPLAY_NAME " background service started\n");
    CFRunLoopRun();
    return 0;
}
