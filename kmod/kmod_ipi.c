
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/uaccess.h>
#include <asm/processor.h>
#include <asm/current.h>

#include <linux/sched/debug.h>

#include "hello.h"

static void
ipi ( void __user *arg )
{
  // pr_info ( DEVICE_FILE_NAME " received ipi on cpu %d\n", smp_processor_id ()
  // );

  struct pt_regs *regs;
  regs = task_pt_regs ( current );

  // save EIP in user stack (RSP)
  regs->sp -= 8;  // reserve space on user stack to put EIP
  copy_to_user ( ( unsigned long * ) regs->sp, &regs->ip, sizeof ( regs->ip ) );

  // when return from kernel go to handler definied in user space
  regs->ip = ( unsigned long ) arg;
}

static long
kmod_ioctl ( struct file *filp, unsigned int cmd, unsigned long arg )
{
  pr_info ( DEVICE_PATH " received IOCTL on core %d\n", smp_processor_id () );

  smp_call_function_single ( 1, ipi, ( void __user * ) arg, 0 );

  return 1;

  // return -ENOTTY;
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
  int r = register_chrdev ( MAJOR_NUM, DEVICE_FILE_NAME, &fops );

  if ( r < 0 )
    {
      pr_alert ( "Error to register character device" DEVICE_FILE_NAME ": %d\n",
                 r );
      return r;
    }

  cls = class_create ( THIS_MODULE, DEVICE_FILE_NAME );
  device_create ( cls, NULL, MKDEV ( MAJOR_NUM, 0 ), NULL, DEVICE_FILE_NAME );

  pr_info ( DEVICE_FILE_NAME " started\n" );

  return 0;
}

static void __exit
kmod_exit ( void )
{
  device_destroy ( cls, MKDEV ( MAJOR_NUM, 0 ) );
  class_destroy ( cls );

  /* Unregister the device */
  unregister_chrdev ( MAJOR_NUM, DEVICE_FILE_NAME );

  pr_info ( DEVICE_FILE_NAME " unloaded\n" );
}

module_init ( kmod_start );
module_exit ( kmod_exit );

MODULE_LICENSE ( "GPL" );
MODULE_DESCRIPTION ( "A Example" );
