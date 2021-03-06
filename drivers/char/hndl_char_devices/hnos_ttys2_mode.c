/*
 *  drivers/char/hndl_char_devices/hnos_ttys2_mode.c
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


/*====================for irda 38khz carrier=========================*/
#include <linux/kernel.h> 
#include <linux/module.h> 
#include <linux/clk.h> 
#include <asm/io.h> 
#include <asm/arch-at91/at91sam9260.h> 
#include <asm/arch-at91/at91_tc.h> 
#include <asm/arch-at91/gpio.h> 
/*====================for irda 38khz carrier end======================*/

#include "hnos_generic.h"
#include "hnos_ioctl.h" 
#include "hnos_proc.h" 
#include "hnos_gpio.h" 
#include "hnos_output.h"
#include "hnos_iomem.h"

#define IS_CONSOLE			1
#define IS_IRDA				0


/*====================for irda 38khz carrier=========================*/


#define TC_PWM_DEBUG			1		

#define TC_PWM_FREQ_DEFAULT		38000			/* 38 KHZ */
#define TC_PWM_DUTYCYCLE_DEFAULT	31		/* duty cycle 50% */ 
#define TC_PWM_CLKSRC                   AT91_TC_TIMER_CLOCK3	/* PWM　use TC clock3: MCK/32 */
#define TC_PWM_CLK_DIV			32			/* According to TC_PWM_CLKSRC */
 
 
/* CMR setting for TIOA output.*/
#define TC_CMR_TIOA  ( TC_PWM_CLKSRC | AT91_TC_WAVE | AT91_TC_EEVT_XC0 \
        |AT91_TC_WAVESEL_UP_AUTO | AT91_TC_ACPA_TOGGLE | AT91_TC_ACPC_TOGGLE )

/* CMR setting for TIOB output.*/
#define TC_CMR_TIOB  (TC_PWM_CLKSRC | AT91_TC_WAVE | AT91_TC_EEVT_XC0 \
			 | AT91_TC_WAVESEL_UP_AUTO | AT91_TC_BCPB_TOGGLE | AT91_TC_BCPC_TOGGLE )


static void volatile __iomem *tc_base;
struct clk *tc_clk;  
/*====================for irda 38khz carrier end======================*/

static char gpio_desc[6] = "PA10";
module_param_string(gpio_desc, gpio_desc, sizeof(gpio_desc), 0);
MODULE_PARM_DESC(gpio_desc, "GPIO usage, default PA10");

/* 
 * Default gpio PA10.
 * PA10 = 0, ttyS2 used as IRDA;
 * PA10 = 1, ttyS2 used as CONSOLE;
 * */
static int gpio = AT91_PIN_PA10;

enum LOCAL_COMMUN_MODE 
{
	COMMUN_CONSOLE = 0,		// Console
	COMMUN_RS232 = 1,		// RS232 工作方式
	COMMUN_ATTACH_IRDA = 2,		// 吸附红外工作方式
	COMMUN_MODULE_IRDA = 3,        // 调制红外工作方式
	COMMUN_IRDA = 4,               // IRDA 工作方式
};

static  struct proc_dir_entry	*hndl_proc_dir = NULL;
static int ttyS2_state = COMMUN_CONSOLE; 
static int ttyS2_mode_get(struct proc_item *item, char *page);
static int ttyS2_mode_set(struct proc_item *item, const char __user * userbuf, unsigned long count);

static struct proc_item items[] = 
{
	{
		.name = "ttyS2_state", 
		.pin = AT91_PIN_PA10,
		.settings = GPIO_OUTPUT_MASK | GPIO_OUTPUT_HIGH , /* output,  */
		.write_func = ttyS2_mode_set,
		.read_func = ttyS2_mode_get,
	},
	{NULL},
};

static int ttyS2_mode_get(struct proc_item *item, char *page)
{
	int len = 0;
	
	len = sprintf(page, "%d\n",ttyS2_state);
	return len;
}

