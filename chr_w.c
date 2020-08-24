// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#define pr_fmt(fmt) "cheezew: " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>

#include "cheeze.h"

#define CHEEZEW_MAJOR 235
#define CHEEZEW_MINOR 12

static struct cdev cheezew_cdev;
static DECLARE_WAIT_QUEUE_HEAD(cheezew_wait);

static int cheezew_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int cheezew_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t cheezew_read(struct file *filp, char *buf, size_t count,
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

static ssize_t cheezew_write(struct file *file, const char __user *buf,
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

static const struct file_operations cheezew_fops = {
	.read = cheezew_read,
	.write = cheezew_write,
	.open = cheezew_open,
	.release = cheezew_release,
};

void cheezew_cleanup_module(void)
{
	unregister_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZEW_MINOR), 1);
	cdev_del(&cheezew_cdev);
	device_destroy(cheeze_chr_class, MKDEV(CHEEZE_CHR_MAJOR, CHEEZEW_MINOR));
}

int cheezew_init_module(void)
{
	struct device *cheezew_device;
	int result;

	/*
	 * Register your major, and accept a dynamic number. This is the
	 * first thing to do, in order to avoid releasing other module's
	 * fops in cheezew_cleanup_module()
	 */

	cdev_init(&cheezew_cdev, &cheezew_fops);
	cheezew_cdev.owner = THIS_MODULE;
	result = cdev_add(&cheezew_cdev, MKDEV(CHEEZE_CHR_MAJOR, CHEEZEW_MINOR), 1);
	if (result) {
		pr_warn("Failed to add cdev for /dev/cheezew\n");
		goto error1;
	}

	result =
	    register_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZEW_MINOR), 1,
				   "/dev/cheezew");
	if (result < 0) {
		pr_warn("can't get major/minor %d/%d\n", CHEEZE_CHR_MAJOR,
			CHEEZEW_MINOR);
		goto error2;
	}

	cheezew_device =
	    device_create(cheeze_chr_class, NULL,
			  MKDEV(CHEEZE_CHR_MAJOR, CHEEZEW_MINOR), NULL, "cheezew");

	if (IS_ERR(cheezew_device)) {
		pr_warn("Failed to create cheezew device\n");
		goto error3;
	}

	return 0;		/* succeed */

error3:
	unregister_chrdev_region(MKDEV(CHEEZE_CHR_MAJOR, CHEEZEW_MINOR), 1);
error2:
	cdev_del(&cheezew_cdev);
error1:

	return result;
}
