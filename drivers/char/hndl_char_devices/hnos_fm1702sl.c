/****************************************Copyright (c)**************************************************
**                                  http://www.hnelec.com
**
**--------------File Info-------------------------------------------------------------------------------
** File name:            hnos_fm1702sl.c
** File direct:            ..\linux-2.6.21.1\drivers\char\hndl_char_devices
** Created by:            kernel team
** Last modified Date:  2008-09-18
** Last Version:        1.0
** Descriptions:        The fmcard reader chip fm1702sl is linux2.6 kernel device driver
**
********************************************************************************************************/

#include "hnos_generic.h"
#include "hnos_ioctl.h"                /*�����ַ��͵��豸��IO��������������ͷ�ļ���*/
#include "hnos_proc.h"
#include "hnos_gpio.h"
#include "hnos_fm1702sl.h"

unsigned char globalbuf[GLOBAL_BUFSIZE]={0};/*ȫ�����飬���ڴ����ʱ����*/
unsigned char Card_UID[5]={0};/*����SIN�ţ�ǰ4byteΪ��Ч���ݣ���5��byteΪУ��*/
unsigned char keybuf[8]={0};/*ǰ2byteΪ��Կ��E2PROM���׵�ַLSB��MSB����6byteΪ��Կ*/
unsigned char valuebuf[5]={0};/*��1byteΪֵ���ڿ�ţ���4byteΪ������ֵ����λ��ǰ*/

/********************************************************************************************************
**                                  function announce
**                              fm1702sl ��API�ӿں�������
********************************************************************************************************/

int fm1702sl_open(struct inode *inode, struct file *filp);
int fm1702sl_release(struct inode *inode, struct file *filp);
//ssize_t fm1702sl_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
//ssize_t fm1702sl_write(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
int fm1702sl_ioctl(struct inode *inode, struct file *filp, unsigned int cmd,
                  unsigned long arg);

/*********************************************************************************************************
**                  "ȫ�ֺ;�̬���������ﶨ��"
**        global variables and static variables define here
********************************************************************************************************/

/*�豸�ṹ��*/
struct  fm1702sl_device
{
    unsigned long is_open;                                  /*����ԭ�Ӳ���*/
    struct class *myclass;
    struct cdev cdev;
    struct spi_device *spi;
    struct semaphore lock;                                  /*���������õ��ź���*/
    u8 tx_buf[TX_BUFSIZE];
    u8 rx_buf[RX_BUFSIZE];
};

static struct fm1702sl_device *fm1702sl_devp;      /*�豸�ṹ��ʵ��*/
static int fm1702sl_major = 0;                        /*��Ϊ0����̬������豸��*/
static int fm1702sl_minor = 0;

/*���屾������file_operations�ṹ��*/
struct file_operations fm1702sl_fops =
{
    .owner      = THIS_MODULE,
    .open       = fm1702sl_open,
    .ioctl      = fm1702sl_ioctl,
//    .read       = fm1702sl_read,
//    .write      = fm1702sl_write,
    .release    = fm1702sl_release,
};

/*********************************************************************************
**                                    ������������
*********************************************************************************/

/****************************************************************
**����: SPIWrite
**����: �ú���ʵ��дFM1702SL�ļĴ���
**����: regadr  �Ĵ�����ַ[0x01~0x3f]
        buffer  �������ݵĴ������ָ��
        width   �������ݵ��ֽ���
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int SPIWrite(unsigned char regadr, unsigned char *buffer, unsigned char width)
{
    struct spi_device *spi = fm1702sl_devp->spi;
    struct spi_message message;
    struct spi_transfer xfer;
    int status = 0;
    unsigned char i, adrtemp;

    if (down_interruptible(&fm1702sl_devp->lock))                   /*����������ֹ���߳�ͻ*/
    {
        return - ERESTARTSYS ;
    }

    //down(&fm1702sl_devp->lock);

    adrtemp = regadr;
    if((adrtemp&0xc0) == 0)        /*��Ϊ�Ĵ�����ַû�г���0x3F��*/
    {
        adrtemp=(adrtemp<<1) & 0x7e;    /*д����ʱ����ַ�����λ�����λ����Ϊ0���м�Ϊ��ַ*/
        fm1702sl_devp->tx_buf[0] = adrtemp;     /*�ȷ��͵�ַ*/

        for(i=0; i<width; i++)
        {
            fm1702sl_devp->tx_buf[i+1] = buffer[i];    /*�ٷ�������*/
        }

        /* Build our spi message */
        spi_message_init(&message);                    /*spi.h��INIT_LIST_HEAD ��ʼ������ͷ*/
        memset(&xfer, 0, sizeof(xfer));
        xfer.len = width+1;                            /*�����ֽ���*/
        xfer.tx_buf = fm1702sl_devp->tx_buf;           /*�������ݵ�ָ��*/
        xfer.rx_buf = fm1702sl_devp->rx_buf;           /*�������ݵ�ָ��*/

        spi_message_add_tail(&xfer, &message);  /*spi.h��list_add_tail ���ӽ�����ӵ����ж�����*/

        /* do the i/o */
        status = spi_sync(spi, &message);              /*spi.c����ɴ���*/
        if(status)
        {
            printk("%s: error! spi status=%x\n", __FUNCTION__, status);
        }
        
        up(&fm1702sl_devp->lock);                      /*�ͷ��ź���*/
        return status;
    }
    else
    {
        up(&fm1702sl_devp->lock);                      /*�ͷ��ź���*/
        return(-1);
    }
}

