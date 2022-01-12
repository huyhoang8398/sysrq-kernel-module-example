#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/eventfd.h>
#include <linux/sysrq.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include <linux/xenkvm_guest_utils.h>

static DECLARE_WAIT_QUEUE_HEAD(wqh);
static DEFINE_SPINLOCK(xk_lock);
static unsigned long high_watermark;
static int last_event;

struct xkgu_data
{
    spinlock_t lock;
    unsigned long level;
};

static int xkgu_open(struct inode *inode, struct file *filp)
{
    struct xkgu_data *data;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (unlikely(!data))
        return -ENOMEM;

    spin_lock_init(&data->lock);

    filp->private_data = data;

    return 0;
}

static int xkgu_release(struct inode *inode, struct file *filp)
{
    struct xkgu_data *data = filp->private_data;
    BUG_ON(!data);
    filp->private_data = NULL;
    kfree(data);
    return 0;
}

static int get_event(struct xkgu_data *data, bool peek)
{
    int ev = -1;
    unsigned long flags;
    spin_lock_irqsave(&data->lock, flags);
    spin_lock(&xk_lock);
    if (data->level != high_watermark)
    {
        ev = last_event;
        if (!peek)
            data->level = high_watermark;
    }
    spin_unlock(&xk_lock);
    spin_unlock_irqrestore(&data->lock, flags);
    return ev;
}

static ssize_t xkgu_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    struct xkgu_data *data = filp->private_data;
    ssize_t ret;
    int ev = -1;
    DEFINE_WAIT(wait);

    while (1)
    {
        prepare_to_wait(&wqh, &wait, TASK_INTERRUPTIBLE);

        ev = get_event(data, false);
        if (ev >= 0)
            break;

        if (signal_pending(current))
        {
            ret = -ERESTARTSYS;
            goto out;
        }

        schedule();
    }

    if (ev >= 0)
    {
        ret = put_user(ev, (int __user *)buf);
        if (!ret)
            ret = sizeof(ev);
    }

out:
    finish_wait(&wqh, &wait);
    return ret;
}

static __poll_t xkgu_poll(struct file *filp, struct poll_table_struct *wait)
{
    struct xkgu_data *data = filp->private_data;
    int ev;
    poll_wait(filp, &wqh, wait);
    ev = get_event(data, true);
    if (ev >= 0)
        return EPOLLIN | EPOLLRDNORM;
    return 0;
}

static void xkgu_handle_sysrq(int key)
{
    int ev;
    switch (key)
    {
    case 'x':
        ev = XKGU_EVENT0;
        break;
    case 'y':
        ev = XKGU_EVENT1;
        break;
    default:
        return;
    }

    spin_lock(&xk_lock);
    last_event = ev;
    high_watermark++;
    spin_unlock(&xk_lock);

    wake_up_all(&wqh);
}

static const struct file_operations xkgu_fops = {
    .owner = THIS_MODULE,
    .open = xkgu_open,
    .release = xkgu_release,
    .read = xkgu_read,
    .poll = xkgu_poll,
    .llseek = no_llseek,
};

static struct miscdevice xkgu_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "xenkvm_guest_utils",
    .fops = &xkgu_fops,
};

static struct sysrq_key_op xkgu_ev0_ops = {
    .handler = xkgu_handle_sysrq,
    .help_msg = "xkgu-ev0(x)",
    .action_msg = "Trigger xenkvm event 0",
    .enable_mask = SYSRQ_ENABLE_SIGNAL,
};

static struct sysrq_key_op xkgu_ev1_ops = {
    .handler = xkgu_handle_sysrq,
    .help_msg = "xkgu-ev1(y)",
    .action_msg = "Trigger xenkvm event 1",
    .enable_mask = SYSRQ_ENABLE_SIGNAL,
};

static int __init xkgu_init(void)
{
    int rc;

    rc = register_sysrq_key('x', &xkgu_ev0_ops);
    if (rc < 0)
        goto err_x;

    rc = register_sysrq_key('y', &xkgu_ev1_ops);
    if (rc < 0)
        goto err_y;

    rc = misc_register(&xkgu_dev);
    if (rc < 0)
        goto err_misc;

    return rc;

err_misc:
    unregister_sysrq_key('y', &xkgu_ev1_ops);
err_y:
    unregister_sysrq_key('x', &xkgu_ev0_ops);
err_x:
    return rc;
}
module_init(xkgu_init);

static void __exit xkgu_exit(void)
{
    unregister_sysrq_key('x', &xkgu_ev0_ops);
    unregister_sysrq_key('y', &xkgu_ev1_ops);
    misc_deregister(&xkgu_dev);
}
module_exit(xkgu_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Xenkvm guest utility driver");
MODULE_AUTHOR("Xenkvm team");
