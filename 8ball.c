/*
 *  8ball.c -- An 8ball char device 
*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/fs.h> 
#include <linux/device.h>
#include <linux/version.h>

#define DEV_NAME "8ball"

/* Driver prototypes */
/* inode represents the underlying file, whereas file struct represents
 * an open file descriptor. There can be numerous file structs representing open
 * fds (from multiple devices accessing the same file), but they ALL point to
 * the same underlying inode.
 */
/* Used to open/close a fd */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
/* Used to interact with the open fd */
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);

/* Function declarations don't necessarily need the argument name: they wouldn't
 * be using them anyway. This might be useful to declare a function protoype
 * with descriptive arg names, but use more concise names in the definition 
 */

/* Global variables are declared static, so they are only global to this file */
static int major; 
static struct class *cls;
static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release,
};

/* Used to only allow one process to access the device at a time (only one can
 * open the device file)
 */
enum {
	CDEV_NOT_USED = 0,
	CDEV_USED = 1,
};

/* Is device open? */
static atomic_t dev_open = ATOMIC_INIT(CDEV_NOT_USED);

static int __init ball_init(void)
{
    /* Registers the device driver and adds its ID (major number) to /proc/devices */
    major = register_chrdev(0, DEV_NAME, &fops);

    if(major < 0){
        pr_alert("Failure registering device: %d\n", major);
		class_destroy(cls);
        return major;
    }

    pr_info("Got device number %d!\n", major);
	
/* Similar physical devices are grouped up into device classes.
 * This class will expose the same interface for all these devices (to the userspace).
 * BUT the underlying implementation can vary depending on the driver used.
 * Multiple drivers can be used within a class for a varying amount of devices
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	cls = class_create(DEV_NAME);
#else 
	cls = class_create(THIS_MODULE, DEV_NAME);
#endif
	/* MKDEV is a macro that just bitshifts the major/minor values in an int.
	 * This serves as the ID of this device (and why this macro is used for deletion)
	*/
	device_create(cls, NULL, MKDEV(major, 0), NULL, DEV_NAME);

	/* Device file is created under /dev, and another file will be under /sys/class (symlink to /dev?) */
	pr_info("Created device on /dev/%s\n", DEV_NAME);

    return 0;
}

static void __exit ball_exit(void)
{
	/* Unregister driver */
    unregister_chrdev(major, DEV_NAME);
	
	/* Unregister device */
	device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);

    pr_alert("Removing device: %d\n", major);
}

/* Methods */

/* Called when process opens device (creates new fd) */
static int device_open(struct inode *, struct file *)
{	
	/* This static variable is given an initialization, which happens only once. Later calls will not reinitalize, as this value has lifetime duration (static)
	 * If we were to instead:
	 * static int a;
	 * a = 0;
	 * The second line is just a statement, which would set a to 0 every time!
	 */
	static int counter = 0;			

	/* Performs atomic compare-and exchange:
	 * 1. ptr to atomic variable (to modify)
	 * 2. value you expect
	 * 3. value you want to set
	 * If curr value == expected, set to new. If not, do nothing
	 * returns value of atomic variable
	 * If this value is 0, this means we didn't modify (no access). 
	 * If it is 1, we modified (have access)
	 */
	if(atomic_cmpxchg(&dev_open, CDEV_NOT_USED, CDEV_USED))
		return -EBUSY; /* Device is currently busy */
	
	pr_info("You have requested for the 8ball's presence %d times...\n", ++counter);

	/* Increments a counter that represents how many devices are using this
	 * device (used to prevent a rmmod when module is in use)
	 */
	try_module_get(THIS_MODULE);
	return 0;
}

/* Called when process closes device file */
static int device_release(struct inode *, struct file *)
{
	/* Now ready for next caller */
	atomic_set(&dev_open, CDEV_NOT_USED);
	
	pr_info("8ball is done...\n");

	/* Decrement usage count */
	module_put(THIS_MODULE);
	
	return 0;
}

/* Called when a process, which already opened dev file, tries to read */
static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset){
	
	int bytes_read = 0;
	char *msg_ptr = "hehe that tickles :>\n";

	if(!*(msg_ptr + *offset)){ /* If we are at end of message already */
		*offset = 0; /* Reset offset */
		return 0; 
	}

	msg_ptr += *offset; /* Set up to read starting from offset */

	while(length && *msg_ptr){ /* While user buffer has space && we have data to write */
		put_user(*(msg_ptr++), buffer++);
		length--;
		bytes_read++;
	}

	*offset += bytes_read;

	return bytes_read;
}

/* Called when a process tries to write to file */
static ssize_t device_write(struct file *filep, const char __user *buffer, size_t length, loff_t *offset)
{
	pr_info("Keep yapping man\n");
	return length; 
	/* The return value represents how many bytes were read. For the userspace
	 * write() call, it will keep trying until it sends all data 
	 * (looping infinitely if you return 0)
	 */
}

module_init(ball_init);
module_exit(ball_exit);

MODULE_LICENSE("GPL");