/****************************************************************
**����: SPIRead
**����: �ú���ʵ�ֶ�FM1702SL�ļĴ���
**����: regadr  �Ĵ�����ַ[0x01~0x3f]
        buffer  �������ݵĴ������ָ��
        width   �������ݵ��ֽ���
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int SPIRead(unsigned char regadr, unsigned char *buffer, unsigned char width)
{
    struct spi_device *spi = fm1702sl_devp->spi;
    struct spi_message message;
    struct spi_transfer xfer;
    int status = 0;
    unsigned char i, adrtemp;

    if (down_interruptible(&fm1702sl_devp->lock))                   /*����������ֹ���߳�ͻ*/
    {
        return - ERESTARTSYS ;
    }

    //down(&fm1702sl_devp->lock);
    adrtemp = regadr;
    if((adrtemp&0xc0) == 0)        /*��Ϊ�Ĵ�����ַû�г���0x3F��*/
    {
        adrtemp = (adrtemp<<1) | 0x80;    /*������ʱ����ַ�����λΪ1�����λ����Ϊ0���м�Ϊ��ַ*/
        fm1702sl_devp->tx_buf[0] = adrtemp;

        for(i=0; i<width; i++)
        {
            if(i != width-1)
            {
                adrtemp = (regadr<<1) | 0x80;
            }
            else
            {
                adrtemp = 0; /*���Ҫ�෢��һ�ֽ�0x00*/
            }
            fm1702sl_devp->tx_buf[i+1] = adrtemp;
        }

        /* Build our spi message */
        spi_message_init(&message);                    /*spi.h��INIT_LIST_HEAD ��ʼ������ͷ*/
        memset(&xfer, 0, sizeof(xfer));
        xfer.len = width+1;                            /*�����ֽ���*/
        xfer.tx_buf = fm1702sl_devp->tx_buf;           /*�������ݵ�ָ��*/
        xfer.rx_buf = fm1702sl_devp->rx_buf;           /*�������ݵ�ָ��*/

        spi_message_add_tail(&xfer, &message);  /*spi.h��list_add_tail ���ӽ�����ӵ����ж�����*/

        /* do the i/o */
        status = spi_sync(spi, &message);              /*spi.c����ɴ���*/
        if(status)
        {
            printk("%s: error! spi status=%x\n", __FUNCTION__, status); 
            return(-1);
        }

        memcpy(buffer, &fm1702sl_devp->rx_buf[1], width);/*�ӵڶ����ֽڿ�ʼ��Ϊ��Ч����*/
        
        up(&fm1702sl_devp->lock);                      /*�ͷ��ź���*/
        return(0);
    }
    else
    {
        up(&fm1702sl_devp->lock);                      /*�ͷ��ź���*/
        return(-1);
    }
}

/****************************************************************
**����: ClearFIFO
**����: �ú���ʵ�����FM1702SL��FIFO������
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int ClearFIFO(void)
{
    unsigned char acktemp, temp[1];
    unsigned int i;

    acktemp = SPIRead(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] |= 0x1;
    acktemp = SPIWrite(Control_Reg, temp, 1);  /*�������FIFO�Լ���дָ��*/
    if(acktemp)
    {
        return(-1);
    }
    for(i=0; i<CLEREFIFO_TIMEOUT; i++)/*�ȴ�FIFO������*/
    {
        acktemp = SPIRead(FIFOLength_Reg, temp, 1);
        if(acktemp == 0)
        {
            if(temp[0] == 0)
            {
                return(0);
            }
        }
        else
        {
        	return(-1);
        }	
    }
    return(-1);
}

/****************************************************************
**����: WriteFIFO
**����: �ú���ʵ����FM1702SL�е�FIFOд����
**����: databuf д�����ݵ�����ָ��
        width   д�����ݵ��ֽ���
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int WriteFIFO(unsigned char *databuf, unsigned char width)
{
    unsigned char acktemp;

    acktemp = SPIWrite(FIFOData_Reg, databuf, width);
    if(acktemp)
    {
        return(-1);
    }
   
    return(0);
}

/****************************************************************
**����: ReadFIFO
**����: �ú���ʵ�ִ�FM1702SL�е�FIFO������
**����: width ��FIFOLength�Ĵ������������ݴ�ŵ�ַ
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int ReadFIFO(unsigned char *width)
{
    unsigned char acktemp;

    acktemp = SPIRead(FIFOLength_Reg, width, 1);  /*��ȡFIFO�е��ֽ���*/
    if(acktemp)
    {
        return(-1);
    }
    if(width[0] == 0)
    {
        printk("%s:FIFO is empty, can't read\n",  __FUNCTION__);
        return(-1);
    }

    width[0] = (width[0]>GLOBAL_BUFSIZE)?GLOBAL_BUFSIZE:width[0];
    acktemp = SPIRead(FIFOData_Reg, globalbuf, width[0]);
    if(acktemp)
    {
        printk("%s:Read FIFO error!\n", __FUNCTION__);
        return(-1);
    }

    return(0);
}

/****************************************************************
**����: CommandSend
**����: �ú���ʵ����FM1702SL��������Ĺ���
**����: comm    ��Ҫ���͵�FM1702����
        databuf ����Ĳ���
        width   �����ֽ���
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int CommandSend(unsigned char comm, unsigned char *databuf, unsigned char width)
{
    unsigned char acktemp, temp[1];
    unsigned int i;
		
    temp[0] = IDLE;
    acktemp = SPIWrite(Command_Reg, temp, 1);
    if(acktemp)
    {		
        return(-1);
    }

    if(width)/*�е�����û��Ҫͨ��FIFO���ݲ���������*/
    {
        acktemp = ClearFIFO();
        if(acktemp)
        {
            return(-1);
        }
        
        acktemp = WriteFIFO(databuf, width);
        if(acktemp)
        {
            return(-1);
        }
    }

    acktemp = SPIWrite(Command_Reg, &comm, 1);
    if(acktemp)
    {
        return(-1);
    }
    
    /*WriteE2����ֻ��ͨ����λ����ָ��Ĵ���дIdle������ֹ*/
    if(comm == WriteE2)
    {
        return(0);
    }    
        
    for(i=0; i<COMMAND_TIMEOUT; i++)/*�ȴ������������������״̬Idle=0x00*/
    {
        acktemp = SPIRead(Command_Reg, temp, 1);
        if(acktemp == 0)
        {   
            if(temp[0] == IDLE)
            {
                return(0);
            }
        }
        else
        {
        	return(-1);
        }
        
        msleep_interruptible(1);	
    }

    return(-1);
}

