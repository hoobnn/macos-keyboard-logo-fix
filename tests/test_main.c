/* Unit tests for the parts of the tool that need neither a keyboard nor
   Input Monitoring: preference matching, path building, XML escaping and the
   process helpers.

   Deliberately not covered: load/save_preferred_connection. They resolve the
   home directory through getpwuid, not $HOME, so a test could only exercise
   them by writing into the real ~/Library/Application Support. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "app_config.h"
#include "keyboard.h"
#include "platform.h"
#include "settings.h"

static int failures = 0;
static int checks = 0;

#define CHECK(condition)                                                     \
    do {                                                                     \
        checks++;                                                            \
        if (!(condition)) {                                                  \
            failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,          \
                    #condition);                                             \
        }                                                                    \
    } while (0)

#define CHECK_STR(actual, expected)                                          \
    do {                                                                     \
        checks++;                                                            \
        const char *a_ = (actual), *e_ = (expected);                         \
        if (strcmp(a_, e_) != 0) {                                           \
            failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__,      \
                    __LINE__, a_, e_);                                       \
        }                                                                    \
    } while (0)

static void test_connection_matching(void) {
    /* A device that is not a supported keyboard never matches. */
    CHECK(!connection_matches_preference(KEYBOARD_CONNECTION_NONE,
                                         KEYBOARD_CONNECTION_NONE));
    CHECK(!connection_matches_preference(KEYBOARD_CONNECTION_NONE,
                                         KEYBOARD_CONNECTION_WIRED));

    /* "auto" accepts every supported connection. */
    CHECK(connection_matches_preference(KEYBOARD_CONNECTION_WIRED,
                                        KEYBOARD_CONNECTION_NONE));
    CHECK(connection_matches_preference(KEYBOARD_CONNECTION_BLE,
                                        KEYBOARD_CONNECTION_NONE));

    /* A specific preference accepts only itself. */
    CHECK(connection_matches_preference(KEYBOARD_CONNECTION_WIRED,
                                        KEYBOARD_CONNECTION_WIRED));
    CHECK(!connection_matches_preference(KEYBOARD_CONNECTION_BLE,
                                         KEYBOARD_CONNECTION_WIRED));
    CHECK(connection_matches_preference(KEYBOARD_CONNECTION_BLE,
                                        KEYBOARD_CONNECTION_BLE));
    CHECK(!connection_matches_preference(KEYBOARD_CONNECTION_WIRED,
                                         KEYBOARD_CONNECTION_BLE));
}

static void test_connection_names(void) {
    CHECK_STR(connection_display_name(KEYBOARD_CONNECTION_NONE), "auto");
    CHECK_STR(connection_display_name(KEYBOARD_CONNECTION_WIRED), "USB wired");
    CHECK_STR(connection_display_name(KEYBOARD_CONNECTION_BLE), "Bluetooth LE");
}

static void test_home_relative_path(void) {
    const char *home = home_directory();
    CHECK(home != NULL);
    if (!home) return;

    char path[PATH_BUFFER_SIZE], expected[PATH_BUFFER_SIZE];
    CHECK(home_relative_path(path, sizeof(path),
                             "Library/LaunchAgents/%s.plist", SERVICE_LABEL) == 0);
    snprintf(expected, sizeof(expected), "%s/Library/LaunchAgents/%s.plist",
             home, SERVICE_LABEL);
    CHECK_STR(path, expected);

    /* A result that would not fit is refused rather than truncated. */
    char tiny[8];
    CHECK(home_relative_path(tiny, sizeof(tiny), "Library") == -1);
}

static void test_xml_escaping(void) {
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);
    CHECK(stream != NULL);
    if (!stream) return;

    xml_write_escaped(stream, "a&b <c> \"d\" 'e' /Users/x y");
    fclose(stream);
    CHECK_STR(buffer,
              "a&amp;b &lt;c&gt; &quot;d&quot; &apos;e&apos; /Users/x y");
    free(buffer);
}

static void test_process_helpers(void) {
    char *echo[] = {"/bin/echo", "hello", "world", NULL};
    char output[64];
    CHECK(capture_process_output("/bin/echo", echo, output, sizeof(output)));
    /* Only the first line, without its newline. */
    CHECK_STR(output, "hello world");

    char *fail[] = {"/usr/bin/false", NULL};
    CHECK(run_process("/usr/bin/false", fail) == 1);
    CHECK(!capture_process_output("/usr/bin/false", fail, output, sizeof(output)));

    char *ok[] = {"/usr/bin/true", NULL};
    CHECK(run_process("/usr/bin/true", ok) == 0);

    /* A missing binary exits 127 from the forked child. */
    char *missing[] = {"/nonexistent/binary", NULL};
    CHECK(run_process("/nonexistent/binary", missing) == 127);
}

static void test_make_directory(void) {
    char root[] = "/tmp/keyboard-logo-fix-test.XXXXXX";
    CHECK(mkdtemp(root) != NULL);

    char nested[PATH_BUFFER_SIZE];
    snprintf(nested, sizeof(nested), "%s/settings", root);
    CHECK(make_directory(nested) == 0);
    /* An existing directory is success, as on every launch after the first. */
    CHECK(make_directory(nested) == 0);

    struct stat info;
    CHECK(stat(nested, &info) == 0 && S_ISDIR(info.st_mode));

    /* A regular file in the way is an error, not success. */
    char file[PATH_BUFFER_SIZE];
    snprintf(file, sizeof(file), "%s/file", root);
    FILE *handle = fopen(file, "w");
    CHECK(handle != NULL);
    if (handle) fclose(handle);
    CHECK(make_directory(file) == -1);

    unlink(file);
    rmdir(nested);
    rmdir(root);
}

int main(void) {
    test_connection_matching();
    test_connection_names();
    test_home_relative_path();
    test_xml_escaping();
    test_process_helpers();
    test_make_directory();

    if (failures) {
        fprintf(stderr, "%d of %d checks failed\n", failures, checks);
        return 1;
    }
    printf("All %d checks passed\n", checks);
    return 0;
}
