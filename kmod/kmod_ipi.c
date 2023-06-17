
/*
 * kernel module to provide IPI interface access from kernel to user level
 * */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <asm/processor.h>
#include <asm/current.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION( 5, 0, 0 )
#include <linux/sched/task_stack.h>  // dependency to task_pt_regs
#endif

#include "kmod_ipi.h"

static void
ipi ( void __user *arg )
{
  struct pt_regs *regs;
  regs = task_pt_regs ( current );

  // pr_info ( "trap entry %lx\n", ( unsigned long ) arg );
  // reserve space on user stack to put EIP
  regs->sp -= 8;

  // save EIP in user stack (RSP)
  copy_to_user ( ( unsigned long * ) regs->sp, &regs->ip, sizeof ( regs->ip ) );

  // change EIP to when return from kernel go to handler defined in user space
  regs->ip = ( unsigned long ) arg;
}

static long
kmod_ioctl ( struct file *filp, unsigned int cmd, unsigned long arg )
{
  struct req_ipi req;
  copy_from_user ( &req, ( void __user * ) arg, sizeof ( req ) );

  smp_call_function_single ( req.core, ipi, req._trap_entry, 0 );

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
  int r = register_chrdev ( MAJOR_NUM, KMOD_IPI_NAME, &fops );

  if ( r < 0 )
    {
      pr_alert ( "Error to register character device" KMOD_IPI_NAME ": %d\n",
                 r );
      return r;
    }

  cls = class_create ( THIS_MODULE, KMOD_IPI_NAME );
  device_create ( cls, NULL, MKDEV ( MAJOR_NUM, 0 ), NULL, KMOD_IPI_NAME );

  pr_info ( KMOD_IPI_NAME " started\n" );

  return 0;
}

static void __exit
kmod_exit ( void )
{
  device_destroy ( cls, MKDEV ( MAJOR_NUM, 0 ) );
  class_destroy ( cls );

  unregister_chrdev ( MAJOR_NUM, KMOD_IPI_NAME );

  pr_info ( KMOD_IPI_NAME " unloaded\n" );
}

module_init ( kmod_start );
module_exit ( kmod_exit );

MODULE_LICENSE ( "GPL" );
MODULE_DESCRIPTION ( "Provide IPI access interface from kernel to user level" );