/****************************************************************
**����: Fm1702slReset
**����: �ú���ʵ�ֶ�FM1702SL������
**����: ��
**���: ��
*****************************************************************/

void Fm1702slReset(void)
{
    at91_set_gpio_output(AT91_PIN_PA13, 1);
		
    msleep_interruptible(10);
		
    at91_set_gpio_value(AT91_PIN_PA13, 0);
		
    msleep_interruptible(10);		
}		
		
/****************************************************************
**����: Fm1702slSpiinit
**����: �ú���ʵ�ֶ�FM1702SL��SPI���г�ʼ��
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int Fm1702slSpiinit(void)
{
    unsigned char acktemp, temp[1];
    unsigned int i;

    for(i=0; i<INSIDEINIT_TIMEOUT; i++)/*�ȴ��ڲ���ʼ���������������*/
    {
        acktemp = SPIRead(Command_Reg, temp, 1);
        if(acktemp)
        {
            return(-1);
        }
        if(temp[0] == IDLE)
        {
            break;
        }
    }         

    if(temp[0])                
    {
        printk("%s:Inside init error!\n", __FUNCTION__);
        return(-1);
    }
    else
    {
        temp[0] = 0x80;
        acktemp = SPIWrite(Page0_Reg, temp, 1);/*��ʼ��SPI�ӿ�*/
        if(acktemp)
        {
            return(-1);
        }
        
        for(i=0; i<SPIINIT_TIMEOUT; i++)/*�ȴ�SPI��ʼ���������������*/
        {            
            acktemp = SPIRead(Command_Reg, temp, 1);
            if(acktemp)
            {
                return(-1);
            }
            if(temp[0] == IDLE)
            {
                break;
            }
        }    
  
        if(temp[0])                
        {
            printk("%s:FM1702SL SPI init error!\n", __FUNCTION__);
            return(-1);
        }

        temp[0] = 0x00;
        acktemp = SPIWrite(Page0_Reg, temp, 1); /*�л�������Ѱַ��ʽ*/
        if(acktemp)
        {
            return(-1);
        }

        return(0);
    }
}

