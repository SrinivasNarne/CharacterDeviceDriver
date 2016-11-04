#include <linux/fs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#define MYDEV_NAME "mycdrv"

#define ASP_CHGACCDIR  _IO('Z', 1)
#define RD_SIZE (16 * PAGE_SIZE)

struct asp_mycdrv {
    struct list_head list;
    char *ramDisk;
    long buffer_size;
    struct cdev cdev;
    struct semaphore sem;
    int devNo;
    long forward_or_reverse;
};

static int number_of_devices = 3;
module_param(number_of_devices, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

static unsigned int major_number = 0;
static struct class *mycdrv_class = NULL;

LIST_HEAD(myDeviceHead);

int mycdrv_open(struct inode *inode, struct file *my_file)
{
    printk(KERN_ALERT "In the OPEN function");
    pr_info(" attempting to open device: %s:n", MYDEV_NAME);
    pr_info(" MAJOR number = %d, MINOR number = %dn", imajor(inode), iminor(inode));
    struct list_head *pos = NULL;
    struct asp_mycdrv *dev = NULL;
    if (imajor(inode) != major_number || iminor(inode) < 0 || iminor(inode) >= number_of_devices) {
        printk(KERN_ALERT "Device couldn't be found with minor=%d and major=%d\n", imajor(inode), iminor(inode));
        return -ENODEV;
    }
    list_for_each(pos, &myDeviceHead) {
        dev = list_entry(pos, struct asp_mycdrv, list);
        if(dev->devNo == iminor(inode)) {
            break;
        }
    }
    my_file->private_data = dev;
    pr_info(" successfully opened  device: %s:nn", MYDEV_NAME);
    pr_info("ref=%d\n", (int)module_refcount(THIS_MODULE));
    return 0;
}

int mycdrv_release(struct inode *inode, struct file *my_file)
{
    printk(KERN_ALERT "In the CLOSE function\n");
    pr_info(" CLOSING device: %s\n", MYDEV_NAME);
    return 0;
}

ssize_t mycdrv_read(struct file *my_file, char __user *buf, size_t count, loff_t *f_pos)
{
    printk(KERN_ALERT "In the READ function");
    struct asp_mycdrv *dev = (struct asp_mycdrv *)my_file->private_data;
    unsigned char *aux_buffer = NULL;
    loff_t i;
    ssize_t value_to_be_returned = 0;
    if (down_interruptible(&(dev->sem)) != 0) {
        printk(KERN_ALERT "Error Performing Operation\n");
    }
    if (*f_pos >= RD_SIZE)
    {
      kfree(aux_buffer);
      up(&(dev->sem));
      return value_to_be_returned;
    }
    if (((dev->forward_or_reverse == 0) && (*f_pos + count > RD_SIZE)) || ((dev->forward_or_reverse == 1) && (*f_pos - count < 0))) {
        printk(KERN_ALERT "Error Performing Operation\n");
        value_to_be_returned = -EINVAL;
        if (aux_buffer != NULL)
        {
          kfree(aux_buffer);
          up(&(dev->sem));
          return value_to_be_returned;
        }
    }
    aux_buffer = (unsigned char*) kzalloc(count, GFP_KERNEL);
    if (aux_buffer == NULL) {
        printk(KERN_ALERT "%s: Error: Out of Memory ", __func__);
        value_to_be_returned = -ENOMEM;
        if (aux_buffer != NULL)
        {
          kfree(aux_buffer);
          up(&(dev->sem));
          return value_to_be_returned;
        }
    }
    if (dev->forward_or_reverse == 0) {
        for (i = 0; i < count; i++) {
            aux_buffer[i] = dev->ramDisk[*f_pos + i];
        }
        *f_pos += count;
    }
    else {
        for (i = 0; i < count; i++)
        aux_buffer[i] = dev->ramDisk[*f_pos - i];
        *f_pos -= count;
    }
    if (copy_to_user(buf, aux_buffer, count) != 0)
    {
        value_to_be_returned = -EFAULT;
        if (aux_buffer != NULL)
        {
          kfree(aux_buffer);
          up(&(dev->sem));
          return value_to_be_returned;
        }
    }
    value_to_be_returned = count;
    if (aux_buffer != NULL)
    {
      kfree(aux_buffer);
      up(&(dev->sem));
      return value_to_be_returned;
    }
}
ssize_t mycdrv_write(struct file *my_file, const char __user *buf, size_t count,
loff_t *f_pos)
{
    printk(KERN_ALERT "In the WRITE function\n");
    struct asp_mycdrv *dev = (struct asp_mycdrv *)my_file->private_data;
    ssize_t value_to_be_returned = 0;
    loff_t i;
    unsigned char *aux_buffer = NULL;

    if (down_interruptible(&(dev->sem))!=0) {
        printk(KERN_ALERT "%s: Cannot perform operation\n", MYDEV_NAME);
    }
    if (*f_pos >= RD_SIZE) {
        printk(KERN_ALERT "%s: Exceeded the buffer size limit", __func__);
        value_to_be_returned = -EINVAL;
        if (aux_buffer != NULL)
        {
          kfree(aux_buffer);
          up(&(dev->sem));
          return value_to_be_returned;
        }
    }
    if (((dev->forward_or_reverse == 0) &&
    (*f_pos + count > RD_SIZE)) ||
    ((dev->forward_or_reverse == 1)
    && (*f_pos - count < 0))) {
        printk(KERN_ALERT "%s: Writting beyond buffer size limit", __func__);
        value_to_be_returned = -EINVAL;
        if (aux_buffer != NULL)
        {
          kfree(aux_buffer);
          up(&(dev->sem));
          return value_to_be_returned;
        }
    }

    aux_buffer = (unsigned char*) kzalloc(count, GFP_KERNEL);
    if (aux_buffer == NULL) {
        printk(KERN_ALERT "%s: out of memory operation failed", __func__);
        value_to_be_returned = -ENOMEM;
    }
    if (aux_buffer != NULL)
        {
          kfree(aux_buffer);
          up(&(dev->sem));
          return value_to_be_returned;
        }

    if (dev->forward_or_reverse == 0) {
        for (i = 0; i < count; i++) {
            dev->ramDisk[*f_pos + i] = aux_buffer[i];
        }
        *f_pos += count;
    }

    else {
        for (i = 0; i < count; i++)
          dev->ramDisk[*f_pos - i] = aux_buffer[i];
        *f_pos -= count;
    }

    value_to_be_returned = count;
    if (aux_buffer != NULL)
    {
      kfree(aux_buffer);
      up(&(dev->sem));
      return value_to_be_returned;
    }
}

loff_t mycdrv_lseek(struct file *my_file, loff_t offset, int orig)
{
    struct asp_mycdrv *dev = (struct asp_mycdrv *)my_file->private_data;
    loff_t position_pointer = 0;
    if (down_interruptible(&(dev->sem))!=0) {
        printk(KERN_ALERT "%s: Error: Cannot lock", MYDEV_NAME);
    }
    switch(orig) {
        case 0:
        position_pointer = offset;
        break;
        case 1:
        if (dev->forward_or_reverse == 0)
        position_pointer = my_file->f_pos + offset;
        else
        position_pointer = my_file->f_pos - offset;
        break;
        case 2:
        position_pointer = RD_SIZE + offset;
        if (dev->forward_or_reverse == 0)
        position_pointer = RD_SIZE + offset;
        else
        position_pointer = RD_SIZE - offset;
        break;
        default:
        position_pointer = -EINVAL;
        up(&(dev->sem));
        return position_pointer;
    }
    if (position_pointer < 0 || position_pointer > RD_SIZE) {
        position_pointer = -EINVAL;
        up(&(dev->sem));
        return position_pointer;
    }
    my_file->f_pos = position_pointer;
    up(&(dev->sem));
    return position_pointer;
}

long mycdrv_ioctl (struct file *my_file, unsigned int cmd, unsigned long arg)
{
    struct asp_mycdrv *dev = (struct asp_mycdrv *)my_file->private_data;
    long value_to_be_returned;
    if (cmd != ASP_CHGACCDIR) {
        printk(KERN_ALERT "command: %d: act_command: %d\n", cmd, ASP_CHGACCDIR);
        return -1;
    }
    if (arg != 0 && arg != 1) {
        printk(KERN_ALERT "%s: Error", __func__);
        return -1;
    }
    if (down_interruptible(&(dev->sem))!=0) {
        printk(KERN_ALERT "%s: Lock not acquired during open\n", MYDEV_NAME);
    }
    value_to_be_returned = dev->forward_or_reverse;
    dev->forward_or_reverse = arg;
    up(&(dev->sem));
    return value_to_be_returned;
}

struct file_operations mycdrv_fops = {
    .owner =    THIS_MODULE,
    .open =     mycdrv_open,
    .read =     mycdrv_read,
    .write =    mycdrv_write,
    .release =  mycdrv_release,
    .llseek =   mycdrv_lseek,
    .unlocked_ioctl = mycdrv_ioctl,
};

static int mycdrv_make_device(struct asp_mycdrv *dev, int minor,
struct class *class)
{
    int checker_temp = 0;
    dev_t devno = MKDEV(major_number, minor);
    struct device *device = NULL;
    dev->buffer_size = RD_SIZE;
    dev->ramDisk = NULL;
    dev->devNo = minor;
    dev->forward_or_reverse = 0;

    sema_init(&(dev->sem),1);
    cdev_init(&dev->cdev, &mycdrv_fops);

    dev->cdev.owner = THIS_MODULE;
    dev->ramDisk = (unsigned char*)kzalloc(dev->buffer_size, GFP_KERNEL);
    if (dev->ramDisk == NULL) {
        printk(KERN_ALERT "%s: out of memoryn", __func__);
        return -ENOMEM;
    }
    checker_temp = cdev_add(&dev->cdev, devno, 1);
    if (checker_temp) {
        printk(KERN_ALERT "Error %d while trying to add %s%d", checker_temp,
        MYDEV_NAME, minor);
        return checker_temp;
    }
    device = device_create(class, NULL, devno, NULL, MYDEV_NAME "%d", minor);
    if (IS_ERR(device)) {
        checker_temp = PTR_ERR(device);
        printk(KERN_ALERT "Error %d while trying to create %s%d", checker_temp,
        MYDEV_NAME, minor);
        cdev_del(&dev->cdev);
        return checker_temp;
    }
    return 0;
}
static void mycdrv_clean(void)
{
    int i = 0;
    struct list_head *position = NULL;
    struct asp_mycdrv *dev = NULL;
    iterate:
    list_for_each(position, &myDeviceHead) {
        dev = list_entry(position, struct asp_mycdrv, list);
        device_destroy(mycdrv_class, MKDEV(major_number, i));
        cdev_del(&dev->cdev);
        kfree(dev->ramDisk);
        list_del(&(dev->list));
        kfree(dev);
        i++;
        goto iterate;
    }
    if (mycdrv_class)
    class_destroy(mycdrv_class);
    unregister_chrdev_region(MKDEV(major_number, 0), number_of_devices);
    return;
}
static int __init my_init(void)
{
    int i = 0;
    dev_t dev = 0;
    int ret_check = 0;
    struct asp_mycdrv *mycdrv_device = NULL;
    if (number_of_devices <= 0) {
        printk("Please enter valid no. present- number_of_devices: %d\n", number_of_devices);
        return -EINVAL;
    }
    ret_check = alloc_chrdev_region(&dev, 0, number_of_devices, MYDEV_NAME);
    if (ret_check < 0) {
        printk("Failure of alloc_chrdev_region()n");
        return ret_check;
    }
    major_number = MAJOR(dev);
    mycdrv_class = class_create(THIS_MODULE, MYDEV_NAME);
    if (IS_ERR(mycdrv_class)) {
        ret_check = PTR_ERR(mycdrv_class);
        mycdrv_clean();
        return ret_check;
    }
    for (i = 0; i < number_of_devices; ++i) {
        mycdrv_device = (struct asp_mycdrv *)kzalloc(sizeof(struct asp_mycdrv),
        GFP_KERNEL);
        if (mycdrv_device == NULL) {
            ret_check = -ENOMEM;
            mycdrv_clean();
            return ret_check;
        }
        ret_check = mycdrv_make_device(mycdrv_device, i, mycdrv_class);
        if (ret_check) {
            mycdrv_clean();
            return ret_check;
        }
        INIT_LIST_HEAD(&(mycdrv_device->list));
        list_add(&(mycdrv_device->list), &myDeviceHead);
    }
    return 0;
}

static void __exit my_exit(void)
{
    printk(KERN_ALERT "Good Bye\n");
    mycdrv_clean();
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Srinivas Narne");
MODULE_DESCRIPTION("Character Device Driver for ASP");
MODULE_LICENSE("GPL v2");
