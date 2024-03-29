#ifndef SEVENSEGMENT_H
#define SEVENSEGMENT_H

#include <linux/types.h>
#include <linux/of.h>

#define SEVENSEGMENT_MAX_CLIENTS    3

#define SEVENSEGMENT_CLEAR_SCREEN   0x76
#define SEVENSEGMENT_DECIMAL_CTRL   0x77
#define SEVENSEGMENT_BRIGHTNESS     0x7a
#define SEVENSEGMENT_DIGIT_1        0x7b
#define SEVENSEGMENT_DIGIT_2        0x7c
#define SEVENSEGMENT_DIGIT_3        0x7d
#define SEVENSEGMENT_DIGIT_4        0x7e
#define SEVENSEGMENT_FACTORY_RESET  0x81

enum SevenSegmentProcFile {
    SEVENSEGMENT_BRIGHTNESS_FILE,
    SEVENSEGMENT_CLEAR_FILE,
    SEVENSEGMENT_CUSTOM_DIGIT1_FILE,
    SEVENSEGMENT_CUSTOM_DIGIT2_FILE,
    SEVENSEGMENT_CUSTOM_DIGIT3_FILE,
    SEVENSEGMENT_CUSTOM_DIGIT4_FILE,
    SEVENSEGMENT_DECIMALS_FILE,
    SEVENSEGMENT_NAME_FILE,
    SEVENSEGMENT_TEXT_FILE,
    SEVENSEGMENT_UNKNOWN_FILE
};

enum SevenSegmentDeviceType {
    SEVENSEGMENT_I2C,
    SEVENSEGMENT_SPI
};

typedef union {
    struct i2c_client *i2c;
    struct spi_device *spi;
} client_type;

struct seven_segment_display{
    client_type device;
    enum SevenSegmentDeviceType device_type;
    char text[5];
    uint8_t digit1;
    uint8_t digit2;
    uint8_t digit3;
    uint8_t digit4;
    uint8_t brightness;
    uint8_t decimals;
    struct proc_dir_entry *procfolder;
};

static const struct of_device_id seven_segment_match[] = {
    { .compatible = "sparkfun,7segment" },
    { }
};

#endif // SEVENSEGMENT_H