/****************************************************************
**����: Fm1702slInit
**����: �ú���ʵ�ֶ�FM1702SL�ĸ���ؼĴ������г�ʼ��
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int Fm1702slInit(void)
{
    unsigned char acktemp, temp[1];

    temp[0] = 0x3f;
    acktemp = SPIWrite(InterruptEn_Reg, temp, 1);/*��ֹ�����ж�*/
    if(acktemp)
    {
        return(-1);
    }
    temp[0] = 0x3f;
    acktemp = SPIWrite(InterruptRq_Reg, temp, 1);/*��������жϱ�־*/
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = 0x5b;/*�����������ڲ���������TX1TX2���13.56MHz���������ݷ�����Ƶ������ز�*/
    acktemp = SPIWrite(TxControl_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    temp[0] = 0x1;/*ѡ��Qʱ����Ϊ������ʱ�ӡ�������ʼ�մ򿪡��ڲ�������*/
    acktemp = SPIWrite(RxControl2_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = 0x7;/*���ݷ��ͺ󣬽������ȴ�7��bitʱ����(1443A֡����ʱ��Ϊ94us)*/
    acktemp = SPIWrite(RxWait_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    return 0 ;
}

/****************************************************************
**����: FM1702Standby
**����: �ú�������ƵоƬ��������ģʽ���Խ���ϵͳ�Ĺ���
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int Fm1702slStandby(void)
{
    unsigned char acktemp, temp[1];
    
    acktemp = SPIRead(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    
    temp[0] |= 0x20;/*��λControl�Ĵ�����StandByλ*/
    acktemp = SPIWrite(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    
    return(0);
} 

/****************************************************************
**����: Fm1702slWakeup
**����: �ú������ڻ�����Ƶ����оƬ
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int Fm1702slWakeup(void)
{
    unsigned char acktemp, temp[1];
    
    acktemp = SPIRead(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    
    temp[0] &= 0xdf;/*��λControl�Ĵ�����StandByλ*/
    acktemp = SPIWrite(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    
    msleep_interruptible(1);/*��Ҫ�ȴ�һ��ʱ������˳�StandByģʽ*/
    
    return(0);
}

/***********************************************************************************
**                                        RF�ͼ�����
***********************************************************************************/

/****************************************************************
**����: RequestAll
**����: Ѱ��������ȫ����
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
**˵��: Ѱ���ɹ����ѿ���������д��globalbuf[0]��globalbuf[1]��
*****************************************************************/

int RequestAll(void)
{
    unsigned char acktemp, temp[1];

    temp[0] = 0x7;/*�յ��ĵ�һ��byte�����λ����FIFO�еĵ�0λ��*/
    acktemp = SPIWrite(BitFraming_Reg, temp, 1);/*���һbyte��Ҫ���ͳ�ȥ��bit��Ϊ7*/
    if(acktemp)
    {
        return(-1);
    }

    acktemp = SPIRead(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    temp[0] &= 0xf7;            /*�رռ��ܵ�Ԫ*/
    acktemp = SPIWrite(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*���չ��̲�����CRC��������CRC��*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x3;/*����TxLastBits��Ϊ0�����Ա����ֹCRC�������CRCУ�����*/
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = REQALL;/*���ⷢ��Request All(0x52)������������*/
    acktemp = CommandSend(Transceive, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    acktemp = ReadFIFO(temp);/*��ȡ���յ������ݣ�Ӧ��Ϊ����byte ATQA*/
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 0x2)
    {
        return(-1);
    }
    
    return(0);
}

/****************************************************************
**����: RequestIdle
**����: Ѱ��������δ��������״̬�Ŀ�
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
**˵��: Ѱ���ɹ����ѿ���������д��globalbuf[0]��globalbuf[1]��
*****************************************************************/

int RequestIdle(void)
{
    unsigned char acktemp, temp[1];

    temp[0] = 0x7;/*�յ��ĵ�һ��byte�����λ����FIFO�еĵ�0λ��*/
    acktemp = SPIWrite(BitFraming_Reg, temp, 1);/*���һbyte��Ҫ���ͳ�ȥ��bit��Ϊ7*/
    if(acktemp)
    {
        return(-1);
    }

    acktemp = SPIRead(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    temp[0] &= 0xf7;            /*�رռ��ܵ�Ԫ*/
    acktemp = SPIWrite(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*���չ��̲�����CRC��������CRC��*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x3;/*����TxLastBits��Ϊ0�����Ա����ֹCRC�������CRCУ�����*/
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = REQIDL;/*���ⷢ��Request std(0x26)������������*/
    acktemp = CommandSend(Transceive,temp,1);
    if(acktemp)
    {
        return(-1);
    }
    acktemp = ReadFIFO(temp);/*��ȡ���յ������ݣ�Ӧ��Ϊ����byte ATQA*/
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 0x2)
    {
        return(-1);
    }

    return(0);
}

/****************************************************************
**����: GetUID
**����: ����ײ�Ļ�ù�����Χ��ĳһ�ſ������к�
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
**˵��: ��õ����кŴ���Card_UID�����У�ǰ4byteΪ��Ч���ݣ�
        ��5byteΪǰ4byte�����У��ֵ
*****************************************************************/

int GetUID(void)
{
    unsigned char acktemp,temp[2],i;

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*���չ��̲�����CRC��������CRC��*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x3;
    acktemp = SPIWrite(ChannelRedundancy_Reg,temp,1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = ANTICOLL; /*���ص�����*/
    temp[1] = 0x20; /*ARG*/
    acktemp = CommandSend(Transceive,temp,2);
    if(acktemp)
    {
        return(-1);
    }
    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 0x5)/*CT SN0 SN1 SN2 BCC1*/
    {
        return(-1);
    }
    acktemp = 0;
    for(i=0; i<5; i++)
    {
        acktemp ^= globalbuf[i];/*ǰ�ĸ��������Ľ��Ӧ�õ��ڵ������*/
    }
    if(acktemp)
    {
        return(-1);
    }
    for(i=0; i<5; i++)
    {
        Card_UID[i] = globalbuf[i];
    }
    return(0);
}

/****************************************************************
**����: SelectTag
**����: ѡ���ض���һ�ſ�
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int SelectTag(void)
{
    unsigned char acktemp, temp[1], i;

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*�Խ��պͷ��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0xf;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    globalbuf[0] = SELECT; /*ѡ��Ƭ����*/
    globalbuf[1] = 0x70; /*ARG*/
    for(i=0; i<5; i++)
    {
        globalbuf[i+2] = Card_UID[i];/*CT SN0 SN1 SN2 BCC1*/
    }
    acktemp = CommandSend(Transceive, globalbuf, 7);
    if(acktemp)
    {
        return(-1);
    }
    acktemp = ReadFIFO(temp);
    if(temp[0] != 0x1)/*�жϽ��յ����ֽ����Ƿ�Ϊ1*/
    {
        return(-1);
    }

    return(0);
}

/****************************************************************
**����: WriteKeytoE2
**����: ����Կд��E2PROM��
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
**˵��: ���ڳ���ʹ��FM1702SL��ƵоƬ������Ҫ���øú�������֤ʱ
ʹ�õ���֤����װ��FM1702SL��EEPROM��
*****************************************************************/

int WriteKeytoE2(void)
{
    unsigned char acktemp, temp[1], i;

    globalbuf[0] = keybuf[0];/*E2PROM��ʼ��ַLSB*/
    globalbuf[1] = keybuf[1];/*E2PROM��ʼ��ַMSB*/
    for(i=1; i<7; i++)
    {
        globalbuf[i+i]=(((keybuf[i+1]&0xf0)>>4)|((~keybuf[i+1])&0xf0));/*key�ĸ�ʽ*/
        globalbuf[1+i+i]=((keybuf[i+1]&0xf)|(~(keybuf[i+1]&0xf)<<4));
    }
    acktemp = CommandSend(WriteE2, globalbuf, 0x0e);/*2byte��ַ+12byte��Կ*/
    if(acktemp)
    {
        return(-1);
    }
    
    msleep_interruptible(4);
    
    acktemp = SPIRead(SecondaryStatus_Reg, temp, 1);/*��ȡ״̬�Ĵ���*/
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] & 0x40)/*E2PROM��������*/
    {
        temp[0] = IDLE;
        acktemp = SPIWrite(Command_Reg, temp, 0x1);/*д���������Idle*/
        if(acktemp)
        {
            return(-1);
        }
        return(0);
    }
    temp[0] = IDLE;
    acktemp = SPIWrite(Command_Reg, temp, 0x1);
    return(-1);
}

/****************************************************************
**����: LoadKeyFromE2
**����: ����Կ��E2PROM���Ƶ�KEY����
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
**˵��: ����ǵ�һ����Ҫͨ����λ���Ȱ���Կд��E2PROM�У�
        �����������WriteKeytoE2����
*****************************************************************/

int LoadKeyFromE2(void)
{
    unsigned char acktemp, temp[2];
    temp[0] = keybuf[0]; /*E2PROM��ʼ��ַLSB*/
    temp[1] = keybuf[1];/*E2PROM��ʼ��ַMSB*/
    acktemp = CommandSend(LoadKeyE2, temp, 0x2);
    if(acktemp)
    {
        return(-1);
    }
    acktemp = SPIRead(ErrorFlag_Reg, temp, 1);/*��ȡ��һ��ָ��Ĵ����ʶ*/
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] & 0x40)/*��������ݲ����Ϲ涨����Կ��ʽ*/
    {
        return(-1);
    }
    return(0);
}

