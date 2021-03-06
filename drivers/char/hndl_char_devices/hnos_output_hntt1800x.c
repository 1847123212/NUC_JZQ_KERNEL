/*
 *  drivers/char/hndl_char_devices/hnos_rmc_hntt1800x.c
 *
	 *  For HNTT1800S Fujian/ HNTT1800F V3.0/ HNTT1800 ND V3.0/ HNTT1800U V1.0.
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "hnos_generic.h"
#include "hnos_ioctl.h" 
#include "hnos_proc.h" 
#include "hnos_gpio.h" 
#include "hnos_output.h"
#include "hnos_hntt1800x.h"

#define SMCBUS_OUTPUT_BASE0	UL(0x30000080)
#define SMCBUS_OUTPUT_BASE1	UL(0x300000A0)

static void __iomem *bases[NR_SMCBUS];
static struct hndl_rmc_device *rmc_hntt1800x;
struct smcbus_rmc_data smcbus_hntt1800x;

#ifdef CONFIG_HNDL_FKGA43  //����
static struct proc_item items_hntt1800x[] = 
{

	{
		.name = "FK0", 
		.pin = AT91_PIN_PC6,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
	{
		.name = "FK1", 
		.pin = AT91_PIN_PC5,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},

	
	{
		.name = "FK2", 
		.pin = AT91_PIN_PC10,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},

	{
		.name = "FK3", 
		.pin = AT91_PIN_PC4,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
	{
		.name = "FK4", 
		.pin = AT91_PIN_PC8,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},


	{
		.name = "FK5", 
		.pin = AT91_PIN_PA6,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
	{
		.name = "FK6", 
		.pin = AT91_PIN_PA7,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
	{
		.name = "FK7", 
		.pin = AT91_PIN_PA8,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
		{
		.name = "Alarm_out", 
		.pin = AT91_PIN_PA2,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
};
#else
static struct proc_item items_hntt1800x[] = 
{
		{
		.name = "PA8", 
		.pin = AT91_PIN_PA8,
		.settings = GPIO_OUTPUT_MASK, /* output */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
};
#endif

static struct gpio_rmc_data gpio_hntt1800x =
{
	.items = items_hntt1800x,
	.size = ARRAY_SIZE(items_hntt1800x),
};

static int __devinit gpio_channels_init(struct hndl_rmc_device *output)
{
	u8 offset = 0;
	u8 size = ARRAY_SIZE(items_hntt1800x);
	return rmc_gpio_register(output, &gpio_hntt1800x, offset, size);
}


/* SMCBUS read not supported by the hardware. */
#if 0
static int smcbus_read(struct smcbus_rmc_data *bus, u32 *reslt)
{
	u8 ch0 = 0, ch1 = 0;

	ch0 = readb(bases[0]);
	ch1 = readb(bases[1]);

	dprintk("%s: ch0 %2x, ch1 %2x\n", __FUNCTION__, ch0, ch1);

	*reslt = ( ch1 << 8 ) | ch0;
	return 0;
}
#endif

static int smcbus_write_channel(void __iomem *base, u8 stat, u8 bitmap, int is_set)
{
	u8 ch = stat;

	if (bitmap & 0xff) {
		if (is_set) {
			ch |= (bitmap);
		} else {
			ch &= (~bitmap);
		}
		writeb(ch, base);
	}
	return 0;
}

static int smcbus_write(struct smcbus_rmc_data *bus, u32 bitmap, int is_set)
{
	smcbus_write_channel(bases[0], (bus->smcbus_stat & 0xff),
			(bitmap & 0xff), is_set);
	smcbus_write_channel(bases[1], ((bus->smcbus_stat >> 8) & 0xff),
			((bitmap >> 8) & 0xff), is_set);

	return 0;
}

static int smcbus_proc_read(struct smcbus_rmc_data *bus, char *buf)
{
	int len = 0;
	//int reslt = smcbus_read(bus);
	int reslt = bus->smcbus_stat;

	dprintk("%s: %x.\n", __FUNCTION__, reslt);
	len = sprintf(buf + len, "%4x\n", reslt);
	return len;
}

static int smcbus_proc_write(struct smcbus_rmc_data *bus,
		const char __user *userbuf, unsigned long count)
{
	u32 value = 0;
	char val[14] = {0};

	if (count >= 14){
		return -EINVAL;
	}

	if (copy_from_user(val, userbuf, count)){
		return -EFAULT;
	}

	value = (unsigned int)simple_strtoull(val, NULL, 16);
	bus->smcbus_stat = value;

	dprintk(KERN_INFO "\n%s:val=%s,after strtoull,value=0x%08x\n",
			__FUNCTION__, val, value);

	writeb((value & 0xff), bases[0]);
	writeb(((value >> 8) & 0xff), bases[1]);

	return 0;

}

