
#ifndef KMOD_IPI_H
#define KMOD_IPI_H

#include <linux/ioctl.h>

#define KMOD_HRTIMER_NAME "kmod_hrtimer"
#define KMOD_HRTIMER_PATH "/dev/kmod_hrtimer"

#define MAJOR_NUM 281

// IOCTLs
#define KMOD_START_TIMER _IOW ( MAJOR_NUM, 0, unsigned long )
#define KMOD_STOP_TIMER _IO ( MAJOR_NUM, 1 )

#endif
