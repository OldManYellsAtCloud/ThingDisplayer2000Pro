Linux driver module for a 4 digit 7-segment display from SparkFun, using i2c interface.

In theory it can handle multiple displays concurrently (up to 3, which is just a magic number), but couldn't test that yet.

After loading, it creates a couple of files in procfs. `$i` is a 0-based index of the device, which is meaningful only if there are multiple displays connected.

| Path | Usage |
| ---- | ---- |
| /proc/ssd/$i/brightness | Accepts integers between 0 and 100, both inclusive. Controls the display's brightness. Write-only. |
| /proc/ssd/$i/clear | Accepts any content. Clears the display. Write-only. |
| /proc/ssd/$i/custom_digitX | X is between 1 and 4. It allows setting custom patterns on the display, for each digit separately. Accepts integers between 0 and 127, both inclusive. The value is a bitmap - see display docs for the meaning of bits. Write-only.|
| /proc/ssd/$i/decimals | Allows setting the dots/semicolons on the display. Accepts integers between 0 and 63, both inclusive. The value is a bitmap - see display docs for the meaning of values. Write-only. |
| /proc/ssd/$i/text | Allows setting the actual text to display. Accepts any text up to 4 characters. Write-only. |
| /proc/ssd/$i/name | Read the name of the device, as set in the device tree. Read-only. |

Of course most likely you can find other drivers, or just use a bash scrips and i2cset, but it's not that fun. The driver was mostly written with BeagleBone and RPi in mind - the sample device tree mirrors this.