static void  smcbus_channels_unregister(struct hndl_rmc_device *output)
{
	rmc_smcbus_unregister(output, &smcbus_hntt1800x);
	iounmap(bases[1]);
	release_mem_region(SMCBUS_OUTPUT_BASE1, 1);
	iounmap(bases[0]);
	release_mem_region(SMCBUS_OUTPUT_BASE0, 1);

	return;
}

static int __devinit smcbus_channels_init(struct hndl_rmc_device *output)
{
	int reslt = -1;

	if (!request_mem_region(SMCBUS_OUTPUT_BASE0, 1, "smcbus_base2")) {
		printk("%s: request mem region error.\n", __FUNCTION__);
		reslt = -1;
		goto base0_request_failed;
	}

	bases[0] = ioremap(SMCBUS_OUTPUT_BASE0, 1);
	if (!bases[0]) {
		printk(KERN_ERR "Can NOT remap address 0x%08x\n", 
				(unsigned int)SMCBUS_OUTPUT_BASE0);
		reslt = -1;
		goto base0_map_failed;
	}

	if (!request_mem_region(SMCBUS_OUTPUT_BASE1, 1, "smcbus_base3")) {
		printk("%s: request mem region error.\n", __FUNCTION__);
		reslt = -1;
		goto base1_request_failed;
	}

	bases[1] = ioremap(SMCBUS_OUTPUT_BASE1, 1);
	if (!bases[1]) {
		printk(KERN_ERR "Can NOT remap address 0x%08x\n", 
				(unsigned int)SMCBUS_OUTPUT_BASE1);
		reslt = -1;
		goto base1_map_failed;
	}

	smcbus_hntt1800x.read = NULL;
	smcbus_hntt1800x.smcbus_stat = 0xffff;
	smcbus_hntt1800x.write = smcbus_write;
	smcbus_hntt1800x.proc_read = smcbus_proc_read;
	smcbus_hntt1800x.proc_write = smcbus_proc_write;

	reslt = rmc_smcbus_register(output, &smcbus_hntt1800x, 
			OUTPUT_SMCBUS_OFFSET, OUTPUT_SMCBUS_SIZE);
	if (reslt < 0) {
		goto smcbus_failed;
	}

	return reslt;

smcbus_failed:
	iounmap(bases[1]);
base1_map_failed:
	release_mem_region(SMCBUS_OUTPUT_BASE1, 1);
base1_request_failed:
	iounmap(bases[0]);
base0_map_failed:
	release_mem_region(SMCBUS_OUTPUT_BASE0, 1);
base0_request_failed:
	return reslt;
}

static void  rmc_hntt1800x_remove(void)
{
	//rmc_gpio_unregister(rmc_hntt1800x, &gpio_hntt1800x);

	if ( ID_MATCHED == hntt1800x_id_match(PRODUCT_HNTT1800S_FJ) ){
	//	smcbus_channels_unregister(rmc_hntt1800x);
		//rmc_smcbus_refresh_stop(rmc_hntt1800x); 
	}

	rmc_device_unregister(rmc_hntt1800x);
	rmc_device_free(rmc_hntt1800x);

    HNOS_DEBUG_INFO("RMC hntt1800x unregistered.\n");
	return;
}

static int __devinit rmc_hntt1800x_init(void)
{
	int ret = 0;

	rmc_hntt1800x = rmc_device_alloc();
	if (!rmc_hntt1800x) {
		return -1;
	}

	  gpio_channels_init(rmc_hntt1800x);

	if ( ID_MATCHED == hntt1800x_id_match(PRODUCT_HNTT1800S_FJ) )  {
		//smcbus_channels_init(rmc_hntt1800x);
	}

	ret = rmc_device_register(rmc_hntt1800x);
	
	if ( (0 == ret)
	      && (ID_MATCHED == hntt1800x_id_match(PRODUCT_HNTT1800S_FJ))) {
		//ret = rmc_smcbus_refresh_start(rmc_hntt1800x);
	}
	#ifndef CONFIG_HNDL_FKGA43  //���Ǹ���
    at91_set_gpio_output(AT91_PIN_PA6, 1);//gprs power
  #endif
    HNOS_DEBUG_INFO("RMC hntt1800x registered.\n");
	return ret; 
}


module_init(rmc_hntt1800x_init);
module_exit(rmc_hntt1800x_remove);

MODULE_LICENSE("Dual BSD/GPL");


