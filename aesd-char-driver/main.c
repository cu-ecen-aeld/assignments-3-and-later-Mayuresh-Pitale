/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>    // For kmalloc/kfree/krealloc
#include "aesdchar.h"

int aesd_major =   0;
int aesd_minor =   0;

MODULE_AUTHOR("Mayuresh-Pitale"); 
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    PDEBUG("open");
    
    // Set filp->private_data to our device structure for use in other methods
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset_byte = 0;
    ssize_t read_bytes = 0;
    ssize_t retval = 0;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    // Use Assignment 7 logic to find the entry and the offset within it
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset_byte);

    if (entry) {
        size_t available_bytes = entry->size - entry_offset_byte;
        size_t bytes_to_copy = (available_bytes < count) ? available_bytes : count;

        if (copy_to_user(buf, entry->buffptr + entry_offset_byte, bytes_to_copy)) {
            retval = -EFAULT;
            goto out;
        }

        read_bytes = bytes_to_copy;
        *f_pos += read_bytes; // Update the file position
        retval = read_bytes;
    }

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = -ENOMEM;
    const char *overwritten_buffer = NULL;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    // Allocate/Expand memory for the current write command
    dev->tmp_entry.buffptr = krealloc(dev->tmp_entry.buffptr, dev->tmp_entry.size + count, GFP_KERNEL);
    if (!dev->tmp_entry.buffptr) {
        goto out;
    }

    // Copy data from user space
    if (copy_from_user((char *)dev->tmp_entry.buffptr + dev->tmp_entry.size, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    dev->tmp_entry.size += count;

    // If the write command ends in a newline, add it to the circular buffer
    if (memchr(dev->tmp_entry.buffptr, '\n', dev->tmp_entry.size)) {
        overwritten_buffer = aesd_circular_buffer_add_entry(&dev->buffer, &dev->tmp_entry);
        
        // Free the memory of the oldest entry if it was overwritten
        if (overwritten_buffer) {
            kfree(overwritten_buffer);
        }

        // Reset the temp entry for the next write command
        dev->tmp_entry.buffptr = NULL;
        dev->tmp_entry.size = 0;
    }

    retval = count;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    // Initialize AESD specific portion
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if (result) {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    // Free all allocated memory in the circular buffer
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        if (entry->buffptr) {
            kfree(entry->buffptr);
        }
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
