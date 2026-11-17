#pragma once

/* Install a private alternate signal stack for the calling thread. Returns
 * zero on success, or -1 if one could not be installed. */
int android_install_thread_signal_stack(void);
