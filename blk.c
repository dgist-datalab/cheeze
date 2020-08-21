// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#define pr_fmt(fmt) "cheeze: " fmt

/*
 * XXX
 ** Concurrent I/O may require rwsem lock
 */

// #define DEBUG

#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/delay.h>

#include "cheeze.h"

#define SECTOR_SHIFT		9
#define SECTOR_SIZE		(1 << SECTOR_SHIFT)
#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)
#define CHEEZE_LOGICAL_BLOCK_SHIFT 12
#define CHEEZE_LOGICAL_BLOCK_SIZE	(1 << CHEEZE_LOGICAL_BLOCK_SHIFT)
#define CHEEZE_SECTOR_PER_LOGICAL_BLOCK	(1 << \
	(CHEEZE_LOGICAL_BLOCK_SHIFT - SECTOR_SHIFT))

// cheeze is intentionally designed to expose 1 disk only

/* Globals */
static int cheeze_major, i;
static struct gendisk *cheeze_disk;
static u64 cheeze_disksize;
static struct page *swap_header_page;

/*
 * Check if request is within bounds and aligned on cheeze logical blocks.
 */
static inline int cheeze_valid_io_request(struct bio *bio)
{
	if (unlikely(
		(bio->bi_iter.bi_sector >= (cheeze_disksize >> SECTOR_SHIFT)) ||
		(bio->bi_iter.bi_sector & (CHEEZE_SECTOR_PER_LOGICAL_BLOCK - 1)) ||
		(bio->bi_iter.bi_size & (CHEEZE_LOGICAL_BLOCK_SIZE - 1)))) {

		return 0;
	}

	/* I/O request is valid */
	return 1;
}

static int cheeze_bvec_read(struct bio_vec *bvec,
			    unsigned int index, unsigned int offset,
			    struct bio *bio)
{
	struct page *page;
	struct cheeze_req *req;
	unsigned char *user_mem, *swap_header_page_mem;
	unsigned long id;
	phys_addr_t addr;

//	mutex_lock(&mutex);
//	i++;

//	pr_info("i: %d\n", i);
//	mutex_unlock(&mutex);
#if 0
	if (unlikely(index != 0)) {
		// pr_err("tried to read outside of swap header\n");
		// Return empty pages on valid requests to workaround toybox binary search
	}
#endif

	page = bvec->bv_page;

	user_mem = kmap_atomic(page);
	// addr = virt_to_phys(user_mem);

	// pr_info("addr: 0x%llx (%lld)\n", addr, addr);

	id = cheeze_push(OP_READ, index, offset, bvec->bv_len, user_mem);

	while (reqs[id].acked == 0) {
		//mb();
		usleep_range(50, 75);
	}

#if 0
	if (index == 0 && swap_header_page) {
		swap_header_page_mem = kmap_atomic(swap_header_page);
		memcpy(user_mem + bvec->bv_offset, swap_header_page_mem, bvec->bv_len);
		kunmap_atomic(swap_header_page_mem);

		// It'll be read one-time only
		__free_page(swap_header_page);
		swap_header_page = NULL;
	} else {
		// Do not allow memory dumps
		memset(user_mem + bvec->bv_offset, 0, bvec->bv_len);
	}
#endif

	//msleep(1000 * 10);

	kunmap_atomic(user_mem);
	flush_dcache_page(page); // Hmm? XXX

	//udelay(500);

//	mutex_lock(&mutex);
//	i--;
//	mutex_unlock(&mutex);

	return 0;
}

static int cheeze_bvec_write(struct bio_vec *bvec,
			     unsigned int index, unsigned int offset,
			     struct bio *bio)
{
	struct page *page;
	struct cheeze_req *req;
	unsigned char *user_mem, *swap_header_page_mem;
	phys_addr_t addr;

	page = bvec->bv_page;

	user_mem = kmap_atomic(page);
//	addr = virt_to_phys(user_mem);

//	pr_info("addr: 0x%llx (%lld)\n", addr, addr);

	cheeze_push(OP_WRITE, index, offset, bvec->bv_len, user_mem);

/*
	if (swap_header_page == NULL)
		swap_header_page = alloc_page(GFP_KERNEL | GFP_NOIO);
	swap_header_page_mem = kmap_atomic(swap_header_page);
	memcpy(swap_header_page_mem, user_mem, PAGE_SIZE);
	kunmap_atomic(swap_header_page_mem);
*/
	kunmap_atomic(user_mem);

