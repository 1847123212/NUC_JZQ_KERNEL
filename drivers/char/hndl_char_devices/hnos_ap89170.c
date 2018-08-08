/****************************************Copyright (c)**************************************************
 **                                  http://www.hnelec.com
 **
 **--------------File Info-------------------------------------------------------------------------------
 ** File name:			hnos_ap89170.c
 ** File direct:			..\linux-2.6.21.1\drivers\char\hndl_char_devices
 ** Created by:			kernel team
 ** Last modified Date:  2008-05-12
 ** Last Version:		1.0
 ** Descriptions:		The voice chip AP89170 is linux2.6 kernel device driver
 **
 ********************************************************************************************************/
#include "hnos_generic.h"
#include "hnos_ioctl.h"                /*�����ַ��͵��豸��IO��������������ͷ�ļ���*/
#include "hnos_proc.h" 
#include "hnos_gpio.h" 
#include "hnos_ap89170.h"
#include "hnos_iomem.h"
/********************************************************************************************************
 **              					function announce
 **              				AP89170 ��API�ӿں�������
 ********************************************************************************************************/

int ap89170_open(struct inode *inode, struct file *filp);
int ap89170_release(struct inode *inode, struct file *filp);
int ap89170_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg);

/*********************************************************************************************************
 **                  "ȫ�ֺ;�̬���������ﶨ��"         
 **        global variables and static variables define here
 ********************************************************************************************************/

/*�豸�ṹ��*/
struct  ap89170_device
{
	unsigned long is_open;                                  //����ԭ�Ӳ���
	struct class *myclass;
	struct cdev cdev;
	struct spi_device *spi;
	struct semaphore lock;                      //���������õ��ź���
	//wait_queue_head_t play_queue; 
	struct iomem_object *iomem_state;                                  
	u8 tx_buf[TX_BUFSIZE];
	u8 rx_buf[RX_BUFSIZE];
};

static struct ap89170_device *ap89170_devp = NULL;      //�豸�ṹ��ʵ��
static int ap89170_major = 0; 			          //��Ϊ0����̬������豸��
static int ap89170_minor = 0;

/*���屾������file_operations�ṹ��*/
struct file_operations ap89170_fops =
{
	.owner      = THIS_MODULE,
	.open       = ap89170_open,
	.ioctl      = ap89170_ioctl,
	.release    = ap89170_release,
};
static struct proc_item items_ap89170[] = 
{
	{
		.name = "voice_power", 
		.pin = GPIO_VOICE_CTRL,
		.settings = GPIO_OUTPUT_MASK , /* output,  */
		.write_func = hnos_proc_gpio_set,
		.read_func = hnos_proc_gpio_get,
	},
	{NULL}
};
static  struct proc_dir_entry	*hndl_proc_dir = NULL;

static int  ap89170_proc_devices_add(void)
{
	struct proc_item *item;
	int ret = 0;

	for (item = items_ap89170; item->name; ++item) {
		ret += hnos_proc_entry_create(item);
	}

	return ret;
}
static int ap89170_proc_devices_remove(void)
{
	struct proc_item *item;

	for (item = items_ap89170; item->name; ++item) {
		remove_proc_entry(item->name, hndl_proc_dir);
	}

	return 0;
}


/*********************************************************************************************************
 ** Function name: ap89170_write_cmd
 ** Descriptions:  send the command and voice group address
 ** Input: cmd:    ap89170's operate command 
 **        adr:    ap89170's voice group address
 ** Output :       
 ********************************************************************************************************/

