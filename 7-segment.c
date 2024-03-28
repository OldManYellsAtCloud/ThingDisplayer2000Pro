#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/proc_fs.h>
#include <linux/sysfs.h>

#include <linux/device/driver.h>
#include <linux/i2c.h>
#include <linux/of.h>

#include "7-segment.h"

static struct proc_dir_entry *procparent;

struct seven_segment_display{
    struct i2c_client *client;
    char text[5];
    uint8_t digit1;
    uint8_t digit2;
    uint8_t digit3;
    uint8_t digit4;
    uint8_t brightness;
    uint8_t decimals;
    struct proc_dir_entry *procfolder;
};

static struct i2c_client* clients[SEVENSEGMENT_MAX_CLIENTS];
static size_t client_counter = 0;

static int seven_segment_send_cmd(struct i2c_client *client, char* cmd, size_t len){
    int ret;
    ret = i2c_master_send(client, cmd, len);
    if (ret < 0)
        pr_err("Could not send i2c cmd \'%s\'. Error: %d\n", cmd, ret);
    return ret;
}

static void seven_segment_reset_screen(struct i2c_client* client){
    char cmd[] = {SEVENSEGMENT_CLEAR_SCREEN};
    seven_segment_send_cmd(client, cmd, 1);
}


static void seven_segment_set_brightness(struct i2c_client *client, uint8_t brightness){
    char cmd[] = {SEVENSEGMENT_BRIGHTNESS, brightness};
    seven_segment_send_cmd(client, cmd, 2);
}

static void seven_segment_factory_reset(struct i2c_client *client){
    char cmd[] = {SEVENSEGMENT_FACTORY_RESET};
    seven_segment_send_cmd(client, cmd, 1);
}

static int seven_segment_parse_and_send_text(struct i2c_client* client, char* c){
    int ret, i;
    ret = strlen(c);
    if (ret > 5) {
        pr_err("Max 4 characters can be displayed, not %d.\n", ret);
        return -EINVAL;
    }

    for (i = 0; i < ret - 1 ; ++i)
        seven_segment_send_cmd(client, &c[i], 1);
    return ret;
}

static int seven_segment_parse_and_set_custom_digit(struct i2c_client* client, char* c, int digit){
    int ret, i;
    char cmd[2];
    if (digit < 1 || digit > 4){
        pr_err("Invalid digit: %d. Must be between 1 and 4.\n", digit);
        return -EINVAL;
    }

    ret = kstrtoint(c, 10, &i);
    if (ret < 0){
        pr_err("Could not parse number: %s\n", c);
        return -EINVAL;
    }
    if (i < 0 || i > 127){
        pr_err("Invalid value. Must be between 0 and 127");
        return -EINVAL;
    }
    cmd[0] = (SEVENSEGMENT_DIGIT_1 + digit - 1);
    cmd[1] = i;
    seven_segment_send_cmd(client, cmd, 2);
    return strlen(c);
}

static int seven_segment_parse_and_set_decimals(struct i2c_client *client, char* c){
    int ret, i;
    char cmd[2];

    ret = kstrtoint(c, 10, &i);
    if (ret < 0){
        pr_err("Could not parse number: %s\n", c);
        return -EINVAL;
    }
    if (i < 0 || i > 63){
        pr_err("Invalid value. Must be between 0 and 63");
        return -EINVAL;
    }
    cmd[0] = SEVENSEGMENT_DECIMAL_CTRL;
    cmd[1] = i;
    seven_segment_send_cmd(client, cmd, 2);
    return strlen(c);
}

static int seven_segment_parse_and_set_brightness(struct i2c_client *client, char* c){
    int i, ret;
    ret = kstrtoint(c, 10, &i);
    if (ret < 0 ){
        pr_err("Invalid brightness: %s\n", c);
        return -EINVAL;
    } else if (i < 0 || i > 100) {
        pr_err("Out of range brightness, should be between 0 and 100!\n");
        return -EINVAL;
    }
    seven_segment_set_brightness(client, i);
    return strlen(c);
}

