#ifndef SERVICE_H
#define SERVICE_H

/* Installation and removal of the per-user LaunchAgent that runs --daemon. */

/* Writes ~/Library/LaunchAgents/<label>.plist and bootstraps it, replacing any
   previous copy and the v0.1.x agent. Returns 0 on success. */
int install_background_service(void);

/* Removes the current and v0.1.x agents. Returns 0 if both are gone. */
int uninstall_background_service(void);

#endif
