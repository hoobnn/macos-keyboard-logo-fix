#ifndef DAEMON_H
#define DAEMON_H

/* Runs the background service: waits for Input Monitoring, then restores the
   logo effect whenever a compatible keyboard appears or the Mac wakes.
   Blocks on the run loop and does not return under normal operation. */
int run_daemon(void);

#endif
