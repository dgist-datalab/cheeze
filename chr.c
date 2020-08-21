// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#define pr_fmt(fmt) "cheeze_chr: " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include "cheeze.h"

#define CHEEZE_CHR_MAJOR 235
#define CHEEZE_CHR_MINOR 11

static struct cdev cheeze_chr_cdev;
static struct class *cheeze_chr_class;
static DECLARE_WAIT_QUEUE_HEAD(cheeze_chr_wait);

#if 0
struct cheeze_chr_state {
	struct semaphore sem;	/* Semaphore on the state structure */

	u8 S[256];		/* The state array */
	u8 i;
	u8 j;

	char *buf;
};
#endif

static int cheeze_chr_open(struct inode *inode, struct file *filp)
{
//	struct cheeze_chr_state *state;

	int num = iminor(inode);

	/*
	 * This should never happen, now when the minors are regsitered
	 * explicitly
	 */
	if (num != CHEEZE_CHR_MINOR)
		return -ENODEV;

//	state = kmalloc(sizeof(struct cheeze_chr_state), GFP_KERNEL);
//	if (!state)
//		return -ENOMEM;

//	sema_init(&state->sem, 1);	/* Init semaphore as a mutex */

//	filp->private_data = state;

	return 0;		/* Success */
}

static int cheeze_chr_release(struct inode *inode, struct file *filp)
{
//	struct cheeze_chr_state *state = filp->private_data;

//	kfree(state->buf);
//	kfree(state);

	return 0;
}

static ssize_t cheeze_chr_read(struct file *filp, char *buf, size_t count,
			    loff_t * f_pos)
{
//	struct cheeze_chr_state *state = filp->private_data;
	struct cheeze_req *req;
	int dobytes, k;
	char *localbuf;

	unsigned int i;
	unsigned int j;
	u8 *S;

//	if (unlikely(down_interruptible(&state->sem)))
//		return -ERESTARTSYS;

	if (unlikely(count != sizeof(struct cheeze_req))) {
		pr_err("read: size mismatch: %ld vs %ld\n",
			count, sizeof(struct cheeze_req));
		return -EINVAL;
	}

	if (unlikely(copy_to_user(buf, cheeze_pop(), count)))
		return -EFAULT;

//	up(&state->sem);
	return count;
}

static ssize_t cheeze_chr_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct cheeze_req req;
//	unsigned long id;

/*
	if (unlikely(count != sizeof(id))) {
		pr_err("read: size mismatch: %ld vs %ld\n",
			count, sizeof(id));
		return -EINVAL;
	}
*/
	pr_info("write: count=%lu\n", count);
	if (unlikely(copy_from_user(&req, buf, sizeof(req))))
		return -EFAULT;

	pr_info("%s: id = %lu\n", __func__, req.id);

	if (unlikely(copy_from_user(req.addr, buf + sizeof(struct cheeze_req), req.size)))
		return -EFAULT;

	reqs[req.id].acked = 1;

	return (ssize_t)count;
}

static const struct file_operations cheeze_chr_fops = {
	.read = cheeze_chr_read,
	.write = cheeze_chr_write,
	.open = cheeze_chr_open,
	.release = cheeze_chr_release,
};

void cheeze_chr_cleanup_module(void)
{
	unregister_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZE_CHR_MINOR), 1);
	cdev_del(&cheeze_chr_cdev);
	device_destroy(cheeze_chr_class, MKDEV(CHEEZE_CHR_MAJOR, CHEEZE_CHR_MINOR));
	class_destroy(cheeze_chr_class);
}

int cheeze_chr_init_module(void)
{
	struct device *cheeze_chr_device;
	int result;

	cheeze_chr_class = class_create(THIS_MODULE, "cheeze_chr");
	if (IS_ERR(cheeze_chr_class)) {
		result = PTR_ERR(cheeze_chr_class);
		pr_warn("Failed to register class cheeze_chr\n");
		goto error0;
	}

	/*
	 * Register your major, and accept a dynamic number. This is the
	 * first thing to do, in order to avoid releasing other module's
	 * fops in cheeze_chr_cleanup_module()
	 */

	cdev_init(&cheeze_chr_cdev, &cheeze_chr_fops);
	cheeze_chr_cdev.owner = THIS_MODULE;
	result = cdev_add(&cheeze_chr_cdev, MKDEV(CHEEZE_CHR_MAJOR, CHEEZE_CHR_MINOR), 1);
	if (result) {
		pr_warn("Failed to add cdev for /dev/cheeze_chr\n");
		goto error1;
	}

	result =
	    register_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZE_CHR_MINOR), 1,
				   "/dev/cheeze_chr");
	if (result < 0) {
		pr_warn("can't get major/minor %d/%d\n", CHEEZE_CHR_MAJOR,
			CHEEZE_CHR_MINOR);
		goto error2;
	}

	cheeze_chr_device =
	    device_create(cheeze_chr_class, NULL,
			  MKDEV(CHEEZE_CHR_MAJOR, CHEEZE_CHR_MINOR), NULL, "cheeze_chr");

	if (IS_ERR(cheeze_chr_device)) {
		pr_warn("Failed to create cheeze_chr device\n");
		goto error3;
	}

	return 0;		/* succeed */

error3:
	unregister_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZE_CHR_MINOR), 1);
error2:
	cdev_del(&cheeze_chr_cdev);
error1:
	class_destroy(cheeze_chr_class);
error0:

	return result;
}