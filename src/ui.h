#ifndef UI_H
#define UI_H

/* The double-click experience: ask which connection the background service
   should drive, save the answer, restore the effect once, and report the
   outcome through a notification or alert. Returns 0 on success or when the
   user cancels. */
int run_selection_interface(void);

/* Explains that Input Monitoring is missing and offers to open the matching
   System Settings pane. Called when the permission check fails on a path the
   user reached by double-clicking, where stderr would go unseen. */
void show_permission_required_dialog(void);

#endif
