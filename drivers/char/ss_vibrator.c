/*
*	drivers/misc/ss_vibrator.c
*	Samsung vibrator driver for general purpose
*	Copyright (C) 2012, Samsung Electronics Co, Ltd
*	
*	This program is free software, you can reditribute it and/or modify
*	it under the terms of the GNU General Public License version 2 as 
*	published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/bcmpmu.h>
#include <linux/wakelock.h>
#include "../staging/android/timed_output.h"

typedef struct
{
	struct timed_output_dev timed_dev;
	struct timer_list vib_timer;
	struct work_struct off_work;
	struct regulator *vib_regulator;
	const char *vib_vcc;
}t_vib_desc;

static t_vib_desc vib_desc;

#define VIB_VCC "hv4"
#define MIN_TIMEOUT 150
#define MAX_TIMEOUT 500
#define DEFAULT_TIMEOUT 170

static void vibrator_control(t_vib_desc *vib_iter, unsigned char onoff)
{

	printk("%s : Vibrator %s\n", __func__, (onoff)?"on":"off");

	if( onoff == 1)
	{
		if(!regulator_is_enabled(vib_iter->vib_regulator))
		{
			regulator_enable(vib_iter->vib_regulator);
		}
	}
	else if( onoff == 0)
	{
		if(regulator_is_enabled(vib_iter->vib_regulator))
		{
			regulator_disable(vib_iter->vib_regulator);
		}
	}
}

static void vibrator_enable_set_timeout(struct timed_out_dev *sdev, int timeout)
{
	t_vib_desc *vib_iter=container_of(sdev, t_vib_desc, timed_dev);
	int valid_timeout;

	if(timeout == 0)
	{
		vibrator_control(vib_iter, 0);
		return;
	}

	vibrator_control(vib_iter, 1);

	valid_timeout=(timeout > MIN_TIMEOUT) ? timeout: DEFAULT_TIMEOUT;
	printk(KERN_INFO "%s : Vibrator timeout = %d \n", __func__, valid_timeout);

	mod_timer(&vib_iter->vib_timer, jiffies + msecs_to_jiffies(valid_timeout));
}

static void vibrator_off_work_func(struct work_struct *work)
{
	t_vib_desc *vib_iter=container_of(work, t_vib_desc, off_work);

	vibrator_control(vib_iter, 0);
}

static void on_vibrate_timer_expired(unsigned long x)
{
	t_vib_desc *vib_iter = (t_vib_desc *)x;
	
	schedule_work(&vib_iter->off_work);
}

static void vibrator_get_remaining_time(struct timed_output_dev *sdev)
{
	t_vib_desc *vib_iter=container_of(sdev, t_vib_desc, timed_dev);
	int retTime=jiffies_to_msecs(jiffies-vib_iter->vib_timer.expires);
	printk(KERN_INFO "Vibrator : remaining time : %dms \n", retTime);
}

static int __devexit vibrator_probe(struct platform_device *pdev)
{
	t_vib_desc *vib_iter;
	int ret=0;

	/* vib_iter=kzalloc(sizeof(t_vib_desc), GFP_KERNEL);
	if(vib_iter == NULL)
	{
		pr_err("%s : memory allocation failure \n", __func__);
		return -ENOMEM;
	} */
	vib_iter=&vib_desc;

	vib_iter->vib_vcc=(const char *)(pdev->dev.platform_data);
	printk(KERN_INFO "%s: Vibrator vcc=%s \n", __func__, vib_iter->vib_vcc);

	//vib_iter->vib_regulator=regulator_get(NULL, VIB_VCC);
	vib_iter->vib_regulator=regulator_get(NULL, vib_iter->vib_vcc);
	if(IS_ERR(vib_iter->vib_regulator))
	{
		printk(KERN_INFO "%s: failed to get regulator \n", __func__);
		return -ENODEV;
	}

	vib_iter->timed_dev.name="vibrator";
	vib_iter->timed_dev.enable=vibrator_enable_set_timeout;
	vib_iter->timed_dev.get_time=vibrator_get_remaining_time;
	
	ret = timed_output_dev_register(&vib_iter->timed_dev);
	if(ret < 0)
	{
		printk(KERN_ERR "Vibrator: timed_output dev registration failure\n");
		timed_output_dev_unregister(&vib_iter->timed_dev);
	}

	init_timer(&vib_iter->vib_timer);
	vib_iter->vib_timer.function = on_vibrate_timer_expired;
	vib_iter->vib_timer.data = (unsigned long)vib_iter;

	platform_set_drvdata(pdev, vib_iter);

	INIT_WORK(&vib_iter->off_work, vibrator_off_work_func);

	printk("%s : ss vibrator probe\n", __func__);	
	return 0;
}

static int __devexit vibrator_remove(struct platform_device *pdev)
{
	t_vib_desc *vib_iter = platform_get_drvdata(pdev);
	timed_output_dev_unregister(&vib_iter->timed_dev);
	regulator_put(vib_iter->vib_regulator);
	return 0;
}

static struct platform_driver vibrator_driver = {
	.probe = vibrator_probe,
	.remove = vibrator_remove,
	.driver = {
		.name = "vibrator",
		.owner = THIS_MODULE,
	},
};

static void __init vibrator_init(void)
{
	platform_driver_register(&vibrator_driver);
}

static void __exit vibrator_exit(void)
{
	platform_driver_unregister(&vibrator_driver);
}

//module_init(vibrator_init);
late_initcall(vibrator_init);
module_exit(vibrator_exit);

MODULE_DESCRIPTION("Samsung Vibrator driver");
MODULE_LICENSE("GPL");
