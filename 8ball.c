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
#define MSG_LEN 80 /* Length of user message */
#define NUM_CHOICES 10

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

static char msg[MSG_LEN]; /* Hold the user's question */

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
	
	/* Decrement usage count */
	module_put(THIS_MODULE);
	
	return 0;
}

/* Called when a process, which already opened dev file, tries to read */
static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset){
	
	int bytes_read = 0;
	int decision = 0;
	char *msg_ptr;

	for(int i = 0; i < MSG_LEN; i++){
		decision += msg[i];
	}

	decision %= NUM_CHOICES;

	switch(decision){
		case 0:
			msg_ptr = "Yes.\n";
			break;

		case 1:
			msg_ptr = "Without a doubt.\n";
			break;

		case 2:
			msg_ptr = "You're better off not knowing.\n";
			break;

		case 3:
			msg_ptr = "YES YES YES!!!\n";
			break;

		case 4:
			msg_ptr = "Concentrate and ask again\n";
			break;

		case 5:
			msg_ptr = "No.\n";
			break;

		case 6:
			msg_ptr = "NO NO NO!!!\n";
			break;

		case 7:
			msg_ptr = "Not with that attitude.\n";
			break;

		case 8:
			msg_ptr = "That knowledge is kept even from me.\n";
			break;

		case 9:
			msg_ptr = "Signs point to yes.\n";
			break;
	}

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

	if(*offset >= MSG_LEN){ /* If buffer is full */
		*offset = 0; /* Reset buffer ptr */
		return 0;
	}

	ssize_t len = min(length, (size_t)(MSG_LEN - *offset)); /* Read up to remaining buffer size */
	
	/* Read question from user */
	if(copy_from_user(msg + *offset, buffer, len))
		/* copy_from_user returns number of bytes that it could *not* read from user */
		return -EFAULT;

	*offset += len;

	return len; 
	/* The return value represents how many bytes were read. For the userspace
	 * write() call, it will keep trying until it sends all data 
	 * (looping infinitely if you return 0)
	 */
}

module_init(ball_init);
module_exit(ball_exit);

MODULE_LICENSE("GPL");
