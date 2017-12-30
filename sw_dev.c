/*
	Copyright (C) 2017  Riki Hayashi

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include<linux/module.h>
#include<linux/fs.h>
#include<linux/cdev.h>
#include<linux/device.h>
#include<asm/uaccess.h>
#include<linux/io.h>
#include<linux/delay.h>
#include<linux/slab.h>

MODULE_AUTHOR("Riki Hayashi");
MODULE_DESCRIPTION("driver for LED control");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#define GPIO_PULLUP 0x02

#define INPUT 0x00
#define OUTPUT 0x01

#define SW_PIN 20
#define LED_PIN 25

static dev_t dev;
static struct cdev cdv;
static struct class *cls = NULL;
static volatile u32 *gpio_base = NULL;

static volatile int open_counter = 0;

unsigned int ret = 0;
int flag = 0;
int sw = 0;

static int gpio_set(int pin, uint32_t func){
	const u32 index = pin / 10;
	const u32 shift = (pin % 10) * 3;
	const u32 mask = ~(0x07 << shift);
	gpio_base[index] = (gpio_base[index] & mask) | ( func << shift);
	return 1;
}

static ssize_t led_write(struct file* filp, const char* buf, size_t count, loff_t* pos){
	char c;

	if(copy_from_user(&c,buf,sizeof(char)))
		return -EFAULT;

	if(c == '0')
		gpio_base[10] = 1 << 25;
	else if(c == '1'){
		gpio_base[7] = 1 << 25;
	}
	return 1;
}

static ssize_t sw_read(struct file* filp, char __user *buf, size_t count, loff_t* pos){
	unsigned char sw_buf[4];

	gpio_base[37] = GPIO_PULLUP & 0x03;
	msleep(1);
	gpio_base[38] = 0x1 << SW_PIN;
	msleep(1);

	gpio_base[37] = 0;
	gpio_base[38] = 0;

	ret = !((gpio_base[13] & (0x01 << SW_PIN)) != 0);
	sprintf(sw_buf, "%d\n", ret);
	count = strlen(sw_buf);

	if(copy_to_user((void *)buf, &sw_buf,count)){
		printk(KERN_INFO "err %d\n", ret);
		return -EFAULT;
	}

	if(ret == '0')
		gpio_base[10] = 1 << 25;
	else if(ret == '1')
		gpio_base[7] = 1 << 25;

	*pos += count;
	return count;
}

static struct file_operations led_fops = {
	.owner	= THIS_MODULE,
	.read	= sw_read,
	.write	= led_write
};

static int __init init_mod(void)
{
	int retval;

	gpio_base = ioremap_nocache(0x3F200000,0xA0);

	gpio_set(SW_PIN, INPUT);
	gpio_set(LED_PIN, OUTPUT);

	retval = alloc_chrdev_region(&dev, 0, 1, "mysw");
	if(retval < 0){
		printk(KERN_ERR "alloc_chrdev_region failed.\n");
		return retval;
	}
	printk(KERN_INFO "%s is loaded. major:%d\n",__FILE__,MAJOR(dev));

	cdev_init(&cdv, &led_fops);
	retval = cdev_add(&cdv, dev, 1);
	if(retval < 0){
		printk(KERN_ERR "cdev_add failed. major:%d, minor:%d",MAJOR(dev),MINOR(dev));
		return retval;
	}
	cls = class_create(THIS_MODULE,"mysw");
	if(IS_ERR(cls)){
		printk(KERN_ERR "class_create failed.");
		return PTR_ERR(cls);
	}
	device_create(cls, NULL, dev, NULL, "mysw%d",MINOR(dev));
	return 0;
}

static void __exit cleanup_mod(void)
{
	cdev_del(&cdv);
	device_destroy(cls, dev);
	class_destroy(cls);
	unregister_chrdev_region(dev, 1);
	printk(KERN_INFO "%s is unload. major:%d\n",__FILE__,MAJOR(dev));
}

module_init(init_mod);
module_exit(cleanup_mod);

