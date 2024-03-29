#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include "7-segment.h"

struct proc_dir_entry *procparent;

static int seven_segment_send_cmd(struct seven_segment_display *ssd, char* cmd, size_t len) {
    int ret;
    switch (ssd->device_type){
    case SEVENSEGMENT_I2C:
        ret = i2c_master_send(ssd->device.i2c, cmd, len);
        break;
    case SEVENSEGMENT_SPI:
    default:
        ret = spi_write(ssd->device.spi, cmd, len);
        break;
    }

    if (ret < 0)
        pr_err("Could not send cmd \'%s\'. Error: %d\n", cmd, ret);
    return ret;
}

static void seven_segment_reset_screen(struct seven_segment_display* client){
    char cmd[] = {SEVENSEGMENT_CLEAR_SCREEN};
    seven_segment_send_cmd(client, cmd, 1);
}


static void seven_segment_set_brightness(struct seven_segment_display *client, uint8_t brightness){
    char cmd[] = {SEVENSEGMENT_BRIGHTNESS, brightness};
    seven_segment_send_cmd(client, cmd, 2);
}

static void seven_segment_factory_reset(struct seven_segment_display *client){
    char cmd[] = {SEVENSEGMENT_FACTORY_RESET};
    seven_segment_send_cmd(client, cmd, 1);
}

static int seven_segment_parse_and_send_text(struct seven_segment_display* client, char* c){
    int ret, i;
    ret = strlen(c);
    if (ret > 5) {
        pr_err("Max 4 characters can be displayed, not %d.\n", ret);
        return -EINVAL;
    }

    for (i = 0; i < ret - 1 ; ++i)
        seven_segment_send_cmd(client, &c[i], 1);

    strncpy(client->text, c, 4);
    client->text[4] = 0; // just to make sure that it's null-terminated.
    return ret;
}

static int seven_segment_parse_and_set_custom_digit(struct seven_segment_display* client, char* c, int digit){
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

    switch(digit){
    case 1:
        client->digit1 = i;
        break;
    case 2:
        client->digit2 = i;
        break;
    case 3:
        client->digit3 = i;
        break;
    case 4:
        client->digit4 = i;
        break;
    }

    return strlen(c);
}

static int seven_segment_parse_and_set_decimals(struct seven_segment_display *client, char* c){
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
    client->decimals = i;
    return strlen(c);
}

