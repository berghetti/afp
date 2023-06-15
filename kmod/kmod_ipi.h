
#ifndef KMOD_IPI_H
#define KMOD_IPI_H

#include <linux/ioctl.h>

#define DEVICE_FILE_NAME "kmod_ipi"
#define DEVICE_PATH "/dev/kmod_ipi"

#define MAJOR_NUM 280

#define KMOD_IPI_SEND _IOW ( MAJOR_NUM, 0, void * )

#endif