	return 0;
}

static int cheeze_bvec_rw(struct bio_vec *bvec,
			  unsigned int index, unsigned int offset,
			  struct bio *bio, int rw)
{
	if (rw == READ)
		return cheeze_bvec_read(bvec, index, offset, bio);
	else
		return cheeze_bvec_write(bvec, index, offset, bio);
}

static void __cheeze_make_request(struct bio *bio, int rw)
{
	int offset, ret;
	unsigned int index;
	struct bio_vec bvec;
	struct bvec_iter iter;

	if (!cheeze_valid_io_request(bio)) {
		pr_err("%s %d: invalid io request. "
		       "(bio->bi_iter.bi_sector, bio->bi_iter.bi_size,"
		       "cheeze_disksize) = "
		       "(%llu, %d, %llu)\n",
		       __func__, __LINE__,
		       (unsigned long long)bio->bi_iter.bi_sector,
		       bio->bi_iter.bi_size, cheeze_disksize);

		bio_io_error(bio);
		return;
	}

	index = bio->bi_iter.bi_sector >> SECTORS_PER_PAGE_SHIFT;
	offset = (bio->bi_iter.bi_sector & (SECTORS_PER_PAGE - 1)) <<
	    SECTOR_SHIFT;

	if (offset) {
		pr_err("%s %d: invalid offset. "
		       "(bio->bi_iter.bi_sector, index, offset) = (%llu, %d, %d)\n",
		       __func__, __LINE__,
		       (unsigned long long)bio->bi_iter.bi_sector,
		       index, offset);
		goto out_error;
	}

	if (bio->bi_iter.bi_size > PAGE_SIZE) {
		goto out_error;
	}

	if (bio->bi_vcnt > 1) {
		goto out_error;
	}

	bio_for_each_segment(bvec, bio, iter) {
		if (bvec.bv_len != PAGE_SIZE || bvec.bv_offset != 0) {
			pr_err("%s %d: bvec is misaligned. "
			       "(bv_len, bv_offset) = (%d, %d)\n",
			       __func__, __LINE__, bvec.bv_len, bvec.bv_offset);
			goto out_error;
		}

		//pr_info("%s: %s, index=%d, offset=%d, bv_len=%d\n",
		//	 __func__, rw ? "write" : "read", index, offset, bvec.bv_len);

		ret = cheeze_bvec_rw(&bvec, index, offset, bio, rw);
		if (ret < 0) {
			if (ret != -ENOSPC)
				pr_err("%s %d: cheeze_bvec_rw failed."
				       "(ret) = (%d)\n",
				       __func__, __LINE__, ret);
			else
				pr_debug("%s %d: cheeze_bvec_rw failed. "
					 "(ret) = (%d)\n",
					 __func__, __LINE__, ret);
			goto out_error;
		}

		index++;
	}

	bio->bi_status = BLK_STS_OK;
	bio_endio(bio);

	return;

out_error:
	bio_io_error(bio);
}

/*
 * Handler function for all cheeze I/O requests.
 */
static blk_qc_t cheeze_make_request(struct request_queue *queue,
				    struct bio *bio)
{
	__cheeze_make_request(bio, bio_data_dir(bio));

	return BLK_QC_T_NONE;
}

static const struct block_device_operations cheeze_fops = {
	.owner = THIS_MODULE
};

static ssize_t disksize_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%llu\n", cheeze_disksize);
}

static ssize_t disksize_store(struct device *dev,
			      struct device_attribute *attr, const char *buf,
			      size_t len)
{
	int ret;
	u64 disksize;

	ret = kstrtoull(buf, 10, &disksize);
	if (ret)
		return ret;

	if (disksize == 0) {
		set_capacity(cheeze_disk, 0);
		return len;
	}

	cheeze_disksize = PAGE_ALIGN(disksize);
	if (!cheeze_disksize) {
		pr_err("disksize is invalid (disksize = %llu)\n", cheeze_disksize);

		cheeze_disksize = 0;

		return -EINVAL;
	}

	set_capacity(cheeze_disk, cheeze_disksize >> SECTOR_SHIFT);

	return len;
}

static DEVICE_ATTR(disksize, S_IRUGO | S_IWUSR, disksize_show, disksize_store);

static struct attribute *cheeze_disk_attrs[] = {
	&dev_attr_disksize.attr,
	NULL,
};

static struct attribute_group cheeze_disk_attr_group = {
	.attrs = cheeze_disk_attrs,
};

