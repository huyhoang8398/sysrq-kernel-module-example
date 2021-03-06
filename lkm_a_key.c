#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/sysrq.h>
#include <linux/poll.h>
#include <linux/wait.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("DO Duy Huy Hoang - Kn");
MODULE_DESCRIPTION("A simple example Linux module.");
MODULE_VERSION("0.01");
#define DEVICE_NAME "lkm_a_key"
#define EXAMPLE_MSG "Hello, World!\n"
#define MSG_BUFFER_LEN 15
#define KEY 65

/* Prototypes for device functions */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static int major_num;
static int device_open_count = 0;
static char msg_buffer[MSG_BUFFER_LEN];
static char *msg_ptr;

static bool can_write = false;
static bool can_read  = false;

static unsigned int etx_poll(struct file * filp, struct poll_table_struct * wait);

/* This structure points to all of the device functions */
static struct file_operations file_ops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release,
	.poll = etx_poll
};


/* poll struct */
/* Dynamic method */
//Initialize wait queue
//init_waitqueue_head(&wait_queue_etx_data);
DECLARE_WAIT_QUEUE_HEAD(wait_queue_etx_data);

/*
 ** This fuction will be called when app calls the poll function
 */
static unsigned int etx_poll(struct file * filp, struct poll_table_struct * wait) {
	__poll_t mask = 0;

	poll_wait(filp, & wait_queue_etx_data, wait);
	pr_info("Poll function\n");

	if (can_read) {
		can_read = false;
		mask |= (POLLIN | POLLRDNORM);
	}

	if (can_write) {
		can_write = false;
		mask |= (POLLOUT | POLLWRNORM);
	}

	return mask;
}


static void sysrq_handle_hello(int key) {
	/* Fill buffer with our message */
	strncpy(msg_buffer, "key A pressed!", MSG_BUFFER_LEN);
	/* Set the msg_ptr to the buffer */
	mdelay(3000);
	strncpy(msg_buffer, EXAMPLE_MSG, MSG_BUFFER_LEN);
}

static const struct sysrq_key_op sysrq_hello_op = {
	.handler    = sysrq_handle_hello,
	.help_msg   = "show-hello-mess(m)",
	.action_msg = "Show hello",
	.enable_mask = SYSRQ_ENABLE_DUMP,
};

/* When a process reads from our device, this gets called. */
static ssize_t device_read(struct file *flip, char *buffer, size_t len, loff_t *offset) {
	int bytes_read = 0;
	/* If we???re at the end, loop back to the beginning */
	if (*msg_ptr == 0) {
		msg_ptr = msg_buffer;
	}
	/* Put data in the buffer */
	while (len && *msg_ptr) {
		/* Buffer is in user data, not kernel, so you can???t just reference
		 * with a pointer. The function put_user handles this for us */
		put_user(*(msg_ptr++), buffer++);
		len--;
		bytes_read++;
	}
	//after 1 read, allow next
	can_read = true;

	//wake up the waitqueue
	wake_up(&wait_queue_etx_data);
	return bytes_read;
}

/* Called when a process tries to write to our device */
static ssize_t device_write(struct file *flip, const char *buffer, size_t len, loff_t *offset) {
	/* This is a read-only device */
	printk(KERN_ALERT "This operation is not supported.\n");
	return -EINVAL;
}

/* Called when a process opens our device */
static int device_open(struct inode *inode, struct file *file) {
	/* If device is open, return busy */
	// if (device_open_count) {
	//     return -EBUSY;
	// }
	device_open_count++;
	try_module_get(THIS_MODULE);
	return 0;
}

/* Called when a process closes our device */
static int device_release(struct inode *inode, struct file *file) {
	/* Decrement the open counter and usage count. Without this, the module would not unload. */
	device_open_count--;
	module_put(THIS_MODULE);
	return 0;
}

static int __init lkm_example_init(void) {
	pr_info("Init success, hello\n");
	register_sysrq_key(KEY, &sysrq_hello_op);

	/* Fill buffer with our message */
	strncpy(msg_buffer, EXAMPLE_MSG, MSG_BUFFER_LEN);
	/* Set the msg_ptr to the buffer */
	msg_ptr = msg_buffer;
	/* Try to register character device */
	major_num = register_chrdev(0, "lkm_a_key", &file_ops);
	if (major_num < 0) {
		printk(KERN_ALERT "Could not register device: %d\n", major_num);
		return major_num;
	} else {
		printk(KERN_INFO "lkm_example module loaded with device major number %d\n", major_num);
		return 0;
	}
}

static void __exit lkm_example_exit(void) {
	unregister_sysrq_key(KEY, &sysrq_hello_op);
	/* Remember ??? we have to clean up after ourselves. Unregister the character device. */
	unregister_chrdev(major_num, DEVICE_NAME);
	printk(KERN_INFO "Goodbye, World!\n");
}

/* Register module functions */
module_init(lkm_example_init);
module_exit(lkm_example_exit);
