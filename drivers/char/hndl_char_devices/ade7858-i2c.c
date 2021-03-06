/*
 * ADE7854/58/68/78 Polyphase Multifunction Energy Metering IC Driver (I2C Bus)
 *
 * Copyright 2010 Analog Devices Inc.
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include "ade7858.h"

static int ade7858_i2c_write_reg_8(struct device *dev,
		u16 reg_address,
		u8 value)
{
	int ret;
	struct ade7858_state *st = dev_get_drvdata(dev);

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;
	st->tx[2] = value;

	ret = i2c_master_send(st->i2c, st->tx, 3);
	mutex_unlock(&st->buf_lock);

	return ret;
}

static int ade7858_i2c_write_reg_16(struct device *dev,
		u16 reg_address,
		u16 value)
{
	int ret;
	struct ade7858_state *st = dev_get_drvdata(dev);

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;
	st->tx[2] = (value >> 8) & 0xFF;
	st->tx[3] = value & 0xFF;

	ret = i2c_master_send(st->i2c, st->tx, 4);
	mutex_unlock(&st->buf_lock);

	return ret;
}

static int ade7858_i2c_write_reg_24(struct device *dev,
		u16 reg_address,
		u32 value)
{
	int ret;
	struct ade7858_state *st = dev_get_drvdata(dev);

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;
	st->tx[2] = (value >> 16) & 0xFF;
	st->tx[3] = (value >> 8) & 0xFF;
	st->tx[4] = value & 0xFF;

	ret = i2c_master_send(st->i2c, st->tx, 5);
	mutex_unlock(&st->buf_lock);

	return ret;
}

static int ade7858_i2c_write_reg_32(struct device *dev,
		u16 reg_address,
		u32 value)
{
	int ret;
	struct ade7858_state *st = dev_get_drvdata(dev);

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;
	st->tx[2] = (value >> 24) & 0xFF;
	st->tx[3] = (value >> 16) & 0xFF;
	st->tx[4] = (value >> 8) & 0xFF;
	st->tx[5] = value & 0xFF;

	ret = i2c_master_send(st->i2c, st->tx, 6);
	mutex_unlock(&st->buf_lock);

	return ret;
}

static int ade7858_i2c_read_reg_8(struct device *dev,
		u16 reg_address,
		u8 *val)
{
	struct ade7858_state *st = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;

	ret = i2c_master_send(st->i2c, st->tx, 2);
	if (ret!=2)
		goto out;

	ret = i2c_master_recv(st->i2c, st->rx, 1);
	if (ret!=1)
		goto out;
	*val = st->rx[0];
out:
	mutex_unlock(&st->buf_lock);
	return ret;
}

static int ade7858_i2c_read_reg_16(struct device *dev,
		u16 reg_address,
		u16 *val)
{
	struct ade7858_state *st = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;

	ret = i2c_master_send(st->i2c, st->tx, 2);
	if (ret!=2)
		goto out;

	ret = i2c_master_recv(st->i2c, st->rx, 2);
	if (ret!=2)
		goto out;
	*val = (st->rx[0] << 8) | st->rx[1];
out:
	mutex_unlock(&st->buf_lock);
	return ret;
}

static int ade7858_i2c_read_reg_24(struct device *dev,
		u16 reg_address,
		u32 *val)
{
	struct ade7858_state *st = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;

	ret = i2c_master_send(st->i2c, st->tx, 2);
	if (ret!=2)
		goto out;

	ret = i2c_master_recv(st->i2c, st->rx, 3);
	if (ret!=3)
		goto out;
	*val = (st->rx[0] << 16) | (st->rx[1] << 8) | st->rx[2];
out:
	mutex_unlock(&st->buf_lock);
	return ret;
}

static int ade7858_i2c_read_reg_32(struct device *dev,
		u16 reg_address,
		u32 *val)
{
	struct ade7858_state *st = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&st->buf_lock);
	st->tx[0] = (reg_address >> 8) & 0xFF;
	st->tx[1] = reg_address & 0xFF;

	ret = i2c_master_send(st->i2c, st->tx, 2);
//	printk("send ret %d\n",ret);
	if (ret!=2)
		goto out;

	ret = i2c_master_recv(st->i2c, st->rx, 4);
//	printk("recv ret %d\n",ret);
	if (ret!=4)
		goto out;
	*val = (st->rx[0] << 24) | (st->rx[1] << 16) | (st->rx[2] << 8) | st->rx[3];
out:
	mutex_unlock(&st->buf_lock);
	return ret;
}

static int __devinit ade7858_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct ade7858_state *st = kzalloc(sizeof *st, GFP_KERNEL);
	printk("ade7858_i2c_probe\n");
	if (!st) {
		ret =  -ENOMEM;
		return ret;
	}

	i2c_set_clientdata(client, st);
	st->read_reg_8 = ade7858_i2c_read_reg_8;
	st->read_reg_16 = ade7858_i2c_read_reg_16;
	st->read_reg_24 = ade7858_i2c_read_reg_24;
	st->read_reg_32 = ade7858_i2c_read_reg_32;
	st->write_reg_8 = ade7858_i2c_write_reg_8;
	st->write_reg_16 = ade7858_i2c_write_reg_16;
	st->write_reg_24 = ade7858_i2c_write_reg_24;
	st->write_reg_32 = ade7858_i2c_write_reg_32;
	st->i2c = client;
	st->irq = client->irq;

	ret = ade7858_probe(st, &client->dev);
	if (ret) {
		kfree(st);
		return ret;
	}

	return ret;
}
#if 0
static int __devexit ade7858_i2c_remove(struct i2c_client *client)
{
	return ade7858_remove(i2c_get_clientdata(client));
}
#endif
static const struct i2c_device_id ade7858_id[] = {
	{  1 },
	{  2 },
	{  3 },
	{  4 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ade7858_id);

static struct i2c_driver ade7858_i2c_driver = {
	.driver = {
		.name = "ade7858-i2c",
	},
	.probe    = ade7858_i2c_probe,
#if 0
	.remove   = __devexit_p(ade7858_i2c_remove),
#endif
	.id	  = 2,
//	.id_table = ade7858_id,
};

static __init int ade7858_i2c_init(void)
{
	return i2c_add_driver(&ade7858_i2c_driver);
}
module_init(ade7858_i2c_init);

static __exit void ade7858_i2c_exit(void)
{
	i2c_del_driver(&ade7858_i2c_driver);
}
module_exit(ade7858_i2c_exit);


MODULE_AUTHOR("zhuzl <zzltjjd@163.com>");
MODULE_DESCRIPTION("Analog Devices ADE7854/58/68/78 Polyphase Multifunction Energy Metering IC I2C Driver");
MODULE_LICENSE("GPL v2");