static int create_device(void)
{
	int ret;

	/* gendisk structure */
	cheeze_disk = alloc_disk(1);
	if (!cheeze_disk) {
		pr_err("%s %d: Error allocating disk structure for device\n",
		       __func__, __LINE__);
		ret = -ENOMEM;
		goto out;
	}

	cheeze_disk->queue = blk_alloc_queue(GFP_KERNEL);
	if (!cheeze_disk->queue) {
		pr_err("%s %d: Error allocating disk queue for device\n",
		       __func__, __LINE__);
		ret = -ENOMEM;
		goto out_put_disk;
	}

	blk_queue_make_request(cheeze_disk->queue, cheeze_make_request);

	cheeze_disk->major = cheeze_major;
	cheeze_disk->first_minor = 0;
	cheeze_disk->fops = &cheeze_fops;
	cheeze_disk->private_data = NULL;
	snprintf(cheeze_disk->disk_name, 16, "cheeze%d", 0);

	/* Actual capacity set using sysfs (/sys/block/cheeze<id>/disksize) */
	set_capacity(cheeze_disk, 0);

	/*
	 * To ensure that we always get PAGE_SIZE aligned
	 * and n*PAGE_SIZED sized I/O requests.
	 */
	blk_queue_physical_block_size(cheeze_disk->queue, PAGE_SIZE);
	blk_queue_logical_block_size(cheeze_disk->queue,
				     CHEEZE_LOGICAL_BLOCK_SIZE);
	blk_queue_io_min(cheeze_disk->queue, PAGE_SIZE);
	blk_queue_io_opt(cheeze_disk->queue, PAGE_SIZE);
	blk_queue_max_hw_sectors(cheeze_disk->queue, PAGE_SIZE / SECTOR_SIZE);

	add_disk(cheeze_disk);

	cheeze_disksize = 0;

	ret = sysfs_create_group(&disk_to_dev(cheeze_disk)->kobj,
				 &cheeze_disk_attr_group);
	if (ret < 0) {
		pr_err("%s %d: Error creating sysfs group\n",
		       __func__, __LINE__);
		goto out_free_queue;
	}

	/* cheeze devices sort of resembles non-rotational disks */
	queue_flag_set_unlocked(QUEUE_FLAG_NONROT, cheeze_disk->queue);
	queue_flag_clear_unlocked(QUEUE_FLAG_ADD_RANDOM, cheeze_disk->queue);

out:
	return ret;

out_free_queue:
	blk_cleanup_queue(cheeze_disk->queue);

out_put_disk:
	put_disk(cheeze_disk);

	return ret;
}

static void destroy_device(void)
{
	if (!cheeze_disk)
		return;

	pr_info("Removing device cheeze0\n");

	sysfs_remove_group(&disk_to_dev(cheeze_disk)->kobj,
			   &cheeze_disk_attr_group);

	del_gendisk(cheeze_disk);
	put_disk(cheeze_disk);

	if (cheeze_disk->queue)
		blk_cleanup_queue(cheeze_disk->queue);
}

static int __init cheeze_init(void)
{
	int ret;

	cheeze_major = register_blkdev(0, "cheeze");
	if (cheeze_major <= 0) {
		pr_err("%s %d: Unable to get major number\n",
		       __func__, __LINE__);
		ret = -EBUSY;
		goto out;
	}

	ret = create_device();
	if (ret) {
		pr_err("%s %d: Unable to create cheeze_device\n",
		       __func__, __LINE__);
		goto free_devices;
	}

	ret = cheeze_chr_init_module();
	if (ret)
		goto destroy_devices;

	reqs = kzalloc(sizeof(struct cheeze_req) * CHEEZE_QUEUE_SIZE, GFP_KERNEL);
	if (reqs == NULL) {
		pr_err("%s %d: Unable to allocate memory for cheeze_req\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto nomem;
	}

	return 0;

nomem:
	cheeze_chr_cleanup_module();
destroy_devices:
	destroy_device();
free_devices:
	unregister_blkdev(cheeze_major, "cheeze");
out:
	return ret;
}

static void __exit cheeze_exit(void)
{
	kfree(reqs);

	cheeze_chr_cleanup_module();

	destroy_device();

	unregister_blkdev(cheeze_major, "cheeze");

	if (swap_header_page)
		__free_page(swap_header_page);
}

module_init(cheeze_init);
module_exit(cheeze_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Park Ju Hyung <qkrwngud825@gmail.com>");