int ap89170_write_cmd(unsigned int cmd, unsigned int adr, unsigned char width)
{
	struct spi_device *spi = ap89170_devp->spi;
	struct spi_message message;
	struct spi_transfer xfer;
	int status = 0;

	if (down_interruptible(&ap89170_devp->lock))                   //����������ֹ���߳�ͻ
	{   
	   return - ERESTARTSYS ;
	}

	//down(&ap89170_devp->lock);

	ap89170_devp->tx_buf[0] = cmd;    		//�ȷ�������
	ap89170_devp->tx_buf[1] = adr;			//�ٷ���voice group�ĵ�ַ

	/* Build our spi message */
	spi_message_init(&message);                    //spi.h��INIT_LIST_HEAD ��ʼ������ͷ
	memset(&xfer, 0, sizeof(xfer));
	xfer.len = width;                                 //�����ֽ���
	xfer.tx_buf = ap89170_devp->tx_buf;           //�������ݵ�ָ��
	xfer.rx_buf = ap89170_devp->rx_buf;           //�������ݵ�ָ��

	spi_message_add_tail(&xfer, &message);  //spi.h��list_add_tail ���ӽ�����ӵ����ж�����

	/* do the i/o */
	status = spi_sync(spi, &message);              //spi.c����ɴ���
	if(status)
	{
		printk("%s: error status=%x\n", __FUNCTION__, status); 
	}

	up(&ap89170_devp->lock);                      //�ͷ��ź���

	return status;
}

/*********************************************************************************************************
 ** Function name: ap89170_cdev_setup
 ** Descriptions:  Create a cdev for ap89170
 ** Input: *dev:   ap89170's device struct pointer 
 **        devno:  ap89170's device number
 ** Output :       
 ********************************************************************************************************/

static void  ap89170_cdev_setup(struct ap89170_device *dev, dev_t devno)
{
	int err;

	cdev_init(&dev->cdev, &ap89170_fops);		//��ʼ��cdev�ṹ

	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &ap89170_fops;
	err = cdev_add(&dev->cdev, devno, 1);		//���ַ��豸���뵽�ں˵��ַ��豸�����У�chrdev[]����

	if(err != 0)
	{
		printk(KERN_NOTICE "Error %d adding ap89170 device, major_%d.\n", err, MAJOR(devno));
	}

	return;
}

/*********************************************************************************************************
 ** Function name: ap89170_open
 ** Descriptions:  AP89170 open function
 App layer will invoke this interface to open ap89170 device, just set a 
 flag which comes from device struct.
 ** Input:inode:   information of device
 **       filp:    pointer of file
 ** Output 0:      OK
 **        other:  not OK    
 ********************************************************************************************************/

int ap89170_open(struct inode *inode, struct file *filp)
{
	static struct ap89170_device *dev;

	dev = container_of(inode->i_cdev, struct ap89170_device, cdev); //�õ�����ĳ���ṹ��Ա�Ľṹ��ָ��
	filp->private_data = dev; /* for other methods */                  //���豸�ṹ��ָ�븳ֵ���ļ�˽������ָ��

	if(test_and_set_bit(0, &dev->is_open) != 0)						//����ĳһλ�����ظ�λԭ����ֵ
	{
		return -EBUSY;       
	}

	return 0; /* success. */
}

/*********************************************************************************************************
 ** Function name: ap89170_release
 ** Descriptions:  AP89170 release function
 App layer will invoke this interface to release ap89170 device, just clear a 
 flag which comes from device struct.
 ** Input:inode:   information of device
 **       filp:    pointer of file
 ** Output 0:      OK
 **        other:  not OK    
 ********************************************************************************************************/

int ap89170_release(struct inode *inode, struct file *filp)
{
	struct ap89170_device *dev = filp->private_data;                //����豸�ṹ��ָ��

	if(test_and_clear_bit(0, &dev->is_open) == 0)   /* release lock, and check... */
	{
		return -EINVAL;     /* already released: error */
	}

	return 0;
}

/*********************************************************************************************************
 ** Function name: ap89170_ioctl
 ** Descriptions:  AP89170 ioctl function
 App layer will invoke this interface to control ap89170 chip
 ** Input:inode:   information of device
 **       filp:    pointer of file
 **       cmd:     command
 **       arg:     additive parameter
 ** Output 0:      OK
 **        other:  not OK  
 ********************************************************************************************************/

