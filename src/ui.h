#ifndef UI_H
#define UI_H

/* The double-click experience: ask which connection the background service
   should drive, save the answer, restore the effect once, and report the
   outcome through a notification or alert. Returns 0 on success or when the
   user cancels. */
int run_selection_interface(void);

#endif
