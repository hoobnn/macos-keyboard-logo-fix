#include "platform.h"

#include <mach-o/dyld.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "app_config.h"

extern char **environ;

int run_process(const char *path, char *const arguments[]) {
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

bool capture_process_output(const char *path, char *const arguments[],
                            char *output, size_t output_size) {
    int channel[2];
    if (pipe(channel) != 0) return false;

    pid_t child = fork();
    if (child < 0) {
        close(channel[0]);
        close(channel[1]);
        return false;
    }
    if (child == 0) {
        close(channel[0]);
        dup2(channel[1], STDOUT_FILENO);
        close(channel[1]);
        execve(path, arguments, environ);
        _exit(127);
    }

    close(channel[1]);
    ssize_t length = read(channel[0], output, output_size - 1);
    close(channel[0]);

    int status = 0;
    if (waitpid(child, &status, 0) < 0) return false;
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0 || length <= 0)
        return false;

    output[length] = '\0';
    output[strcspn(output, "\n")] = '\0';
    return true;
}

int executable_path(char *output, size_t output_size) {
    uint32_t size = (uint32_t)output_size;
    if (_NSGetExecutablePath(output, &size) != 0) return -1;
    char resolved[PATH_BUFFER_SIZE];
    if (!realpath(output, resolved)) return -1;
    if (strlen(resolved) + 1 > output_size) return -1;
    strcpy(output, resolved);
    return 0;
}

const char *home_directory(void) {
    struct passwd *account = getpwuid(getuid());
    return account ? account->pw_dir : NULL;
}

int home_relative_path(char *output, size_t output_size,
                       const char *format, ...) {
    const char *home = home_directory();
    if (!home) return -1;

    char relative[PATH_BUFFER_SIZE];
    va_list arguments;
    va_start(arguments, format);
    int relative_length = vsnprintf(relative, sizeof(relative), format, arguments);
    va_end(arguments);
    if (relative_length < 0 || relative_length >= (int)sizeof(relative)) return -1;

    if (snprintf(output, output_size, "%s/%s", home, relative) >= (int)output_size)
        return -1;
    return 0;
}

int make_directory(const char *path) {
    if (mkdir(path, 0755) == 0) return 0;
    /* An existing directory is the expected outcome on every run but the first. */
    struct stat info;
    if (stat(path, &info) == 0 && S_ISDIR(info.st_mode)) return 0;
    return -1;
}

void xml_write_escaped(FILE *file, const char *text) {
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