int ap89170_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	u8 tmp_data;
	int i;
	int ret = 0;
	unsigned char state=0;

	struct iomem_object *iomem = ap89170_devp->iomem_state;
	if(iomem == NULL)
		return -1;

	tmp_data = arg & 0xff;								//arg����Ϊ��Ҫ���ŵ�voice group��ַ				                    

	/*��������������ݴ���*/
	if (_IOC_TYPE(cmd) != HNDL_AT91_IOC_MAGIC)          //�ж��Ƿ����ڸ��豸
	{           
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > HNDL_AT91_IOC_MAXNR )            //�ж��Ƿ񳬹������������
	{            
		return -ENOTTY;
	}

	switch(cmd)											//���ݲ�ͬ�Ĳ���cmd���Ͳ�ͬ������
	{
		case AP89170_IOCTL_POWER_UP:

			ret = ap89170_write_cmd(AP89170_CMD_PUP2, 0, 1);
			if(ret)
			{
				printk("%s:Powerup2 error\n", __FUNCTION__);
				return(-1);
			}

			for(i=0; i<POWER_TIMEOUT; i++)
			{
				iomem->read_byte(iomem, &state, IO_RDONLY);
				if(state & AP89170_BUSY)
				{   //printk("Powerup2 wait %u\n", i);
					break;
				}
			}
			if(!(state & AP89170_BUSY))
			{ 
				printk("%s:fail to power up\n", __FUNCTION__);
				return(-1);
			}
			msleep_interruptible(50);
			break;

		case AP89170_IOCTL_POWER_DOWN:

			ret = ap89170_write_cmd(AP89170_CMD_PDN2, 0, 1);
			if(ret)
			{
				printk("%s:Powerdown2 error\n", __FUNCTION__);
				return(-1);
			}

			for(i=0; i<POWER_TIMEOUT; i++)
			{
				iomem->read_byte(iomem, &state, IO_RDONLY);
				if(state & AP89170_BUSY)
				{   //printk("Powerdown2 wait %u\n", i);
					break;
				}
			}
			if(!(state & AP89170_BUSY))
			{ 
				printk("%s:fail to power down\n", __FUNCTION__);
				return(-1);
			}    
			msleep_interruptible(50);
			break;

		case AP89170_IOCTL_PAUSE:
			ret = ap89170_write_cmd(AP89170_CMD_PAUSE, 0, 1);
			if(ret)
			{
				printk("%s:Pause error\n", __FUNCTION__);
				return(-1);
			}                     
			break;

		case AP89170_IOCTL_RESUME:
			ret = ap89170_write_cmd(AP89170_CMD_RESUME, 0, 1);
			if(ret)
			{
				printk("%s:Resume error\n", __FUNCTION__);
				return(-1);
			}           
			break;

		case AP89170_IOCTL_PLAY:

			for(i=0; i<WAITPLAY_TIMEOUT; i++)
			{
				iomem->read_byte(iomem, &state, IO_RDONLY);
				if(!(state&AP89170_BUSY_FULL))
				{   //printk("Play wait %u\n", i);
					break;
				}
				msleep_interruptible(10);
			}
			if(state&AP89170_BUSY_FULL)
			{
				printk("%s:WAIT TO PLAY TIMEOUT\n", __FUNCTION__); 
				return(-1);
			}

			ret = ap89170_write_cmd(AP89170_CMD_PLAY, tmp_data, 2); 
			if(ret)
			{
				printk("%s:Play error\n", __FUNCTION__);
				return(-1);
			}

			for(i=0; i<PLAYSTART_TIMEOUT; i++)
			{
				iomem->read_byte(iomem, &state, IO_RDONLY);
				if(state & AP89170_BUSY)
				{   //printk("Play start wait %u\n", i);
					msleep_interruptible(10);
					iomem->read_byte(iomem, &state, IO_RDONLY);
					if(state & AP89170_BUSY)
					{  
						break;
					} 
                                   else
                                   {
                                   	printk("%s:fail to start play\n", __FUNCTION__);
				        	return(-1);
                                   }
				}
				msleep_interruptible(10);
			}    
			if(!(state & AP89170_BUSY))
			{
				printk("%s:fail to start play\n", __FUNCTION__);
				return(-1);
			}

			msleep_interruptible(500);

			for(i=0; i<PLAYEND_TIMEOUT; i++)
			{    
				iomem->read_byte(iomem, &state, IO_RDONLY);
				if(!(state & AP89170_BUSY_FULL))
				{   //printk("Play end wait %u\n", i);
					return(0);
				}
				msleep_interruptible(50);
			}
			if(state&AP89170_BUSY_FULL)
			{
				printk("%s:WAIT TO PLAYEND TIMEOUT\n", __FUNCTION__); 
				return(-1);
			}              

			break;

		case AP89170_IOCTL_STATUS:
			/* The high 5 bit of this arg must be 00010 B */
			tmp_data &= 0x07;
			tmp_data |= 0x10;
			ret = ap89170_write_cmd(AP89170_CMD_RESUME, tmp_data, 2);
			if(ret)
			{
				printk("%s:Resume error\n", __FUNCTION__);
				return(-1);
			}
			break;

		case AP89170_IOCTL_PREFETCH:
			for(i=0; i<WAITEMPTY_TIMEOUT; i++)
			{ 
				iomem->read_byte(iomem, &state, IO_RDONLY);//�жϻ������Ƿ��д��
				if(!(state & AP89170_FULL))
				{   //printk("prefetch wait %u\n", i);
					break;
				}
				msleep_interruptible(10);    
			}
			if(state & AP89170_FULL)
			{
				printk("%s:WAIT TO READYED FILL TIMEOUT\n", __FUNCTION__);
				return(-1);
			}

			ret = ap89170_write_cmd(AP89170_CMD_PREFETCH, tmp_data, 2);
			if(ret)
			{
				printk("%s:Prefetch error\n", __FUNCTION__);
				return(-1);
			}

			for(i=0; i<FILLBUF_TIMEOUT; i++)
			{ 
				iomem->read_byte(iomem, &state, IO_RDONLY); //�ж���װ�ص�voice group�Ƿ�ɹ�����
				if(state & AP89170_FULL)
				{   //printk("prefetch end wait %u\n", i);
					break;
				}
				msleep_interruptible(10);    
			} 
			if(!(state & AP89170_FULL))
			{
				printk("%s:WAIT TO FILLED TIMEOUT\n", __FUNCTION__);
				return(-1);
			}                              
			break;

		case AP89170_IOCTL_PUP1:
			ret = ap89170_write_cmd(AP89170_CMD_PUP1, 0, 1);
			if(ret)
			{
				printk("%s:Powerup1 error\n", __FUNCTION__);
				return(-1);
			}
			break;

		case AP89170_IOCTL_PDN1:
			ret = ap89170_write_cmd(AP89170_CMD_PDN1, 0, 1);
			if(ret)
			{
				printk("%s:Powerdown1 error\n", __FUNCTION__);
				return(-1);
			}    
			break;
        case IOCTL_VOICE_MODULES_POWERON:
            ret = at91_set_gpio_value(GPIO_VOICE_CTRL, ePowerON);
            break;
            
        case IOCTL_VOICE_MODULES_POWEROFF:
            ret = at91_set_gpio_value(GPIO_VOICE_CTRL, ePowerOFF);
            break;
		default:
			return -ENOTTY;
	}

	return ret;
}

