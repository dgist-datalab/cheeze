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
#include <linux/completion.h>

#include "cheeze.h"

#define CHEEZE_CHR_MAJOR 235
#define CHEEZE_CHR_MINOR 11

static struct cdev cheeze_chr_cdev;
static DECLARE_WAIT_QUEUE_HEAD(cheeze_chr_wait);

static int cheeze_chr_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int cheeze_chr_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t cheeze_chr_read(struct file *filp, char *buf, size_t count,
			    loff_t * f_pos)
{
	struct cheeze_req *req;

	if (unlikely(count != sizeof(struct cheeze_req))) {
		pr_err("%s: size mismatch: %ld vs %ld\n",
			__func__, count, sizeof(*req));
		WARN_ON(1);
		return -EINVAL;
	}

	req = cheeze_peek();
	if (unlikely(req == NULL)) {
		WARN_ON(1);
		pr_err("%s: failed to peek queue\n", __func__);
		return -ERESTARTSYS;
	}

	if (unlikely(copy_to_user(buf, req, count))) {
		WARN_ON(1);
		pr_err("%s: copy_to_user() failed\n", __func__);
		return -EFAULT;
	}

	return count;
}

static ssize_t cheeze_chr_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct cheeze_req req;

	if (unlikely(count != sizeof(req))) {
		WARN_ON(1);
		pr_err("%s: size mismatch: %ld vs %ld\n",
			__func__, count, sizeof(req));
		return -EINVAL;
	}

	if (unlikely(copy_from_user(&req, buf, sizeof(req)))) {
		WARN_ON(1);
		pr_err("%s: failed to fill req\n", __func__);
		return -EFAULT;
	}

	pr_debug("%s: req[%d]\n"
		"    rw=%d\n"
		"    offset=%u\n"
		"    size=%u\n"
		"    addr=%p\n",
			__func__, req.id, req.rw, req.offset, req.size, req.addr);

	switch (req.rw) {
	case READ:
		if (unlikely(copy_from_user(req.addr, req.user_buf, req.size))) {
			WARN_ON(1);
			pr_err("%s: copy_from_user() failed\n", __func__);
			return -EFAULT;
		}
		break;
	case WRITE:
		if (unlikely(copy_to_user(req.user_buf, req.addr, req.size))) {
			WARN_ON(1);
			pr_err("%s: copy_to_user() failed\n", __func__);
			return -EFAULT;
		}
		break;
	}

	complete(&reqs[req.id].acked);
	pr_debug("%s: reqs[%d].acked = 1\n", __func__, req.id);

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
}

int cheeze_chr_init_module(void)
{
	struct device *cheeze_chr_device;
	int result;


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

	return result;
}
