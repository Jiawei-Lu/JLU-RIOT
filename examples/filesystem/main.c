/*
 * Copyright (C) 2018 OTA keys S.A.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       File system usage example application
 *
 * @author      Vincent Dupont <vincent@otakeys.com>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>


#include "shell.h"

#include "mtd_sdcard.h"
#include "sdcard_spi.h"
#include "sdcard_spi_params.h"
#include "io1_xplained.h"
#include "io1_xplained_params.h"
#include "fmt.h"

#include "periph/gpio.h"

#include "timex.h"
#include "ztimer.h"
#include "at30tse75x.h"


#define DELAY_1S   (1U) /* 1 seconds delay between each test */

#if !defined(MTD_0) && MODULE_MTD_SDCARD
#include "mtd_sdcard.h"
#include "sdcard_spi.h"
#include "sdcard_spi_params.h"
#include "io1_xplained.h"
#include "io1_xplained_params.h"

#include "vfs_default.h"

#define SDCARD_SPI_NUM ARRAY_SIZE(sdcard_spi_params)
static io1_xplained_t dev;
/* SD card devices are provided by drivers/sdcard_spi/sdcard_spi.c */
extern sdcard_spi_t sdcard_spi_devs[SDCARD_SPI_NUM];

/* Configure MTD device for the first SD card */
static mtd_sdcard_t mtd_sdcard_dev = {
    .base = {
        .driver = &mtd_sdcard_driver
    },
    .sd_card = &dev.sdcard,
    .params = &sdcard_spi_params[0],
};
static mtd_dev_t *mtd0 = (mtd_dev_t*)&mtd_sdcard_dev;
#define MTD_0 mtd0
#endif


/* Flash mount point */
#define FLASH_MOUNT_POINT   "/sd0"



static int _cat(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }
    /* With newlib or picolibc, low-level syscalls are plugged to RIOT vfs
     * on native, open/read/write/close/... are plugged to RIOT vfs */
#if defined(MODULE_NEWLIB) || defined(MODULE_PICOLIBC)
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        printf("file %s does not exist\n", argv[1]);
        return 1;
    }
    char c;
    while (fread(&c, 1, 1, f) != 0) {
        putchar(c);
    }
    fclose(f);
#else
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("file %s does not exist\n", argv[1]);
        return 1;
    }
    char c;
    while (read(fd, &c, 1) != 0) {
        putchar(c);
    }
    close(fd);
#endif
    fflush(stdout);
    return 0;
}

static int _tee(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <file> <str>\n", argv[0]);
        return 1;
    }

#if defined(MODULE_NEWLIB) || defined(MODULE_PICOLIBC)
    FILE *f = fopen(argv[1], "w+");
    if (f == NULL) {
        printf("error while trying to create %s\n", argv[1]);
        return 1;
    }
    if (fwrite(argv[2], 1, strlen(argv[2]), f) != strlen(argv[2])) {
        puts("Error while writing");
    }
    fclose(f);
#else
    int fd = open(argv[1], O_RDWR | O_CREAT, 00777);
    if (fd < 0) {
        printf("error while trying to create %s\n", argv[1]);
        return 1;
    }
    if (write(fd, argv[2], strlen(argv[2])) != (ssize_t)strlen(argv[2])) {
        puts("Error while writing");
    }
    close(fd);
#endif
    return 0;
}


#include "fs/littlefs2_fs.h"

/* file system specific descriptor
 * for littlefs2, some fields can be tweaked to define the size
 * of the partition, see header documentation.
 * In this example, default behavior will be used, i.e. the entire
 * memory will be used (parameters come from mtd) */
static littlefs2_desc_t fs_desc = {
    .lock = MUTEX_INIT,
};
/* littlefs file system driver will be used */
#define FS_DRIVER littlefs2_file_system


#if  defined(MTD_0) && defined(MODULE_LITTLEFS2) 
#define FLASH_AND_FILESYSTEM_PRESENT    1
#else
#define FLASH_AND_FILESYSTEM_PRESENT    0
#endif
/* constfs example */
#include "fs/constfs.h"

#define HELLO_WORLD_CONTENT "Hello World!\n"
#define HELLO_RIOT_CONTENT  "Hello RIOT!\n"

/* this defines two const files in the constfs */
static constfs_file_t constfs_files[] = {
    {
        .path = "/hello-world",
        .size = sizeof(HELLO_WORLD_CONTENT),
        .data = (const uint8_t *)HELLO_WORLD_CONTENT,
    },
    {
        .path = "/hello-riot",
        .size = sizeof(HELLO_RIOT_CONTENT),
        .data = (const uint8_t *)HELLO_RIOT_CONTENT,
    }
};

/* this is the constfs specific descriptor */
static constfs_t constfs_desc = {
    .nfiles = ARRAY_SIZE(constfs_files),
    .files = constfs_files,
};

/* constfs mount point, as for previous example, it needs a file system driver,
 * a mount point and private_data as a pointer to the constfs descriptor */
static vfs_mount_t const_mount = {
    .fs = &constfs_file_system,
    .mount_point = "/const",
    .private_data = &constfs_desc,
};

static vfs_mount_t flash_mount = {
    .fs = &FS_DRIVER,
    .mount_point = FLASH_MOUNT_POINT,
    .private_data = &fs_desc,
};
static io1_xplained_t dev;

