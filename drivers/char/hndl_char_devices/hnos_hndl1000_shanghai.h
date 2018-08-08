/*
 * drivers/char/hndl_char_devices/hnos_defines_hntt1800x.h 
 *
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */
#ifndef __HNOS_DEFINES_HNTT1000_SHANGHAI_H 
#define __HNOS_DEFINES_HNTT1000_SHANGHAI_H 

/* HNTT1800X ң�� ͨ������ */
#define		INPUT_STATUS_0			(1 << 0)      /* ״̬��1 */
#define		INPUT_STATUS_1			(1 << 1)      /* ״̬��2 */
#define		INPUT_OPEN_COVER		(1 << 2)      /* �˸ǿ��Ǽ�� */
#define		INPUT_OPEN_GLASS		(1 << 3)      /* ��͸�����Ǽ�� */
#define		INPUT_TDK6513_STATE		(1 << 4)      /* У��״̬ */
#define		INPUT_ADSORB_IRDA		(1 << 5)      /* �����ж� */

#define		INPUT_SMCBUS_OFFSET		16              /* (������չ)ң������ӵ�16·��ʼ */
#define		INPUT_SMCBUS_SIZE		16              /* (������չ)ң�Ź���16· */

/* HNTT1800X ң�� ͨ������ */
#define		OUTPUT_CTRL_0			(1 << 0)      /* ���ɿ������ */
#define		OUTPUT_CTRL_1			(1 << 1)      /* �澯��� */
#define		OUTPUT_REMOTE_POWER		(1 << 2)      /* ң�ظ澯��Դ����, д1�򿪵�Դ */
#define		OUTPUT_REMOTE_ENABLE		(1 << 3)      /* ң�ظ澯����, д0���� */
#define		OUTPUT_PLC_POWER		(1 << 4)      /* �ز���Դ���� */

#define		OUTPUT_SMCBUS_OFFSET		16              /* (������չ)ң������ӵ�16·��ʼ */
#define		OUTPUT_SMCBUS_SIZE		16              /* (������չ)ң�Ź���16· */

#define		NCHANNEL_PER_SMCBUS		8      
#define		NR_SMCBUS			2

#endif
