#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Application identity and the identifiers inherited from v0.1.x, which are
   still read so existing installs keep their settings across the rename. */

#define APP_DISPLAY_NAME "Keyboard Logo Fix"

#define SERVICE_LABEL "com.ikuyu.keyboard-logo-fix"
#define LEGACY_SERVICE_LABEL "local.codex.t100-logo-white"

#define SETTINGS_DIRECTORY "KeyboardLogoFix"
#define LEGACY_SETTINGS_DIRECTORY "T100Logo"

#define LOG_FILE_NAME "KeyboardLogoFix.log"

/* Paths are assembled into fixed buffers of this size throughout. */
#define PATH_BUFFER_SIZE 4096

#endif
