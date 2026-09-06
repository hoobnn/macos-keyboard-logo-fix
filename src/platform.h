#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>

/* Thin wrappers over the POSIX and filesystem primitives the rest of the
   program needs, so the higher layers stay free of fork/exec bookkeeping. */

/* Runs `path` to completion and returns its exit status, or -1 if the process
   could not be started or did not exit normally. */
int run_process(const char *path, char *const arguments[]);

/* Resolves the absolute, symlink-free path of the running executable. */
int executable_path(char *output, size_t output_size);

/* Absolute path to the current user's home directory, or NULL. */
const char *home_directory(void);

/* Joins `format` (a printf template) onto the home directory. Returns -1 if
   the home directory is unknown or the result would not fit. */
int home_relative_path(char *output, size_t output_size,
                       const char *format, ...)
    __attribute__((format(printf, 3, 4)));

/* Creates `path` and every missing parent below the home directory. */
int make_directory(const char *path);

/* Writes `text` to `file` with the five XML predefined entities escaped. */
void xml_write_escaped(FILE *file, const char *text);

#endif
