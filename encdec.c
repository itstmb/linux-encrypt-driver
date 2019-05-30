#include <linux/ctype.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <linux/string.h>

#include <stdio.h>

#include "encdec.h"

#define MODULE_NAME "encdec"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YOUR NAME");

int 	encdec_open(struct inode *inode, struct file *filp);
int 	encdec_release(struct inode *inode, struct file *filp);
int 	encdec_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

ssize_t encdec_read_caesar( struct file *filp, char *buf, size_t count, loff_t *f_pos );
ssize_t encdec_write_caesar(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

ssize_t encdec_read_xor( struct file *filp, char *buf, size_t count, loff_t *f_pos );
ssize_t encdec_write_xor(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

int memory_size = 0;

MODULE_PARM(memory_size, "i");

int major = 0;

struct file_operations fops_caesar = {
	.open 	 =	encdec_open,
	.release =	encdec_release,
	.read 	 =	encdec_read_caesar,
	.write 	 =	encdec_write_caesar,
	.llseek  =	NULL,
	.ioctl 	 =	encdec_ioctl,
	.owner 	 =	THIS_MODULE
};

struct file_operations fops_xor = {
	.open 	 =	encdec_open,
	.release =	encdec_release,
	.read 	 =	encdec_read_xor,
	.write 	 =	encdec_write_xor,
	.llseek  =	NULL,
	.ioctl 	 =	encdec_ioctl,
	.owner 	 =	THIS_MODULE
};

// Use this structure as your file-object's private data structure
typedef struct {
	unsigned char key;
	int read_state;
} encdec_private_data;

// buffers init by me
char * caesar_buffer;
char * xor_buffer;

int init_module(void)
{
	major = register_chrdev(major, MODULE_NAME, &fops_caesar);
	if(major < 0)
	{
		return major;
	}

	// 1. Allocate memory for the two device buffers using kmalloc (each of them should be of size 'memory_size')
	caesar_buffer = kmalloc(memory_size, GFP_KERNEL);
	xor_buffer = kmalloc(memory_size, GFP_KERNEL);

	memset(caesar_buffer, 0, memory_size);
	memset(xor_buffer, 0, memory_size);

	printk("encdec: ready\n");

	return 0;
}

void cleanup_module(void)
{
	// 1. Unregister the device-driver
	printk("encdec: unregistering\n");
	unregister_chrdev(major, MODULE_NAME);

	// 2. Free the allocated device buffers using kfree
	kfree(caesar_buffer);
	kfree(xor_buffer);
}

int encdec_open(struct inode *inode, struct file *filp)
{
	int minor = MINOR(inode->i_rdev);

	// 1. Set 'filp->f_op' to the correct file-operations structure (use the minor value to determine which)

	if (minor == 0) // Caesar Cipher
	{
		filp->f_op = &fops_caesar;
	}
	else if (minor == 1) // XOR Cipher
	{
		filp->f_op = &fops_xor;
	}
	else // invalid
	{
		return -ENODEV;
	}

	// 2. Allocate memory for 'filp->private_data' as needed (using kmalloc)
	encdec_private_data * pvt_data = kmalloc(sizeof(encdec_private_data), GFP_KERNEL);
	pvt_data->read_state = ENCDEC_READ_STATE_DECRYPT;
	pvt_data->key = 0;
	filp->private_data = pvt_data;

	return 0;
}

int encdec_release(struct inode *inode, struct file *filp)
{
	// 1. Free the allocated memory for 'filp->private_data' (using kfree)
	printk("encdec: freeing private data\n");
	kfree(&filp->private_data);

	return 0;
}

int encdec_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	// 1. Update the relevant fields in 'filp->private_data' according to the values of 'cmd' and 'arg'
	if (cmd == ENCDEC_CMD_CHANGE_KEY)
	{
		((encdec_private_data *) filp->private_data)->key = arg;
	}

	else if (cmd == ENCDEC_CMD_SET_READ_STATE)
	{
		((encdec_private_data *) filp->private_data)->read_state = arg;
	}

	else if (cmd == ENCDEC_CMD_ZERO)
	{
		int minor = MINOR(inode->i_rdev);
		if(minor == 0)
		{
			memset(caesar_buffer, 0, memory_size);
		}
		else if(minor == 1)
		{
			memset(xor_buffer, 0, memory_size);
		}
	}

	return 0;
}

ssize_t encdec_read_caesar( struct file *filp, char *buf, size_t count, loff_t *f_pos )
{
	if(*f_pos >= memory_size)
	{
		return -EINVAL;
	}
	if(count + *f_pos > memory_size)
	{
		count -= memory_size - count - *f_pos;
	}

	unsigned i;
	char* newstr = kmalloc(sizeof(char) * count, GFP_KERNEL);

	for(i = 0; i < count; i++)
	{
		newstr[i] = caesar_buffer[i + *f_pos];
	}
	if(((encdec_private_data *) filp->private_data)->read_state == ENCDEC_READ_STATE_DECRYPT)
	{
		for(i = 0; i < count; i++)
		{
			newstr[i] = ((newstr[i] - ((encdec_private_data *)filp->private_data)->key) + 128) % 128;
		}
	}

	copy_to_user(buf, newstr, sizeof(char) * count);

	*f_pos += count;

	kfree(newstr);

	return 0;
}


ssize_t encdec_write_caesar(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	unsigned i = 0;
	char* newstr = kmalloc(sizeof(char) * count, GFP_KERNEL);

	copy_from_user(newstr, buf, sizeof(char) * count);

	if(count + *f_pos > memory_size)
	{
		return -ENOSPC;
	}
	for(i = 0; i < count; i++)
	{
		caesar_buffer[i + *f_pos] = (newstr[i] + ((encdec_private_data *)filp->private_data)->key) % 128;
	}

	*f_pos += count;

	kfree(newstr);

	return 0;
}

ssize_t encdec_read_xor( struct file *filp, char *buf, size_t count, loff_t *f_pos )
{
	if(*f_pos >= memory_size)
	{
		return -EINVAL;
	}
	if(count + *f_pos > memory_size)
	{
		count -= memory_size - count - *f_pos;
	}

	unsigned i;
	char* newstr = kmalloc(sizeof(char) * count, GFP_KERNEL);

	if(count + *f_pos > memory_size)
	{
		return -ENOSPC;
	}

	for(i = 0; i < count; i++)
	{
		newstr[i] = xor_buffer[i + *f_pos];
	}
	if(((encdec_private_data *) filp->private_data)->read_state == ENCDEC_READ_STATE_DECRYPT)
	{
		for(i = 0; i < count; i++)
		{
			newstr[i] = newstr[i] ^ ((encdec_private_data *)filp->private_data)->key;
		}
	}

	copy_to_user(buf, newstr, sizeof(char) * count);

	*f_pos += count;

	kfree(newstr);

	return 0;
}

ssize_t encdec_write_xor(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	unsigned i = 0;
	char* newstr = kmalloc(sizeof(char) * count, GFP_KERNEL);

	copy_from_user(newstr, buf, sizeof(char) * count);

	if(count + *f_pos > memory_size)
	{
		return -ENOSPC;
	}
	for(i = 0; i < count; i++)
	{
		int key = ((encdec_private_data *)filp->private_data)->key;
		xor_buffer[i + *f_pos] = newstr[i] ^ key;
	}

	*f_pos += count;

	kfree(newstr);

	return 0;
}