/****************************************************************
**����: LoadKeyFromFifo
**����: ����Կ��FIFO���Ƶ�KEY����
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int LoadKeyFromFifo(void)
{
	unsigned char acktemp, temp[2], i;
    
    for(i=1; i<7; i++)
    {
        globalbuf[i+i]=(((keybuf[i+1]&0xf0)>>4)|((~keybuf[i+1])&0xf0));/*key�ĸ�ʽ*/
        globalbuf[1+i+i]=((keybuf[i+1]&0xf)|(~(keybuf[i+1]&0xf)<<4));
    }
    
    acktemp = CommandSend(LoadKey, &globalbuf[2], 12);
    if(acktemp)
    {
        return(-1);
    }
    
    acktemp = SPIRead(ErrorFlag_Reg, temp, 1);/*��ȡ��һ��ָ��Ĵ����ʶ*/
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] & 0x40)/*��������ݲ����Ϲ涨����Կ��ʽ*/
    {
        return(-1);
    }
    
    return(0);
}    
    
/****************************************************************
**����: AuthenticationA
**����: ��֤��Ƭ����A
**����: sectornum:������(0~15)
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int AuthenticationA(unsigned char sectornum)
{
    unsigned char acktemp, temp[6], i;

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*�Խ��պͷ��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0xf;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
    return(-1);
    }

    temp[0] = AUTHENTA; /*KEYA����֤����*/
    temp[1] = sectornum*4+3;/*������������Կ�ĵ�ַ�����)*/
    for(i=0; i<4; i++)
    {
        temp[2+i] = Card_UID[i];
    }
    acktemp = CommandSend(Authent1, temp, 0x6);/*��֤���̵�һ��*/
    if(acktemp)
    {
        return(-1);
    }
    acktemp = SPIRead(ErrorFlag_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] & 0xe)/*�ж��Ƿ����*/
    {
        return(-1);
    }
    acktemp = CommandSend(Authent2, NULL, 0);/*��֤���̵ڶ���*/
    if(acktemp)
    {
        return(-1);
    }
    acktemp = SPIRead(ErrorFlag_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] & 0xe)/*�ж��Ƿ����*/
    {
        return(-1);
    }
    acktemp = SPIRead(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] & 0x8)/*�жϼ��ܵ�Ԫ�Ƿ��Ѿ���*/
    {
        return(0);
    }
    return(-1);
}

/****************************************************************
**����: AuthenticationB
**����: ��֤��Ƭ����B
**����: sectornum:������(0~15)
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int AuthenticationB(unsigned char sectornum)
{
    unsigned char acktemp, temp[6], i;

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*�Խ��պͷ��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0xf;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = AUTHENTB; /*KEYB����֤����*/
    temp[1] = sectornum*4+3;/*������������Կ�ĵ�ַ�����)*/
    for(i=0; i<4; i++)
    {
        temp[2+i] = Card_UID[i];
    }
    acktemp = CommandSend(Authent1, temp, 0x6);/*��֤���̵�һ��*/
    if(acktemp)
    {
        return(-1);
    }
    acktemp = SPIRead(ErrorFlag_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] & 0xe)/*�ж��Ƿ����*/
    {
        return(-1);
    }
    acktemp = CommandSend(Authent2, NULL, 0);/*��֤���̵ڶ���*/
    if(acktemp)
    {
        return(-1);
    }
    acktemp = SPIRead(ErrorFlag_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0]&0xe)/*�ж��Ƿ����*/
    {
        return(-1);
    }
    acktemp = SPIRead(Control_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0]&0x8)/*�жϼ��ܵ�Ԫ�Ƿ��Ѿ���*/
    {
        return(0);
    }
    return(-1);
}

/****************************************************************
**����: ReadBlock
**����: ��ȡM1��һ������
**����: blocknum:���ַ
**���: �ɹ�����0��ʧ�ܷ���-1
**˵�����������ݴ��globalbuf[]��
*****************************************************************/

int ReadBlock(unsigned char blocknum)
{
    unsigned char acktemp, temp[2];

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*�Խ��պͷ��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0xf;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = READBLOCK;/*������*/
    temp[1] = blocknum;/*���*/
    acktemp = CommandSend(Transceive, temp, 2);
    if(acktemp)
    {
        return(-1);
    }
    acktemp = SPIRead(ErrorFlag_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0]&0xe)/*�ж��Ƿ����*/
    {
        return(-1);
    }
    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 16)/*��ȡ��16���ֽ�*/
    {
        return(-1);
    }
    return(0);
}

/****************************************************************
**����: WriteBlock
**����: ��ȡM1��һ������
**����: blocknum:���ַ
**���: �ɹ�����0��ʧ�ܷ���-1
**˵����д����globalbuf[]��ָ����
*****************************************************************/

int WriteBlock(void)
{
    unsigned char acktemp, temp[2];

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*ֻ�Է��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x7;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = WRITEBLOCK;/*д����*/
    temp[1] = globalbuf[0];/*���*/
    acktemp = CommandSend(Transceive, temp, 2);
    if(acktemp)
    {
        return(-1);
    }

    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 1)
    {
        return(-1);
    }
    if(globalbuf[0] != 0xa)
    {
        return(-1);
    }

    acktemp = CommandSend(Transceive, globalbuf+1, 0x10);/*����16byte����*/
    if(acktemp)
    {
        return(-1);
    }

    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 1)
    {
        return(-1);
    }
    if(globalbuf[0] != 0xa)
    {
        return(-1);
    }

    return(0);
}

/****************************************************************
**����: Increment
**����: �ú���ʵ�ֶ�ֵ�����Ŀ������ֵ����
**����: blocknum:ֵ�����Ŀ��ַ
**���: �ɹ�����0��ʧ�ܷ���-1
**˵����valuebuf��Ϊ�ӵ�ֵ�����ֽ���ǰ
*****************************************************************/