static size_t seven_segment_get_free_idx(void){
    if (!client_counter || !clients[0]){
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


static enum SevenSegmentProcFile seven_segment_get_proc_enum(char* file_name){
    if (!strncmp("brightness", file_name, strlen("brightness")))
        return SEVENSEGMENT_BRIGHTNESS_FILE;
    if (!strncmp("clear", file_name, strlen("clear")))
        return SEVENSEGMENT_CLEAR_FILE;
    if (!strncmp("custom_digit1", file_name, strlen("custom_digit1")))
        return SEVENSEGMENT_CUSTOM_DIGIT1_FILE;
    if (!strncmp("custom_digit2", file_name, strlen("custom_digit2")))
        return SEVENSEGMENT_CUSTOM_DIGIT2_FILE;
    if (!strncmp("custom_digit3", file_name, strlen("custom_digit3")))
        return SEVENSEGMENT_CUSTOM_DIGIT3_FILE;
    if (!strncmp("custom_digit4", file_name, strlen("custom_digit4")))
        return SEVENSEGMENT_CUSTOM_DIGIT4_FILE;
    if (!strncmp("decimals", file_name, strlen("decimals")))
        return SEVENSEGMENT_DECIMALS_FILE;
    if (!strncmp("name", file_name, strlen("name")))
        return SEVENSEGMENT_NAME_FILE;
    if (!strncmp("text", file_name, strlen("text")))
        return SEVENSEGMENT_TEXT_FILE;

    return SEVENSEGMENT_UNKNOWN_FILE;
}

static ssize_t seven_segment_proc_write(struct file* f, const char __user* buf, size_t sz, loff_t* off){
    enum SevenSegmentProcFile sspf;
    int ret;
    size_t idx;
    char* text;
    struct i2c_client* client;

    sspf = seven_segment_get_proc_enum((char *)f->f_path.dentry->d_iname);
    idx = f->f_path.dentry->d_parent->d_iname[0] - 48;
    client = clients[idx];
    text = kmalloc(sz + 1, GFP_KERNEL);

    if (!text){
        pr_err("Could not allocate memory for text\n");
        return -ENOMEM;
    }

    ret = copy_from_user(text, buf, sz);
    if (ret < 0){
        pr_err("Could not copy text from user\n");
        return -ENOMEM;
    }
    text[sz] = 0;

    switch(sspf){
    case SEVENSEGMENT_BRIGHTNESS_FILE:
        sz = seven_segment_parse_and_set_brightness(client, text);
        break;
    case SEVENSEGMENT_CLEAR_FILE:
        seven_segment_reset_screen(client);
        break;
    case SEVENSEGMENT_TEXT_FILE:
        sz = seven_segment_parse_and_send_text(client, text);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT1_FILE:
        sz = seven_segment_parse_and_set_custom_digit(client, text, 1);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT2_FILE:
        sz = seven_segment_parse_and_set_custom_digit(client, text, 2);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT3_FILE:
        sz = seven_segment_parse_and_set_custom_digit(client, text, 3);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT4_FILE:
        sz = seven_segment_parse_and_set_custom_digit(client, text, 4);
        break;
    case SEVENSEGMENT_DECIMALS_FILE:
        sz = seven_segment_parse_and_set_decimals(client, text);
        break;
    case SEVENSEGMENT_UNKNOWN_FILE:
    default:
        pr_err("Unknown file: %s\n", f->f_path.dentry->d_iname);
        sz = -ENOENT;
    }

    return sz;
}

ssize_t	seven_segment_proc_read(struct file *f, char __user *buf, size_t sz, loff_t *off){
    ssize_t ret;
    size_t idx, len;
    struct i2c_client *client;
    idx = f->f_path.dentry->d_parent->d_iname[0] - 48;
    client = clients[idx];

    len = strlen(client->name);
    ret = len;
    if (*off >= len || copy_to_user(buf, client->name, strlen(client->name))){
        ret = 0;
    } else {
        *off += len;
    }

    return ret;
}

static struct proc_ops pops = {
    .proc_write = seven_segment_proc_write,
    .proc_read = seven_segment_proc_read
};

static int seven_segment_probe(struct i2c_client *client){
    int client_idx;
    char procfsname[2];

    if (client_counter >= SEVENSEGMENT_MAX_CLIENTS - 1){
        pr_err("Too many displays present! Max 3 are allowed.\n");
        return -ENFILE;
    }

    if (i2c_verify_client(&client->dev)){
        client_idx = seven_segment_get_free_idx();
        clients[client_idx] = client;
        procfsname[0] = client_idx + 48;
        procfsname[1] = 0;

        if (!procparent){
            pr_err("procparent doesn't exist!\n");
            return -ENOMEM;
        }

        struct seven_segment_display *ssd = kmalloc(sizeof(struct seven_segment_display), GFP_KERNEL);
        ssd->procfolder = proc_mkdir(procfsname, procparent);

        if (!ssd->procfolder)
            pr_err("could not create ssd->procfolder!\n");

        proc_create("text", 0220, ssd->procfolder, &pops);
        proc_create("clear", 0220, ssd->procfolder, &pops);
        proc_create("decimals", 0220, ssd->procfolder, &pops);
        proc_create("custom_digit1", 0220, ssd->procfolder, &pops);
        proc_create("custom_digit2", 0220, ssd->procfolder, &pops);
        proc_create("custom_digit3", 0220, ssd->procfolder, &pops);
        proc_create("custom_digit4", 0220, ssd->procfolder, &pops);
        proc_create("brightness", 0220, ssd->procfolder, &pops);
        proc_create("name", 0444, ssd->procfolder, &pops);

        i2c_set_clientdata(client, ssd);
    } else {
        return -ENODEV;
    }

    return 0;
}

static void seven_segment_remove(struct i2c_client *client){
    size_t idx;
    struct seven_segment_display *ssd;
    ssd = i2c_get_clientdata(client);
    proc_remove(ssd->procfolder);

    idx = seven_segment_get_client_idx(client);
    clients[idx] = NULL;

    kfree(i2c_get_clientdata(client));
}

static const struct of_device_id seven_segment_match[] = {
    { .compatible = "sparkfun,7segment" },
    { }
};

MODULE_DEVICE_TABLE(of, seven_segment_match);

static struct i2c_driver seven_segment_driver = {
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

    procparent = proc_mkdir("ssd", NULL);
    if (!procparent){
        pr_err("/proc/ssd creation failed!\n");
        return -ENOMEM;
    }

    ret = i2c_register_driver(THIS_MODULE, &seven_segment_driver);
    if (ret){
        pr_err("Failed to register driver: %d\n", ret);
    }

    return ret;
}

static void __exit seven_segment_exit(void){
    i2c_del_driver(&seven_segment_driver);
    proc_remove(procparent);
}

module_init(seven_segment_init);
module_exit(seven_segment_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Gyorgy Sarvari");
