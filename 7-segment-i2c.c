#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/proc_fs.h>
#include <linux/sysfs.h>

#include <linux/device/driver.h>
#include <linux/i2c.h>
#include <linux/of.h>

#include "7-segment.h"

extern ssize_t seven_segment_write_proc_file(struct seven_segment_display *ssd, struct file* f, const char __user* buf, size_t sz);
extern ssize_t seven_segment_read_proc_file(struct seven_segment_display *ssd, struct file* f, char __user** buf, loff_t **off);
extern int seven_segment_register_top_proc_dir(void);
extern void seven_segment_create_proc_files(struct proc_dir_entry* parent, struct proc_ops* pops);

extern struct proc_dir_entry *procparent;

static size_t i2c_client_counter = 0;
static struct i2c_client* clients[SEVENSEGMENT_MAX_CLIENTS];

static size_t seven_segment_get_free_idx(void){
    if (!i2c_client_counter || !clients[0]){
        return 0;
    }

    if (!clients[1])
        return 1;

    return 2;
}

static ssize_t seven_segment_get_client_idx(struct i2c_client *client){
    ssize_t i;
    for (i = 0; i < SEVENSEGMENT_MAX_CLIENTS; ++i){
        if (clients[i] == client)
            return i;
    }
    return -1;
}


static ssize_t seven_segment_proc_write(struct file* f, const char __user* buf, size_t sz, loff_t* off){
    size_t idx;
    struct seven_segment_display* ssd;
    struct i2c_client* client;

    idx = f->f_path.dentry->d_parent->d_iname[0] - 48;
    client = clients[idx];
    ssd = i2c_get_clientdata(client);

    sz = seven_segment_write_proc_file(ssd, f, buf, sz);
    return sz;
}

ssize_t	seven_segment_proc_read(struct file *f, char __user *buf, size_t sz, loff_t *off){
    ssize_t ret;
    size_t idx;
    struct seven_segment_display* ssd;
    struct i2c_client *client;
    idx = f->f_path.dentry->d_parent->d_iname[0] - 48;
    client = clients[idx];
    ssd = i2c_get_clientdata(client);
    ret = seven_segment_read_proc_file(ssd, f, &buf, &off);

    return ret;
}

static struct proc_ops pops = {
    .proc_write = seven_segment_proc_write,
    .proc_read = seven_segment_proc_read
};

static int seven_segment_probe(struct i2c_client *client){
    int client_idx;
    char procfsname[2];

    if (i2c_client_counter >= SEVENSEGMENT_MAX_CLIENTS - 1){
        pr_err("Too many displays present! Max 3 are allowed.\n");
        return -ENFILE;
    }

    client_idx = seven_segment_get_free_idx();
    clients[client_idx] = client;
    ++i2c_client_counter;

    procfsname[0] = client_idx + 48;
    procfsname[1] = 0;

    if (!procparent){
        pr_err("procparent doesn't exist!\n");
        return -ENOMEM;
    }

    struct seven_segment_display *ssd = kmalloc(sizeof(struct seven_segment_display), GFP_KERNEL);
    ssd->procfolder = proc_mkdir(procfsname, procparent);
    ssd->device.i2c = client;
    ssd->device_type = SEVENSEGMENT_I2C;

    if (!ssd->procfolder)
        pr_err("could not create ssd->procfolder!\n");

    seven_segment_create_proc_files(ssd->procfolder, &pops);

    i2c_set_clientdata(client, ssd);

    return 0;
}

static void seven_segment_remove(struct i2c_client *client){
    size_t idx;
    struct seven_segment_display *ssd;
    ssd = i2c_get_clientdata(client);
    proc_remove(ssd->procfolder);

    idx = seven_segment_get_client_idx(client);
    if (idx >= 0) {
        clients[idx] = NULL;
        --i2c_client_counter;
    }
    kfree(ssd);
}

static struct i2c_driver seven_segment_i2c_driver = {
    .probe = seven_segment_probe,
    .remove = seven_segment_remove,
    .driver = {
        .name = "sev_segment",
        .of_match_table = seven_segment_match,
        .owner = THIS_MODULE
    }
};

static int __init seven_segment_init(void){
    int ret;

    ret = seven_segment_register_top_proc_dir();
    if (ret)
        goto out;

    ret = i2c_register_driver(THIS_MODULE, &seven_segment_i2c_driver);
    if (ret){
        pr_err("Failed to register i2c driver: %d\n", ret);
    }
out:
    return ret;
}

static void __exit seven_segment_exit(void){
    i2c_del_driver(&seven_segment_i2c_driver);
    proc_remove(procparent);
}

MODULE_DEVICE_TABLE(of, seven_segment_match);

module_init(seven_segment_init);
module_exit(seven_segment_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gyorgy Sarvari");
