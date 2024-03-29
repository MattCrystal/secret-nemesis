/*
 * Base from Chad Goodman, chad.goodman@gmail.com
 * modifications to base from faux123
 * Copyright 2012 Chad Goodman
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/earlysuspend.h>
#include <linux/mutex.h>

#include <linux/writeback.h>

#define DYN_FSYNC_VERSION 2

/*
 * fsync_mutex protects dyn_fsync_active during early suspend / lat resume transitions
 */
static DEFINE_MUTEX(fsync_mutex);

bool early_suspend_active = false;
static bool dyn_fsync_active = true;

static ssize_t dyn_fsync_active_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (dyn_fsync_active ? 1 : 0));
}

static ssize_t dyn_fsync_active_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;

	if(sscanf(buf, "%u\n", &data) == 1) {
		if (data == 1) {
			pr_info("%s: dynamic fsync enabled\n", __FUNCTION__);
			dyn_fsync_active = true;
		}
		else if (data == 0) {
			pr_info("%s: dyanamic fsync disabled\n", __FUNCTION__);
			dyn_fsync_active = false;
		}
		else
			pr_info("%s: bad value: %u\n", __FUNCTION__, data);
	} else
		pr_info("%s: unknown input!\n", __FUNCTION__);

	return count;
}

static ssize_t dyn_fsync_version_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "version: %u\n", DYN_FSYNC_VERSION);
}

static ssize_t dyn_fsync_earlysuspend_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "early suspend active: %u\n", early_suspend_active);
}

static struct kobj_attribute dyn_fsync_active_attribute = 
	__ATTR(Dyn_fsync_active, 0666, dyn_fsync_active_show, dyn_fsync_active_store);

static struct kobj_attribute dyn_fsync_version_attribute = 
	__ATTR(Dyn_fsync_version, 0444 , dyn_fsync_version_show, NULL);

static struct kobj_attribute dyn_fsync_earlysuspend_attribute = 
	__ATTR(Dyn_fsync_earlysuspend, 0444 , dyn_fsync_earlysuspend_show, NULL);

static struct attribute *dyn_fsync_active_attrs[] =
	{
		&dyn_fsync_active_attribute.attr,
		&dyn_fsync_version_attribute.attr,
		&dyn_fsync_earlysuspend_attribute.attr,
		NULL,
	};

static struct attribute_group dyn_fsync_active_attr_group =
	{
		.attrs = dyn_fsync_active_attrs,
	};

static struct kobject *dyn_fsync_kobj;

void dyn_fsync_early_suspend(struct early_suspend *h)
{
	mutex_lock(&fsync_mutex);
	if (dyn_fsync_active) {
		early_suspend_active = true;
#if 1
		/* flush all outstanding buffers */
		wakeup_flusher_threads(0,0);
		//sync_filesystems(0);
		//sync_filesystems(1);
#endif
	}
	mutex_unlock(&fsync_mutex);
}

void dyn_fsync_late_resume(struct early_suspend *h)
{
	mutex_lock(&fsync_mutex);
	early_suspend_active = false;
	mutex_unlock(&fsync_mutex);
}

static struct early_suspend dyn_fsync_early_suspend_handler = 
	{
		//.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
		//.suspend = dyn_fsync_early_suspend,
		//.resume = dyn_fsync_late_resume,
	};

static int dyn_fsync_init(void)
{
	int sysfs_result;

	register_early_suspend(&dyn_fsync_early_suspend_handler);

	dyn_fsync_kobj = kobject_create_and_add("dyn_fsync", kernel_kobj);
	if (!dyn_fsync_kobj) {
		pr_err("%s dyn_fsync kobject create failed!\n", __FUNCTION__);
		return -ENOMEM;
        }

	sysfs_result = sysfs_create_group(dyn_fsync_kobj, &dyn_fsync_active_attr_group);

        if (sysfs_result) {
		pr_info("%s dyn_fsync sysfs create failed!\n", __FUNCTION__);
		kobject_put(dyn_fsync_kobj);
	}
	return sysfs_result;
}

static void dyn_fsync_exit(void)
{
	unregister_early_suspend(&dyn_fsync_early_suspend_handler);

	if (dyn_fsync_kobj != NULL)
		kobject_put(dyn_fsync_kobj);
}

module_init(dyn_fsync_init);
module_exit(dyn_fsync_exit);

MODULE_AUTHOR("Chad Goodman <chad.goodman@gmail.com>");
MODULE_DESCRIPTION("F/Sync Control");
MODULE_LICENSE("GPL");
