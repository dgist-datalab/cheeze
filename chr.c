// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#define pr_fmt(fmt) "cheeze_chr: " fmt

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/completion.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/backing-dev.h>
#include <linux/blk-mq.h>

#include "cheeze.h"

#define CHEEZE_CHR_MAJOR 235
#define CHEEZE_CHR_MINOR 11

static struct cdev cheeze_chr_cdev;
static DECLARE_WAIT_QUEUE_HEAD(cheeze_chr_wait);

static int do_request(struct cheeze_req *req)
{
	int rw = 0;
	unsigned long b_len = 0;
	struct bio_vec bvec;
	struct req_iterator iter;
	loff_t off = 0;
	void *b_buf;
	struct request *rq;
	unsigned int *nr_bytes;

	rq = req->rq;
	nr_bytes = req->nr_bytes;

	switch (req_op(rq)) {
	case REQ_OP_FLUSH:
		pr_warn("ignoring REQ_OP_FLUSH\n");
		WARN_ON(1);
		return -EIO;
		break;
	case REQ_OP_WRITE_ZEROES:
		pr_warn("ignoring REQ_OP_WRITE_ZEROES\n");
		WARN_ON(1);
		return -EIO;
		break;
	case REQ_OP_DISCARD:
		pr_warn("ignoring REQ_OP_DISCARD\n");
		WARN_ON(1);
		return -EIO;
		break;
	case REQ_OP_WRITE:
		rw = 1;
		/* fallthrough */
	case REQ_OP_READ:
		break;
	default:
		WARN_ON(1);
		return -EIO;
		break;
	}

	pr_debug("%s++\n", __func__);

	/* Iterate over all requests segments */
	rq_for_each_segment(bvec, rq, iter) {
		b_len = bvec.bv_len;

		/* Get pointer to the data */
		b_buf = page_address(bvec.bv_page) + bvec.bv_offset;

		pr_debug("off: %lld, len: %ld, dest_buf: %px, user_buf: %px\n", off, b_len, b_buf, req->user.buf);

		if (rw) {
			// Write
			if (unlikely(copy_to_user(req->user.buf + off, b_buf, 1 << CHEEZE_LOGICAL_BLOCK_SHIFT))) {
				WARN_ON(1);
				pr_err("%s: copy_to_user() failed\n", __func__);
				return -EFAULT;
			}
		} else {
			// Read
			if (unlikely(copy_from_user(b_buf, req->user.buf + off, 1 << CHEEZE_LOGICAL_BLOCK_SHIFT))) {
				WARN_ON(1);
				pr_err("%s: copy_from_user() failed\n", __func__);
				return -EFAULT;
			}
		}

		/* Increment counters */
		off += b_len;
		*nr_bytes += b_len;
	}

	pr_debug("%s--\n", __func__);

	return 0;
}

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

	if (unlikely(count != sizeof(struct cheeze_req_user))) {
		pr_err("%s: size mismatch: %ld vs %ld\n",
			__func__, count, sizeof(struct cheeze_req_user));
		WARN_ON(1);
		return -EINVAL;
	}

	req = cheeze_peek();
	if (unlikely(req == NULL)) {
		WARN_ON(1);
		pr_err("%s: failed to peek queue\n", __func__);
		return -ERESTARTSYS;
	}

	if (unlikely(copy_to_user(buf, &req->user, count))) {
		WARN_ON(1);
		pr_err("%s: copy_to_user() failed\n", __func__);
		return -EFAULT;
	}

	return count;
}

static ssize_t cheeze_chr_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct cheeze_req *req;
	struct cheeze_req_user ureq;

	if (unlikely(count != sizeof(struct cheeze_req_user))) {
		WARN_ON(1);
		pr_err("%s: size mismatch: %ld vs %ld\n",
			__func__, count, sizeof(struct cheeze_req_user));
		return -EINVAL;
	}

	if (unlikely(copy_from_user(&ureq, buf, sizeof(ureq)))) {
		WARN_ON(1);
		pr_err("%s: failed to fill req\n", __func__);
		return -EFAULT;
	}

	pr_debug("write: req[%d]\n"
		"  buf=%px\n"
		"  pos=%u\n"
		"  len=%u\n",
			ureq.id, ureq.buf, ureq.pos, ureq.len);

	req = reqs + ureq.id;
	req->user.buf = ureq.buf;

	// Process bio
	req->ret = do_request(req);

	complete(&req->acked);

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