/*********************************************************************************************************
 ** Function name: ap89170_remove
 ** Descriptions:  AP89170 remove function
 SPI core will invoke this interface to remove ap89170
 ** Input: *spi    spi core device struct pointer
 ** Output 0:      OK
 **        other:  not OK    
 ********************************************************************************************************/

static int  ap89170_remove(struct spi_device *spi)
{
	dev_t devno = MKDEV(ap89170_major, ap89170_minor);  //�����豸�źʹ��豸����ϳ��豸�� 
	struct class* myclass;

	if (hndl_proc_dir) {
		ap89170_proc_devices_remove();
		hnos_proc_rmdir();
	}

	if(ap89170_devp != 0)
	{
		/* Get rid of our char dev entries */    
		cdev_del(&ap89170_devp->cdev);  				        //��ϵͳ���Ƴ�һ���ַ��豸  

		iomem_object_put(ap89170_devp->iomem_state);

		myclass = ap89170_devp->myclass;
		if(myclass != 0)
		{
			class_device_destroy(myclass, devno);		        //����һ�����豸
			class_destroy(myclass);						        //����һ����
		}

		kfree(ap89170_devp);                                                   //�ͷ�ap89170_probe������������ڴ�
		ap89170_devp = NULL;
	}

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);					//�ͷŷ����һϵ���豸��
	return 0;
}

/*********************************************************************************************************
 ** Function name: ap89170_remove
 ** Descriptions:  AP89170 probe function
 SPI core will invoke this probe interface to init ap89170.
 ** Input: *spi    spi core device struct pointer
 ** Output 0:      OK
 **        other:  not OK       
 ********************************************************************************************************/