int Increment(void)
{
    unsigned char acktemp, temp[2];

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*�Խ��պͷ��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x7;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = INCREMENT; /*��ֵ����*/
    temp[1] = valuebuf[0];/*���*/
    acktemp = CommandSend(Transceive, temp, 2);
    if(acktemp)
    {
        return(-1);
    }

    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 1)
    {
        return(-1);
    }
    if(globalbuf[0] != 0xa)
    {
        return(-1);
    }

    acktemp = CommandSend(Transmit, valuebuf+1, 4);/*����4byte�ӵ�ֵ�����ֽ���ǰ*/
    if(acktemp)
    {
        return(-1);
    }

    return(0);
}

/****************************************************************
**����: Decrement
**����: �ú���ʵ�ֶ�ֵ�����Ŀ���м�ֵ����
**����: blocknum:ֵ�����Ŀ��ַ
**���: �ɹ�����0��ʧ�ܷ���-1
**˵����valuebuf��Ϊ�ӵ�ֵ�����ֽ���ǰ
*****************************************************************/

int Decrement(void)
{
    unsigned char acktemp, temp[2];

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*ֻ�Է��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x7;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = DECREMENT;/*��ֵ����*/
    temp[1] = valuebuf[0];/*���*/
    acktemp = CommandSend(Transceive, temp, 2);
    if(acktemp)
    {
        return(-1);
    }

    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 1)
    {
        return(-1);
    }
    if(globalbuf[0] != 0xa)
    {
        return(-1);
    }

    acktemp = CommandSend(Transmit, valuebuf+1, 4);/*����4byte����ֵ�����ֽ���ǰ*/
    if(acktemp)
    {
        return(-1);
    }

    return(0);
}

/****************************************************************
**����: Restore
**����: �ú���ʵ��MIFARE���Զ��ָ�,���ݲ�
**����: blocknum:��Ƭ�Ͻ��������ݵĿ��ַ
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int Restore(unsigned char blocknum)
{
    unsigned char acktemp, temp[2], i;

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*ֻ�Է��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x7;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = PRESTORE;/*�ش�����*/
    temp[1] = blocknum;/*���*/
    acktemp = CommandSend(Transceive, temp, 2);
    if(acktemp)
    {
        return(-1);
    }

    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 1)
    {
        return(-1);
    }
    if(globalbuf[0] != 0xa)
    {
        return(-1);
    }

    for(i=0; i<4; i++) 
    {
        globalbuf[i] = 0x00;
    }
    
    acktemp = CommandSend(Transmit, globalbuf, 4);
    if(acktemp)
    {
        return(-1);
    }
    
    return(0);
}

/****************************************************************
**����: Transfer
**����: �ú���ʵ��MIFARE������Ǯ���������
**����: blocknum:�ڲ��Ĵ��������ݽ���ŵĵ�ַ
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int Transfer(unsigned char blocknum)
{
    unsigned char acktemp, temp[2];

    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*ֻ�Է��͵����ݽ���CRCУ�顢*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x7;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = TRANSFER;/*��������*/
    temp[1] = blocknum;/*���*/
    acktemp = CommandSend(Transceive, temp, 2);
    if(acktemp)
    {
        return(-1);
    }

    acktemp = ReadFIFO(temp);
    if(acktemp)
    {
        return(-1);
    }
    if(temp[0] != 1)
    {
        return(-1);
    }
    if(globalbuf[0] != 0xa)
    {
        return(-1);
    }

    return(0);
}

/****************************************************************
**����: Halt
**����: �ú���ʵ����ͣMIFARE��
**����: ��
**���: �ɹ�����0��ʧ�ܷ���-1
*****************************************************************/

int Halt(void)
{
    unsigned char acktemp, temp[2];

    /*temp[0] = 0x63;
    acktemp = SPIWrite(CRCPresetLSB_Reg, temp, 1);
    if(acktemp)
    {		
        return(-1);
    }
    
    temp[0] = 0x3f;
    acktemp = SPIWrite(CWConductance_Reg, temp, 1);
    if(acktemp)
    {		
        return(-1);
    }*/
    
    /*��������λ�Ƚ�CRCЭ��������CRC�㷨ΪISO1443A������16bitCRC��*/
    /*���չ��̲�����CRC��������CRC��*/
    /*��У�顢���ͺͽ��ն�������żУ��*/
    temp[0] = 0x7;
    acktemp = SPIWrite(ChannelRedundancy_Reg, temp, 1);
    if(acktemp)
    {
        return(-1);
    }

    temp[0] = HALT;/*ͣ������*/
    temp[1] = 0x00;
    acktemp = CommandSend(Transmit, temp, 2);
    if(acktemp)
    {
        return(-1);
    }
    
    msleep_interruptible(5);

    return(0);
}       
    
#if 0
/*************************************************************************
**                            RF�߼�����
*************************************************************************/
int rf_card(request)
{
    unsigned char acktemp;

    if(request)
    {
        acktemp = RequestAll();
    }
    else
    {
        acktemp = RequestStd();
    }

    if(acktemp)
    {
        return(-1);
    }

    acktemp = GetUID();
    if(acktemp)
    {
        return(-1);
    }

    acktemp = SelectTag();
    if(acktemp)
    {
        return(-1);
    }

    return(0);
}
#endif

/*********************************************************************************************************
** Function name: fm1702sl_cdev_setup
** Descriptions:  Create a cdev for fm1702sl
** Input: *dev:   fm1702sl's device struct pointer
**        devno:  fm1702sl's device number
** Output :
********************************************************************************************************/

static void  fm1702sl_cdev_setup(struct fm1702sl_device *dev, dev_t devno)
{
    int err;

    cdev_init(&dev->cdev, &fm1702sl_fops);        /*��ʼ��cdev�ṹ*/

    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &fm1702sl_fops;
    err = cdev_add(&dev->cdev, devno, 1);        /*���ַ��豸���뵽�ں˵��ַ��豸�����У�chrdev[]��*/

    if(err != 0)
    {
        printk(KERN_NOTICE "Error %d adding fm1702sl device, major_%d.\n", err, MAJOR(devno));
    }

    return;
}

/*********************************************************************************************************
** Function name: fm1702sl_open
** Descriptions:  fm1702sl open function
                  App layer will invoke this interface to open fm1702sl device, just set a
                  flag which comes from device struct.
** Input:inode:   information of device
**       filp:    pointer of file
** Output 0:      OK
**        other:  not OK
********************************************************************************************************/

