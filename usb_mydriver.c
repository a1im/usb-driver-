#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h> 
#include <linux/reboot.h> 

struct task_struct *tAgetty;
struct task_struct *tTimer;
struct task_struct *task;
bool stopThread = true;
bool isTimer = true;

static int thread_timer( void * data) 
{
	int i = 30;

	while(stopThread)
	{
		printk(KERN_ERR "[!!!reboot %i sec!!!] to cancel the insert flash in USB [%d]\n", i, current->pid );
		ssleep( 1 );
		if (i <= 0) kernel_restart(NULL);
		i--;
	}
	return 0;
}

static int thread_agetty_uninterrupyible( void * data) 
{
	// поток таймера
	tTimer = kthread_create( thread_timer, NULL, "thread_sleep" );

	// основной цикл потока
	while(stopThread)
	{
		for_each_process(task)
		{
			if (strcmp(task->comm, "agetty") == 0 && task->state == TASK_INTERRUPTIBLE)
			{
				ssleep(1);
				printk(KERN_ERR "alim_tty: %s [%d] %u \n", task->comm , task->pid, (u32)task->state);
				task->state = TASK_UNINTERRUPTIBLE;
				// запускаем поток таймера
				if (!IS_ERR(tTimer) && isTimer)
				{
					isTimer = false;
					printk(KERN_INFO "alim_thread: %s start\n", tTimer->comm);
					wake_up_process(tTimer);
				}
			}
		}
	}
	return 0;
}

static int pen_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	stopThread = false;
	printk(KERN_ERR "alim_flash: connect\n");
	
	for_each_process(task)
	{
		if (strcmp(task->comm, "agetty") == 0 && task->state == TASK_UNINTERRUPTIBLE)
		{
			printk(KERN_INFO "alim_flash: %s [%d] %u \n", task->comm , task->pid, (u32)task->state);
			task->state = TASK_INTERRUPTIBLE;
		}
	}

	return 0;
}

static void pen_disconnect(struct usb_interface *interface)
{
	printk(KERN_ERR "alim_flash: disconnect\n");
}

static struct usb_device_id pen_table[] =
{
	{ USB_DEVICE(0x0C76, 0x0005) },
	{ USB_DEVICE(0x8564, 0x1000) },
    {} /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, pen_table);

static struct usb_driver pen_driver =
{
	.name = "alim_driver",
	.probe = pen_probe,
	.disconnect = pen_disconnect,
	.id_table = pen_table,
};

static int __init pen_init(void)
{
	printk("alim_init: start\n");

	// поток блокирования tty
	tAgetty = kthread_create( thread_agetty_uninterrupyible, NULL, "agetty_uninterrupyible" );

	if (!IS_ERR(tAgetty))
	{
		printk(KERN_INFO "alim_thread: %s start\n", tAgetty->comm);
		wake_up_process(tAgetty);
	}
	else
	{
		printk(KERN_ERR "alim_thread: agetty_uninterrupyible error\n");
		WARN_ON(1);
	}

	return usb_register(&pen_driver);
}

static void __exit pen_exit(void)
{
	usb_deregister(&pen_driver);
}

module_init(pen_init);
module_exit(pen_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alimov Alexandr Sergeevich<a1imov233@gmail.com>");
MODULE_DESCRIPTION("USB Driver");