static int seven_segment_parse_and_set_brightness(struct seven_segment_display *client, char* c){
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
    client->brightness = i;
    return strlen(c);
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

void seven_segment_create_proc_files(struct proc_dir_entry* parent, struct proc_ops* pops){
    if (!proc_create("text", 0664, parent, pops))
        pr_err("Could not create text file in procfs!\n");
    if (!proc_create("clear", 0220, parent, pops))
        pr_err("Could not create clear file in procfs!\n");
    if (!proc_create("decimals", 0664, parent, pops))
        pr_err("Could not create decimals file in procfs!\n");
    if (!proc_create("custom_digit1", 0664, parent, pops))
        pr_err("Could not create custom_digit1 file in procfs!\n");
    if (!proc_create("custom_digit2", 0664, parent, pops))
        pr_err("Could not create custom_digit2 file in procfs!\n");
    if (!proc_create("custom_digit3", 0664, parent, pops))
        pr_err("Could not create custom_digit3 file in procfs!\n");
    if (!proc_create("custom_digit4", 0664, parent, pops))
        pr_err("Could not create custom_digit4 file in procfs!\n");
    if (!proc_create("brightness", 0664, parent, pops))
        pr_err("Could not create brightness file in procfs!\n");
    if (!proc_create("name", 0444, parent, pops))
        pr_err("Could not create name file in procfs!\n");
}

EXPORT_SYMBOL(seven_segment_create_proc_files);


static size_t seven_segment_int_number_of_digits(int i){
    size_t digitNum = 1;

    if (i < 0){
        ++digitNum; // minus sign
        i *= -1;
    }

    for (; i > 9; ++digitNum, i/=10);
    return digitNum;
}

static ssize_t seven_segment_send_int_to_user(char __user** buf, loff_t** off, int i){
    int ret;
    char* tmp;
    size_t digitNum = seven_segment_int_number_of_digits(i);

    if (**off >= digitNum)
        return 0;

    tmp = kzalloc(digitNum, GFP_KERNEL);
    sprintf(tmp, "%d", i);

    ret = copy_to_user(*buf, tmp, digitNum);
    if (ret < 0){
        pr_err("Could not copy to user: %d.\n", ret);
        ret = 0;
    } else {
        ret = digitNum;
        **off += digitNum;
    }
    kfree(tmp);
    return ret;
}

static ssize_t seven_segment_send_str_to_user(char __user** buf, loff_t** off, char* str){
    int ret;
    size_t len;

    if (!str)
        return 0;

    len = strnlen(str, 64); // 64 is arbitrary limit, no special meaning
    if (**off >= len)
        return 0;

    ret = copy_to_user(*buf, str, len);
    if (ret < 0){
        pr_err("Could not copy to user: %d\n", ret);
        ret = 0;
    } else {
        ret = len;
        **off += len;
    }
    return ret;
}

ssize_t seven_segment_read_proc_file(struct seven_segment_display *ssd, struct file* f, char __user** buf, loff_t **off){
    ssize_t ret;
    enum SevenSegmentProcFile sspf;
    sspf = seven_segment_get_proc_enum((char *)f->f_path.dentry->d_iname);

    switch(sspf){
    case SEVENSEGMENT_BRIGHTNESS_FILE:
        ret = seven_segment_send_int_to_user(buf, off, ssd->brightness);
        break;
    case SEVENSEGMENT_TEXT_FILE:
        ret = seven_segment_send_str_to_user(buf, off, ssd->text);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT1_FILE:
        ret = seven_segment_send_int_to_user(buf, off, ssd->digit1);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT2_FILE:
        ret = seven_segment_send_int_to_user(buf, off, ssd->digit2);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT3_FILE:
        ret = seven_segment_send_int_to_user(buf, off, ssd->digit3);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT4_FILE:
        ret = seven_segment_send_int_to_user(buf, off, ssd->digit4);
        break;
    case SEVENSEGMENT_DECIMALS_FILE:
        ret = seven_segment_send_int_to_user(buf, off, ssd->decimals);
        break;
    case SEVENSEGMENT_NAME_FILE:
        if (ssd->device_type == SEVENSEGMENT_I2C){
            ret = seven_segment_send_str_to_user(buf, off, ssd->device.i2c->name);
        } else {
            ret = seven_segment_send_str_to_user(buf, off, (char *)ssd->device.spi->dev.init_name);
        }
        break;
    case SEVENSEGMENT_UNKNOWN_FILE:
    case SEVENSEGMENT_CLEAR_FILE:
    default:
        pr_err("Unknown file: %s\n", f->f_path.dentry->d_iname);
        ret = -ENOENT;
    }

    return ret;
}

EXPORT_SYMBOL(seven_segment_read_proc_file);

ssize_t seven_segment_write_proc_file(struct seven_segment_display *ssd, struct file* f, const char __user* buf, size_t sz){
    int ret;
    enum SevenSegmentProcFile sspf;
    char* text;

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

    sspf = seven_segment_get_proc_enum((char *)f->f_path.dentry->d_iname);

    switch(sspf){
    case SEVENSEGMENT_BRIGHTNESS_FILE:
        sz = seven_segment_parse_and_set_brightness(ssd, text);
        break;
    case SEVENSEGMENT_CLEAR_FILE:
        seven_segment_reset_screen(ssd);
        break;
    case SEVENSEGMENT_TEXT_FILE:
        sz = seven_segment_parse_and_send_text(ssd, text);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT1_FILE:
        sz = seven_segment_parse_and_set_custom_digit(ssd, text, 1);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT2_FILE:
        sz = seven_segment_parse_and_set_custom_digit(ssd, text, 2);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT3_FILE:
        sz = seven_segment_parse_and_set_custom_digit(ssd, text, 3);
        break;
    case SEVENSEGMENT_CUSTOM_DIGIT4_FILE:
        sz = seven_segment_parse_and_set_custom_digit(ssd, text, 4);
        break;
    case SEVENSEGMENT_DECIMALS_FILE:
        sz = seven_segment_parse_and_set_decimals(ssd, text);
        break;
    case SEVENSEGMENT_UNKNOWN_FILE:
    default:
        pr_err("Unknown file: %s\n", f->f_path.dentry->d_iname);
        sz = -ENOENT;
    }
    kfree(text);
    return sz;
}

EXPORT_SYMBOL(seven_segment_write_proc_file);

int seven_segment_register_top_proc_dir(void) {
    if (!procparent){
        procparent = proc_mkdir("ssd", NULL);
    }

    if (!procparent){
        pr_err("/proc/ssd creation failed!\n");
        return -ENOMEM;
    }
    return 0;
}

EXPORT_SYMBOL(seven_segment_register_top_proc_dir);

MODULE_LICENSE("GPL");