int fm1702sl_open(struct inode *inode, struct file *filp)
{
    static struct fm1702sl_device *dev;

    dev = container_of(inode->i_cdev, struct fm1702sl_device, cdev); /*�õ�����ĳ���ṹ��Ա�Ľṹ��ָ��*/
    filp->private_data = dev; /* for other methods */                  /*���豸�ṹ��ָ�븳ֵ���ļ�˽������ָ��*/

    if(test_and_set_bit(0, &dev->is_open) != 0)                        /*����ĳһλ�����ظ�λԭ����ֵ*/
    {
        return -EBUSY;
    }

    return(0); /* success. */
}

/*********************************************************************************************************
** Function name: fm1702sl_release
** Descriptions:  fm1702sl release function
                  App layer will invoke this interface to release fm1702sl device, just clear a
                  flag which comes from device struct.
** Input:inode:   information of device
**       filp:    pointer of file
** Output 0:      OK
**        other:  not OK
********************************************************************************************************/

int fm1702sl_release(struct inode *inode, struct file *filp)
{
    struct fm1702sl_device *dev = filp->private_data;                /*����豸�ṹ��ָ��*/

    if(test_and_clear_bit(0, &dev->is_open) == 0)   /* release lock, and check... */
    {
        return -EINVAL;     /* already released: error */
    }

    return(0);
}

/*********************************************************************************************************
** Function name: fm1702sl_ioctl
** Descriptions:  fm1702sl ioctl function
                  App layer will invoke this interface to control fm1702sl chip
** Input:inode:   information of device
**       filp:    pointer of file
**       cmd:     command
**       arg:     additive parameter
** Output 0:      OK
**        other:  not OK
********************************************************************************************************/

int fm1702sl_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret=0;
    unsigned char temp;

    /*��������������ݴ���*/
    if (_IOC_TYPE(cmd) != HNDL_AT91_IOC_MAGIC)          /*�ж��Ƿ����ڸ��豸*/
    {
        return -ENOTTY;
    }

    if (_IOC_NR(cmd) > HNDL_AT91_IOC_MAXNR )            /*�ж��Ƿ񳬹������������*/
    {
        return -ENOTTY;
    }

    switch(cmd)                                            /*���ݲ�ͬ�Ĳ���cmd���Ͳ�ͬ������*/
    {
        case IOCTL_FM1702SL_RESET:
            Fm1702slReset();
            break;
        
        case IOCTL_FM1702SL_SPIINIT:
            ret = Fm1702slSpiinit();
            break;

        case IOCTL_FM1702SL_INIT:
            ret = Fm1702slInit();
            break;

        case IOCTL_FM1702SL_REQUESTALL:
            ret = RequestAll();
            if(ret)
            {
                return -1;
            }
            
            ret = copy_to_user((unsigned char *)arg, globalbuf, 2);
            if(ret)
            {
                return -EFAULT;
            }

            break;

        case IOCTL_FM1702SL_REQUESTIDLE:
            ret = RequestIdle();
            if(ret)
            {
                return -1;
            }
            
            ret = copy_to_user((unsigned char *)arg, globalbuf, 2);
            if(ret)
            {
                return -EFAULT;
            }

            break;

        case IOCTL_FM1702SL_GETUID:
            ret = GetUID();
            break;

        case IOCTL_FM1702SL_SELECTTAG:
            ret = SelectTag();
            if(ret)
            {
                return(-1);
            }
            ret = put_user(globalbuf[0], (unsigned char *)arg);
            if(ret)
            {
                return -EFAULT;
            }
            break;

        case IOCTL_FM1702SL_WRITEKEYTOE2:
            ret = copy_from_user(keybuf, (unsigned char *)arg, 8);
            if(ret)
            {
                return -EFAULT;
            } 
 
            ret = WriteKeytoE2();

            break;

        case IOCTL_FM1702SL_LOADKEYE2:
            ret = copy_from_user(keybuf, (unsigned char *)arg, 2);
            if(ret)
            {
                return -EFAULT;
            } 

            ret = LoadKeyFromE2();
            if(ret)
            {
                return -1;
            }
            break;

	    case IOCTL_FM1702SL_LOADKEYFIFO:
            ret = copy_from_user(&keybuf[2], (unsigned char *)arg, 6);
            if(ret)
            {
                return -EFAULT;
            }
 
            ret = LoadKeyFromFifo();

            break;

        case IOCTL_FM1702SL_AUTHENTICATIONA:
            temp = arg & 0xff;
            
            ret = AuthenticationA(temp);

            break;

         case IOCTL_FM1702SL_AUTHENTICATIONB:
            temp = arg & 0xff;
            
            ret = AuthenticationB(temp);
            
            break;

        case IOCTL_FM1702SL_WRITEBLOCK:
            ret = copy_from_user(globalbuf, (unsigned char *)arg, 17);/*��1��byte��д���blocknum*/
            if(ret)
            {
                return -EFAULT;
            }

            ret = WriteBlock();
            
            break;

        case IOCTL_FM1702SL_READBLOCK:
            if (get_user(temp, (unsigned char *)arg))/*��1��byte����Ҫ����blocknum*/
            {
                return -EFAULT;
            }
            
            ret = ReadBlock(temp);
            if(ret)
            {
                return -1;
            }
            
            ret = copy_to_user((unsigned char *)(arg+1), globalbuf, 16);/*������16byte���ݷ��ں�����û��ռ�*/
            if(ret)
            {
                return -EFAULT;
            }

            break;

        case IOCTL_FM1702SL_INCREMENT:
            ret = copy_from_user(valuebuf, (unsigned char *)arg, 5);/*��1��byte��д���blocknum*/
            if(ret)
            {
                return -EFAULT;
            }
  
            ret = Increment();
            
            break;

        case IOCTL_FM1702SL_DECREMENT:
            ret = copy_from_user(valuebuf, (unsigned char *)arg, 5);/*��1��byte��д���blocknum*/
            if(ret)
            {
                return -EFAULT;
            }
 
            ret = Decrement();
            
            break;

        case IOCTL_FM1702SL_RESTORE:
            temp = arg & 0xff;
                
            ret = Restore(temp);
                       
            break;

        case IOCTL_FM1702SL_TRANSFER:
            temp = arg & 0xff;
                
            ret = Transfer(temp);
            
            break;

        case IOCTL_FM1702SL_HALT:
            ret = Halt();
            break;
            
        case IOCTL_FM1702SL_STANDBY:
            ret = Fm1702slStandby();
            break;  

        case IOCTL_FM1702SL_WAKEUP:
            ret = Fm1702slWakeup();
            break; 
            
        default:
            return -ENOTTY;
    }

    return ret;
}

