// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#define pr_fmt(fmt) "cheezer: " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include "cheeze.h"

#define CHEEZER_MINOR 11

static struct cdev cheezer_cdev;
static DECLARE_WAIT_QUEUE_HEAD(cheezer_wait);

static int cheezer_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int cheezer_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t cheezer_read(struct file *filp, char *buf, size_t count,
			    loff_t * f_pos)
{
	struct cheeze_req *req;

	if (unlikely(count != sizeof(struct cheeze_req))) {
		pr_err("read: size mismatch: %ld vs %ld\n",
			count, sizeof(struct cheeze_req));
		return -EINVAL;
	}

	req = cheeze_pop();

	if (unlikely(copy_to_user(buf, req, count)))
		return -EFAULT;

	return count;
}

static ssize_t cheezer_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct cheeze_req req;

	if (unlikely(count != sizeof(req))) {
		pr_err("read: size mismatch: %ld vs %ld\n",
			count, sizeof(req));
		return -EINVAL;
	}

	if (unlikely(copy_from_user(&req, buf, sizeof(req))))
		return -EFAULT;

	if (unlikely(copy_from_user(req.addr, req.user_buf, req.size)))
		return -EFAULT;

	reqs[req.id].acked = 1;

	return (ssize_t)count;
}

static const struct file_operations cheezer_fops = {
	.read = cheezer_read,
	.write = cheezer_write,
	.open = cheezer_open,
	.release = cheezer_release,
};

void cheezer_cleanup_module(void)
{
	unregister_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZER_MINOR), 1);
	cdev_del(&cheezer_cdev);
	device_destroy(cheeze_chr_class, MKDEV(CHEEZE_CHR_MAJOR, CHEEZER_MINOR));
}

int cheezer_init_module(void)
{
	struct device *cheezer_device;
	int result;

	/*
	 * Register your major, and accept a dynamic number. This is the
	 * first thing to do, in order to avoid releasing other module's
	 * fops in cheezer_cleanup_module()
	 */

	cdev_init(&cheezer_cdev, &cheezer_fops);
	cheezer_cdev.owner = THIS_MODULE;
	result = cdev_add(&cheezer_cdev, MKDEV(CHEEZE_CHR_MAJOR, CHEEZER_MINOR), 1);
	if (result) {
		pr_warn("Failed to add cdev for /dev/cheezer\n");
		goto error1;
	}

	result =
	    register_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZER_MINOR), 1,
				   "/dev/cheezer");
	if (result < 0) {
		pr_warn("can't get major/minor %d/%d\n", CHEEZE_CHR_MAJOR,
			CHEEZER_MINOR);
		goto error2;
	}

	cheezer_device =
	    device_create(cheeze_chr_class, NULL,
			  MKDEV(CHEEZE_CHR_MAJOR, CHEEZER_MINOR), NULL, "cheezer");

	if (IS_ERR(cheezer_device)) {
		pr_warn("Failed to create cheezer device\n");
		goto error3;
	}

	return 0;		/* succeed */

error3:
	unregister_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZER_MINOR), 1);
error2:
	cdev_del(&cheezer_cdev);
error1:

	return result;
}
