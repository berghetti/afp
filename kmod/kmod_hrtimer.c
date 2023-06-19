
/*
 * kernel module to provide IPI interface access from kernel to user level
 * */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/hrtimer.h>
#include <asm/processor.h>
#include <asm/current.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 5, 0, 0 )
#include <linux/sched/task_stack.h>  // dependency to task_pt_regs
#endif

#include "kmod_hrtimer.h"

/*
 * HRTIMER_MODE_REL
 * HRTIMER_MODE_REL_SOFT
 * HRTIMER_MODE_REL_HARD
 * HRTIMER_MODE_REL_PINNED
 * HRTIMER_MODE_REL_PINNED_SOFT
 * HRTIMER_MODE_REL_PINNED_HARD
 * */

#define TIMER_MODE HRTIMER_MODE_REL_HARD

// timer interval in ns (20us)
#define INTERVAL 20 * 1000

static struct hrtimer timer;
static ktime_t interval;

// low level entry point to handler in user space;
static unsigned long _trap_entry;

static enum hrtimer_restart
trampoline ( struct hrtimer *timer )
{
  struct pt_regs *regs;
  regs = task_pt_regs ( current );

  // pr_info ( "received timer on core %u\n", smp_processor_id () );

  // reserve space on user stack to put EIP
  regs->sp -= 8;

  // save EIP in user stack (RSP)
  copy_to_user ( ( unsigned long * ) regs->sp, &regs->ip, sizeof ( regs->ip ) );

  // change EIP to when return from kernel go to handler defined in user space
  regs->ip = _trap_entry;

  // rearm timer
  hrtimer_forward_now ( timer, interval );
  return HRTIMER_RESTART;
}

static inline void
start_timer ( void )
{
  hrtimer_start ( &timer, interval, TIMER_MODE );
}

static inline void
stop_timer ( void )
{
  hrtimer_cancel ( &timer );
}

static long
kmod_ioctl ( struct file *filp, unsigned int cmd, unsigned long arg )
{
  switch ( cmd )
    {
      case KMOD_START_TIMER:
        _trap_entry = arg;
        start_timer ();
        pr_info ( "started timer on core %u\n", smp_processor_id () );
        break;
      case KMOD_STOP_TIMER:
        stop_timer ();
        pr_info ( "canceled timer on core %u\n", smp_processor_id () );
        break;
    }

  return 0;
}

static int
kmod_open ( struct inode *inode, struct file *filp )
{
  return 0;
}

static int
kmod_release ( struct inode *inode, struct file *filp )
{
  return 0;
}

static struct file_operations fops = { .owner = THIS_MODULE,
                                       .unlocked_ioctl = kmod_ioctl,
                                       .open = kmod_open,
                                       .release = kmod_release };

static struct class *cls;

static int __init
kmod_start ( void )
{
  int r = register_chrdev ( MAJOR_NUM, KMOD_HRTIMER_NAME, &fops );

  if ( r < 0 )
    {
      pr_alert ( "Error to register character device" KMOD_HRTIMER_NAME
                 ": %d\n",
                 r );
      return r;
    }

  cls = class_create ( THIS_MODULE, KMOD_HRTIMER_NAME );
  device_create ( cls, NULL, MKDEV ( MAJOR_NUM, 0 ), NULL, KMOD_HRTIMER_NAME );

  // setup timer
  hrtimer_init ( &timer, CLOCK_MONOTONIC, TIMER_MODE );
  timer.function = trampoline;
  interval = ktime_set ( 0, INTERVAL );

  pr_info ( KMOD_HRTIMER_NAME " started\n" );

  return 0;
}

static void __exit
kmod_exit ( void )
{
  device_destroy ( cls, MKDEV ( MAJOR_NUM, 0 ) );
  class_destroy ( cls );

  unregister_chrdev ( MAJOR_NUM, KMOD_HRTIMER_NAME );

  // ensure timer disabled
  if ( hrtimer_active ( &timer ) )
    stop_timer ();

  pr_info ( KMOD_HRTIMER_NAME " unloaded\n" );
}

module_init ( kmod_start );
module_exit ( kmod_exit );

MODULE_LICENSE ( "GPL" );
MODULE_DESCRIPTION ( "Provide IPI access interface from kernel to user level" );