/*********************************************************************************************************
** Function name: fm1702sl_remove
** Descriptions:  fm1702sl remove function
                  SPI core will invoke this interface to remove fm1702sl
** Input: *spi    spi core device struct pointer
** Output 0:      OK
**        other:  not OK
********************************************************************************************************/

static int  fm1702sl_remove(struct spi_device *spi)
{
    dev_t devno = MKDEV(fm1702sl_major, fm1702sl_minor);  /*�����豸�źʹ��豸����ϳ��豸��*/
    struct class* myclass;

    if(fm1702sl_devp != 0)
    {
        /* Get rid of our char dev entries */
        cdev_del(&fm1702sl_devp->cdev);                          /*��ϵͳ���Ƴ�һ���ַ��豸*/

        myclass = fm1702sl_devp->myclass;
        if(myclass != 0)
        {
            class_device_destroy(myclass, devno);                /*����һ�����豸*/
            class_destroy(myclass);                                /*����һ����*/
        }

        kfree(fm1702sl_devp);                                                   /*�ͷ�fm1702sl_probe������������ڴ�*/
        fm1702sl_devp = NULL;
    }

    /* cleanup_module is never called if registering failed */
    unregister_chrdev_region(devno, 1);                    /*�ͷŷ����һϵ���豸��*/
    return(0);
}

/*********************************************************************************************************
** Function name: fm1702sl_probe
** Descriptions:  fm1702sl probe function
                  SPI core will invoke this probe interface to init fm1702sl.
** Input: *spi    spi core device struct pointer
** Output 0:      OK
**        other:  not OK
********************************************************************************************************/

static int __devinit fm1702sl_probe(struct spi_device *spi)
{
    int result  = 0;
    dev_t dev   = 0;
    struct class* myclass;

     /*
     * Get a range of minor numbers to work with, asking for a dynamic
     * major unless directed otherwise at load time.
     */
    if(fm1702sl_major)            /*֪�����豸��*/
    {
        dev = MKDEV(fm1702sl_major, fm1702sl_minor);
        result = register_chrdev_region(dev, 1, FM1702SL_DEV_NAME);
    }
    else                        /*��֪�����豸�ţ���̬����*/
    {
        result = alloc_chrdev_region(&dev, fm1702sl_minor, 1, FM1702SL_DEV_NAME);
        fm1702sl_major = MAJOR(dev);

    }

    if(result < 0)
    {
        printk(KERN_WARNING "hndl_kb: can't get major %d\n", fm1702sl_major);
        return result;
    }

    /* allocate the devices -- we do not have them static. */
    fm1702sl_devp = kmalloc(sizeof(struct fm1702sl_device), GFP_KERNEL);   /*Ϊfm1702sl�豸�ṹ�������ڴ�*/
    if(!fm1702sl_devp)
    {
        /* Can not malloc memory for fm1702sl */
        printk("fm1702sl Error: Can not malloc memory\n");
        fm1702sl_remove(spi);
        return -ENOMEM;
    }
    memset(fm1702sl_devp, 0, sizeof(struct fm1702sl_device));

    init_MUTEX(&fm1702sl_devp->lock);                                        /*��ʼ���ź���*/

    /* Register a class_device in the sysfs. */
    myclass = class_create(THIS_MODULE, FM1702SL_DEV_NAME);             /*class.c�У���ϵͳ�н���һ����*/
    if(NULL == myclass)
    {
        printk("fm1702sl Error: Can not create class\n");
        fm1702sl_remove(spi);
        return result;
    }

    class_device_create(myclass, NULL, dev, NULL, FM1702SL_DEV_NAME);

    fm1702sl_devp->myclass = myclass;

    fm1702sl_cdev_setup(fm1702sl_devp, dev);

    fm1702sl_devp->spi = spi;

    HNOS_DEBUG_INFO("Initialized device %s, major %d.\n", FM1702SL_DEV_NAME, fm1702sl_major);

    return(0);
}

/*fm1702sl���������ӵ�spi ��������*/
static struct spi_driver fm1702sl_driver = {
    .driver = {
        .name   = "FM1702SL",
        .owner  = THIS_MODULE,
    },
    .probe  = fm1702sl_probe,
    .remove = __devexit_p(fm1702sl_remove),
};

/*********************************************************************************************************
** Function name: Fm1702slInit
** Descriptions:  fm1702sl init function
                  register fm1702sl driver to SPI core
** Input:none
** Output 0:      OK
**        other:  not OK
********************************************************************************************************/

static int __init fm1702sl_init(void)
{
    int ret;

    ret = spi_register_driver(&fm1702sl_driver);                 /*spi.c�У�driver_register*/
		
    return ret;
}

/*********************************************************************************************************
** Function name: fm1702sl_exit
** Descriptions:  fm1702sl exit function
                  unregister fm1702sl driver from SPI core
** Input:none
** Output none
********************************************************************************************************/

static void __exit fm1702sl_exit(void)
{
    spi_unregister_driver(&fm1702sl_driver);                     /*spi.h�У�driver_unregister*/

    return;
}

module_init(fm1702sl_init);
module_exit(fm1702sl_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("kernel team");