static int _mount(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if FLASH_AND_FILESYSTEM_PRESENT
    int res = vfs_mount(&flash_mount);
    if (res < 0) {
        printf("Error while mounting %s...try format\n", FLASH_MOUNT_POINT);
        return 1;
    }

    printf("%s successfully mounted\n", FLASH_MOUNT_POINT);
    return 0;
#else
    puts("No external flash file system selected");
    return 1;
#endif
}

static int _format(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if FLASH_AND_FILESYSTEM_PRESENT
    int res = vfs_format(&flash_mount);
    if (res < 0) {
        printf("Error while formatting %s\n", FLASH_MOUNT_POINT);
        return 1;
    }

    printf("%s successfully formatted\n", FLASH_MOUNT_POINT);
    return 0;
#else
    puts("No external flash file system selected");
    return 1;
#endif
}

static void _sd_card_cid(void)
{
    puts("SD Card CID info:");
    printf("MID: %d\n", dev.sdcard.cid.MID);
    printf("OID: %c%c\n", dev.sdcard.cid.OID[0], dev.sdcard.cid.OID[1]);
    printf("PNM: %c%c%c%c%c\n",
           dev.sdcard.cid.PNM[0], dev.sdcard.cid.PNM[1], dev.sdcard.cid.PNM[2],
           dev.sdcard.cid.PNM[3], dev.sdcard.cid.PNM[4]);
    printf("PRV: %u\n", dev.sdcard.cid.PRV);
    printf("PSN: %" PRIu32 "\n", dev.sdcard.cid.PSN);
    printf("MDT: %u\n", dev.sdcard.cid.MDT);
    printf("CRC: %u\n", dev.sdcard.cid.CID_CRC);
    puts("+----------------------------------------+\n");
}

static const shell_command_t shell_commands[] = {
    { "cat", "print the content of a file", _cat },
    { "tee", "write a string in a file", _tee },    
    { "mount", "mount flash filesystem", _mount },
    { "format", "format flash file system", _format },
    { NULL, NULL, NULL }
};


int main(void)
{
    #if defined(MTD_0) && defined(MODULE_LITTLEFS2)
    /* spiffs and littlefs need a mtd pointer
     * by default the whole memory is used */
    fs_desc.dev = MTD_0;
    #elif defined(MTD_0) && defined(MODULE_FATFS_VFS)
    fatfs_mtd_devs[fs_desc.vol_idx] = MTD_0;
    #endif
    int res = vfs_mount(&const_mount);
    if (res < 0) {
        puts("Error while mounting constfs");
    }
    else {
        puts("constfs mounted successfully");
    }
    
    dev.sdcard.init_done = false;

    puts("insert SD-card and use 'init' command to set card to spi mode");
    puts("WARNING: using 'write' or 'copy' commands WILL overwrite data on your sd-card and");
    puts("almost for sure corrupt existing filesystems, partitions and contained data!");

    float temperature;

    puts("IO1 Xplained extension test application\n");
    puts("+-------------Initializing------------+\n");

    if (io1_xplained_init(&dev, &io1_xplained_params[0]) != IO1_XPLAINED_OK) {
        puts("[Error] Cannot initialize the IO1 Xplained extension\n");
        return 1;
    }

    puts("Initialization successful");
    puts("\n+--------Starting tests --------+");
    
        /* Get temperature in degrees celsius */
        at30tse75x_get_temperature(&dev.temp, &temperature);
        printf("Temperature [°C]: %i.%03u\n"
               "+-------------------------------------+\n",
               (int)temperature,
               (unsigned)((temperature - (int)temperature) * 1000));
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* Card detect pin is inverted */
        if (!gpio_read(IO1_SDCARD_SPI_PARAM_DETECT)) {
            _sd_card_cid();
            ztimer_sleep(ZTIMER_MSEC, DELAY_1S);
        }

        uint16_t light;
        io1_xplained_read_light_level(&light);
        printf("Light level: %i\n"
               "+-------------------------------------+\n",
               light);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* set led */
        gpio_set(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* clear led */
        gpio_clear(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* toggle led */
        gpio_toggle(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* toggle led again */
        gpio_toggle(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);
    vfs_format(&flash_mount);

    vfs_DIR mount = {0};

    /* list mounted file systems */
    puts("mount points:");
    while (vfs_iterate_mount_dirs(&mount)) {
        printf("\t%s\n", mount.mp->mount_point);
    }
    printf("\ndata dir: %s\n", VFS_DEFAULT_DATA);

    // /***************_format******************/
    // #if defined(FLASH_AND_FILESYSTEM_PRESENT)
    // int res1 = vfs_format(&flash_mount);
    // if (res1 < 0) {
    //     printf("Error while formatting %s\n", FLASH_MOUNT_POINT);
    //     return 1;
    // }

    // printf("%s successfully formatted\n", FLASH_MOUNT_POINT);
    // return 0;
    // #else
    // puts("No external flash file system selected");
    // return 1;
    // #endif
    /***************_mount*******************/
    // #if defined(FLASH_AND_FILESYSTEM_PRESENT)
    // int res2 = vfs_mount(&flash_mount);
    // if (res2 < 0) {
    //     printf("Error while mounting %s...try format\n", FLASH_MOUNT_POINT);
    //     return 1;
    // }

    // printf("%s successfully mounted\n", FLASH_MOUNT_POINT);
    // return 0;
    // #else
    // puts("No external flash file system selected");
    // return 1;
    // #endif
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