static int ttyS2_mode_set(struct proc_item *item, const char __user * userbuf,
		unsigned long count) 
{
	unsigned int value = 0;
	char val[12] = {0};

	unsigned pin = item->pin;

	if (count >= 11){
		return -EINVAL;
	}

	if (copy_from_user(val, userbuf, count)){
		return -EFAULT;
	}

	value = (unsigned int)simple_strtoull(val, NULL, 0);

	dprintk(KERN_INFO "%s:val=%s,after strtoull,value=0x%08x\n", __FUNCTION__, val, value);

	ttyS2_state = value;
	if ( (COMMUN_IRDA == ttyS2_state) 
	     || (COMMUN_MODULE_IRDA == ttyS2_state)  
	     || (COMMUN_ATTACH_IRDA == ttyS2_state) ) { 
		at91_set_gpio_value(pin, IS_IRDA);
	} else {        
		at91_set_gpio_value(pin, IS_CONSOLE);
	}

	return 0;
}

static int __init  ttyS2_proc_devices_add(void)
{
	struct proc_item *item;
	int ret = 0;

	for (item = items; item->name; ++item) {
		hnos_gpio_cfg(item->pin, item->settings);
		ret += hnos_proc_entry_create(item);
	}

	return ret;
}

static int ttyS2_proc_devices_remove(void)
{
	struct proc_item *item;

	for (item = items; item->name; ++item) {
		remove_proc_entry(item->name, hndl_proc_dir);
	}

	return 0;
}

/*====================for irda 38khz carrier=========================*/
static inline u32 at91_tc_read(unsigned int offset) 
{ 
   return __raw_readl(tc_base + offset); 
} 

static inline void at91_tc_write(unsigned int offset, u32 value) 
{ 
   __raw_writel(value, tc_base + offset); 
} 

static int __init wave_init() 
{   
   at91_set_B_periph(AT91_PIN_PB0, 0); 

   tc_clk = clk_get(NULL, "tc3_clk"); 
   clk_enable(tc_clk); 

   tc_base = ioremap(AT91SAM9260_BASE_TC3, 0xFC); 
   if (!tc_base){ 
      printk("ioremap ERROR\n"); 
      goto unmap; 
   } 

   at91_tc_write(AT91_TC_CCR, //channel control teg
            AT91_TC_CLKDIS); //counter clock disable command

   at91_tc_write(AT91_TC_BMR,        //tc block mode reg,external clock signal 0,1,2 not selection
            AT91_TC_TC0XC0S_NONE | 
            AT91_TC_TC1XC1S_NONE | 
            AT91_TC_TC2XC2S_NONE); 

   at91_tc_write(AT91_TC_CMR,  TC_CMR_TIOA); //channel mode  reg


   wmb(); //保证指令执行的顺序


#define TIMER_COUNTER_MAX_DUTY TC_PWM_DUTYCYCLE_DEFAULT


   at91_tc_write(AT91_TC_RC, 
            TIMER_COUNTER_MAX_DUTY); 

   wmb(); 

/* Enable TC */ 
   at91_tc_write(AT91_TC_CCR, 
            AT91_TC_SWTRG | AT91_TC_CLKEN); 
  
unmap: 
   return 0; 
} 

static void __exit wave_exit(void) 
{ 
   printk("Removing wave.\n"); 

   at91_tc_write(AT91_TC_CCR, 
            AT91_TC_CLKDIS); 

   wmb(); 

   clk_disable(tc_clk); 
   clk_put(tc_clk); 
   iounmap(tc_base); 
} 


/*====================for irda 38khz carrier end======================*/
/* proc module init */
static int __init ttyS2_module_init(void)
{
	int status;
    wave_init();
    gpio = hnos_gpio_parse(gpio_desc, sizeof(gpio_desc));
    if (gpio < 0) {
        return -EINVAL;
    }
    items[0].pin = gpio;

	hndl_proc_dir = hnos_proc_mkdir();
	if (!hndl_proc_dir) {
		status = -ENODEV;
	} else {
		status = ttyS2_proc_devices_add();
		if (status) {
			hnos_proc_rmdir();
		}
	}

	HNOS_DEBUG_INFO("Proc Filesystem Interface of ttyS2 Mode Management init.\n");
	return (!status) ? 0 : -ENODEV;
}

static void ttyS2_module_exit(void)
{
	wave_exit();
	ttyS2_proc_devices_remove();
	hnos_proc_rmdir();

	HNOS_DEBUG_INFO("Proc Filesystem Interface of ttyS2 Mode Management exit.\n");
	return;
}

module_init(ttyS2_module_init);
module_exit(ttyS2_module_exit);

MODULE_LICENSE("Dual BSD/GPL");



