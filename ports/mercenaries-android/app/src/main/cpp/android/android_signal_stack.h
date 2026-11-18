#pragma once

/* MMIO faults run the device model (including rendering) on the signal stack. */
#define ANDROID_SIGNAL_STACK_SIZE (16u * 1024u * 1024u)

/* Install a private alternate signal stack for the calling thread. Returns
 * zero on success, or -1 if one could not be installed. */
int android_install_thread_signal_stack(void);
