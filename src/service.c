#include "service.h"

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "app_config.h"
#include "platform.h"

#define LAUNCHCTL_PATH "/bin/launchctl"

static int launch_agent_path(const char *label, char *path, size_t path_size) {
    return home_relative_path(path, path_size,
                              "Library/LaunchAgents/%s.plist", label);
}

static void launchctl_domain(char *output, size_t output_size) {
    snprintf(output, output_size, "gui/%u", getuid());
}

static int remove_background_service_named(const char *label) {
    char plist_path[PATH_BUFFER_SIZE], domain[64];
    if (launch_agent_path(label, plist_path, sizeof(plist_path)) != 0) return 1;
    launchctl_domain(domain, sizeof(domain));
    char *bootout[] = {"launchctl", "bootout", domain, plist_path, NULL};
    run_process(LAUNCHCTL_PATH, bootout);
    if (unlink(plist_path) != 0 && access(plist_path, F_OK) == 0) return 1;
    return 0;
}

/* Writes the agent plist atomically so a crash mid-write cannot leave launchd
   with a truncated file. */
static int write_agent_plist(const char *plist_path, const char *executable,
                             const char *log_path) {
    char temporary[PATH_BUFFER_SIZE];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", plist_path)
        >= (int)sizeof(temporary))
        return 1;

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

    if (fclose(file) != 0) return 1;
    return rename(temporary, plist_path) == 0 ? 0 : 1;
}

int install_background_service(void) {
    char executable[PATH_BUFFER_SIZE], plist_path[PATH_BUFFER_SIZE];
    char log_path[PATH_BUFFER_SIZE], directory[PATH_BUFFER_SIZE];
    if (executable_path(executable, sizeof(executable)) != 0 ||
        launch_agent_path(SERVICE_LABEL, plist_path, sizeof(plist_path)) != 0 ||
        home_relative_path(log_path, sizeof(log_path),
                           "Library/Logs/%s", LOG_FILE_NAME) != 0)
        return 1;

    if (home_relative_path(directory, sizeof(directory),
                           "Library/LaunchAgents") == 0)
        make_directory(directory);
    if (home_relative_path(directory, sizeof(directory), "Library/Logs") == 0)
        make_directory(directory);

    /* Stop the v0.1.x service before installing the renamed one, so the two
       do not drive the same keyboard at once. */
    remove_background_service_named(LEGACY_SERVICE_LABEL);

    if (write_agent_plist(plist_path, executable, log_path) != 0) return 1;

    char domain[64], service_target[128];
    launchctl_domain(domain, sizeof(domain));
    snprintf(service_target, sizeof(service_target), "%s/%s", domain, SERVICE_LABEL);
    char *bootout[] = {"launchctl", "bootout", domain, plist_path, NULL};
    char *bootstrap[] = {"launchctl", "bootstrap", domain, plist_path, NULL};
    char *enable[] = {"launchctl", "enable", service_target, NULL};

    run_process(LAUNCHCTL_PATH, bootout);
    if (run_process(LAUNCHCTL_PATH, bootstrap) != 0) return 1;
    run_process(LAUNCHCTL_PATH, enable);
    fprintf(stderr, "Background service installed: %s\n", plist_path);
    return 0;
}

int uninstall_background_service(void) {
    int current_result = remove_background_service_named(SERVICE_LABEL);
    int legacy_result = remove_background_service_named(LEGACY_SERVICE_LABEL);
    fprintf(stderr, APP_DISPLAY_NAME " background service removed\n");
    return current_result == 0 && legacy_result == 0 ? 0 : 1;
}