static int __devinit ap89170_probe(struct spi_device *spi)
{
	int result  = 0;
	dev_t dev   = 0;
	struct class* myclass;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if(ap89170_major)			//֪�����豸��
	{
		dev = MKDEV(ap89170_major, ap89170_minor);
		result = register_chrdev_region(dev, 1, AP89170_DEV_NAME);
	}
	else						//��֪�����豸�ţ���̬����
	{
		result = alloc_chrdev_region(&dev, ap89170_minor, 1, AP89170_DEV_NAME);
		ap89170_major = MAJOR(dev);      
	}

	if(result < 0)
	{
		printk(KERN_WARNING "hndl_kb: can't get major %d\n", ap89170_major);
		return result;
	}    

	/* allocate the devices -- we do not have them static. */
	ap89170_devp = kmalloc(sizeof(struct ap89170_device), GFP_KERNEL);   //ΪAP89170�豸�ṹ�������ڴ�
	if(!ap89170_devp)
	{
		/* Can not malloc memory for ap89170 */
		printk("ap89170 Error: Can not malloc memory\n");
		ap89170_remove(spi);
		return -ENOMEM;
	}
	memset(ap89170_devp, 0, sizeof(struct ap89170_device));

	init_MUTEX(&ap89170_devp->lock);              						  //��ʼ���ź���    

	/* Register a class_device in the sysfs. */
	myclass = class_create(THIS_MODULE, AP89170_DEV_NAME);             //class.c�У���ϵͳ�н���һ����
	if(NULL == myclass)
	{
		printk("ap89170 Error: Can not create class\n");
		ap89170_remove(spi);
		return result;
	}

	class_device_create(myclass, NULL, dev, NULL, AP89170_DEV_NAME);

	ap89170_devp->myclass = myclass;

	ap89170_cdev_setup(ap89170_devp, dev);    

	ap89170_devp->spi = spi;

	ap89170_devp->iomem_state = iomem_object_get(AP89170_SIGNAL_ADR, 0);
	if (!ap89170_devp->iomem_state)
	{
		printk(KERN_ERR "%s: can't get iomem (phy %08x).\n", __FUNCTION__,
				(unsigned int) AP89170_SIGNAL_ADR);
		ap89170_remove(spi);
		return(-1);		
	}

    at91_set_gpio_output(GPIO_VOICE_CTRL, ePowerOFF);

	hndl_proc_dir = hnos_proc_mkdir();
	if (!hndl_proc_dir) {
		result = -ENODEV;
		printk(KERN_ERR "hnos_proc_mkdir fail \n");
		ap89170_remove(spi);
		return(-1);	
	} else {
		result = ap89170_proc_devices_add();
		if (result) {
		    printk(KERN_ERR "att7022_proc_devices_add fail \n");
			ap89170_remove(spi);
		    return(-1);	
		}
	}
    
	HNOS_DEBUG_INFO("Initialized device %s, major %d.\n", AP89170_DEV_NAME, ap89170_major);
	return 0;
}


/*ap89170���������ӵ�spi ��������*/
static struct spi_driver ap89170_driver = {
	.driver = {
		.name   = "ap89170",
		.owner  = THIS_MODULE,
	},
	.probe  = ap89170_probe,
	.remove = __devexit_p(ap89170_remove),
};

/*********************************************************************************************************
 ** Function name: ap89170_init
 ** Descriptions:  AP89170 init function
 register AP89170 driver to SPI core
 ** Input:none
 ** Output 0:      OK
 **        other:  not OK
 ********************************************************************************************************/

static int __init ap89170_init(void)
{
	int ret;

	ret = spi_register_driver(&ap89170_driver);                 //spi.c�У�driver_register

	return ret;
}

/*********************************************************************************************************
 ** Function name: ap89170_exit
 ** Descriptions:  AP89170 exit function
 unregister AP89170 driver from SPI core
 ** Input:none
 ** Output none
 ********************************************************************************************************/

static void __exit ap89170_exit(void)
{
	spi_unregister_driver(&ap89170_driver);                     //spi.h�У�driver_unregister

	return;
}

module_init(ap89170_init);
module_exit(ap89170_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("kernel team");
