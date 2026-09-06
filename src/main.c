#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daemon.h"
#include "keyboard.h"
#include "service.h"
#include "ui.h"

#define MIN_DURATION_SECONDS 1
#define MAX_DURATION_SECONDS 300

/* Exit codes, distinguished so callers and launchd logs can tell the causes
   apart. */
#define EXIT_USAGE 2
#define EXIT_NO_PERMISSION 3
#define EXIT_SERVICE_FAILED 4

static void print_usage(const char *program) {
    fprintf(stderr,
            "usage: %s [duration_seconds|--daemon|--uninstall]\n"
            "  (no arguments)     install the background service and pick a connection\n"
            "  duration_seconds   restore the saved logo effect for %d-%d seconds\n"
            "  --daemon           run the background service in the foreground\n"
            "  --uninstall        remove the background service\n",
            program, MIN_DURATION_SECONDS, MAX_DURATION_SECONDS);
}

/* The double-click path: set up the agent, then show the connection picker. */
static int run_app_launch(void) {
    if (install_background_service() != 0) {
        fprintf(stderr, "Failed to install background service\n");
        return EXIT_SERVICE_FAILED;
    }
    if (!ensure_input_monitoring_access()) {
        fprintf(stderr, "Input Monitoring permission is required\n");
        return EXIT_NO_PERMISSION;
    }
    return run_selection_interface();
}

static int run_one_shot(int seconds) {
    if (!ensure_input_monitoring_access()) {
        fprintf(stderr, "Input Monitoring permission is required\n");
        return EXIT_NO_PERMISSION;
    }
    return restore_saved_logo_effect(seconds);
}

int main(int argc, char **argv) {
    if (argc == 1) return run_app_launch();

    if (argc > 2) {
        print_usage(argv[0]);
        return EXIT_USAGE;
    }

    if (strcmp(argv[1], "--daemon") == 0) return run_daemon();
    if (strcmp(argv[1], "--uninstall") == 0) return uninstall_background_service();
    if (strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    int seconds = atoi(argv[1]);
    if (seconds < MIN_DURATION_SECONDS || seconds > MAX_DURATION_SECONDS) {
        fprintf(stderr, "duration must be between %d and %d seconds\n",
                MIN_DURATION_SECONDS, MAX_DURATION_SECONDS);
        return EXIT_USAGE;
    }
    return run_one_shot(seconds);
}
