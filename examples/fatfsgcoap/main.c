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
#include <errno.h>

#include "shell.h"
#include "msg.h"

/*GCoAP*/
#include "net/gcoap.h"
#include "shell.h"
#include "gcoap_example.h"

/*Kernel*/
#include "kernel_defines.h"

/*RPL*/
#include "net/gnrc/rpl.h"
#include "net/gnrc/rpl/structs.h"
#include "net/gnrc/rpl/dodag.h"

/*RTC*/
#include "periph_conf.h"
#include "periph/rtc.h"
// #include "periph/rtc_mem.h"

/*PM layer*/
#include "periph/pm.h"
#ifdef MODULE_PM_LAYERED
#include "pm_layered.h"
#endif

/*IO1 Xplianed Extension Board*/
#include "at30tse75x.h"
#include "io1_xplained.h"
#include "io1_xplained_params.h"
#include "fmt.h"
#include "periph/gpio.h"
#include "board.h"

/*Timer*/
#include "timex.h"
#include "ztimer.h"
#include "xtimer.h"

/*FAT Filesystem and VFS tool*/
#include "fs/fatfs.h"
#include "vfs.h"
#include "mtd.h"
// #include "vfs_default.h"

#ifdef MODULE_MTD_SDCARD
#include "mtd_sdcard.h"
#include "sdcard_spi.h"
#include "sdcard_spi_params.h"
#endif

#if FATFS_FFCONF_OPT_FS_NORTC == 0
#include "periph/rtc.h"
#endif

#define FLASH_MOUNT_POINT  "/sd0" /*mount point*/

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#define PERIOD              (2U)
#define REPEAT              (4U)

#define TM_YEAR_OFFSET      (1900)
#define DELAY_1S   (1U) /* 1 seconds delay between each test */

static io1_xplained_t dev;

int wakeup_gap =200;
// static unsigned cnt = 0;
/*RTC struct defination*/
struct tm coaptime;
struct tm alarm_time;
struct tm current_time;

/*RPL*/
gnrc_ipv6_nib_ft_t entry;      
void *state = NULL;
uint8_t dst_address[] = {0};
// uint8_t nexthop_address[] = {0};
// int i = 0;
unsigned iface = 0U;

extern int _gnrc_netif_config(int argc, char **argv);

/*-----------------FAT File System config Start-----------------*/
static fatfs_desc_t fatfs;

static vfs_mount_t flash_mount = {
    .mount_point = FLASH_MOUNT_POINT,
    .fs = &fatfs_file_system,
    .private_data = (void *)&fatfs,
};
#if defined(MODULE_MTD_NATIVE) || defined(MODULE_MTD_MCI)
/* mtd devices are provided in the board's board_init.c*/
extern mtd_dev_t *mtd0;
#endif

#if defined(MODULE_MTD_SDCARD)
#define SDCARD_SPI_NUM ARRAY_SIZE(sdcard_spi_params)



extern sdcard_spi_t sdcard_spi_devs[SDCARD_SPI_NUM];
mtd_sdcard_t mtd_sdcard_devs[SDCARD_SPI_NUM];

/* always default to first sdcard*/
static mtd_dev_t *mtd_sdcard = (mtd_dev_t*)&mtd_sdcard_devs[0];

#define FLASH_AND_FILESYSTEM_PRESENT    1


#endif

/*-----------------FAT File System End-----------------------*/


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

static int _umount(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if FLASH_AND_FILESYSTEM_PRESENT
    int res = vfs_umount(&flash_mount);
    if (res < 0) {
        printf("Error while unmounting %s\n", FLASH_MOUNT_POINT);
        return 1;
    }

    printf("%s successfully unmounted\n", FLASH_MOUNT_POINT);
    return 0;
#else
    puts("No external flash file system selected");
    return 1;
#endif
}



static const shell_command_t shell_commands[] = {
    { "cat", "print the content of a file", _cat },
    { "tee", "write a string in a file", _tee },
    { "mount", "mount flash filesystem", _mount },
    { "format", "format flash file system", _format },
    { "umount", "unmount flash filesystem", _umount },
    { "coap", "CoAP example", gcoap_cli_cmd },
    { NULL, NULL, NULL }
};


void print_time(const char *label, const struct tm *time)
{
    printf("%s  %04d-%02d-%02d %02d:%02d:%02d\n", label,
            time->tm_year + TM_YEAR_OFFSET,
            time->tm_mon + 1,
            time->tm_mday,
            time->tm_hour,
            time->tm_min,
            time->tm_sec);
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

static void _MTD_define(void)
{
    #if MODULE_MTD_SDCARD
    for(unsigned int i = 0; i < SDCARD_SPI_NUM; i++){
        mtd_sdcard_devs[i].base.driver = &mtd_sdcard_driver;
        mtd_sdcard_devs[i].sd_card = &dev.sdcard;
        mtd_sdcard_devs[i].params = &sdcard_spi_params[i];
        mtd_init(&mtd_sdcard_devs[i].base);
    }
    #endif

    #if defined(MODULE_MTD_NATIVE) || defined(MODULE_MTD_MCI)
    fatfs.dev = mtd0;
    #endif

    #if defined(MODULE_MTD_SDCARD)
    fatfs.dev = mtd_sdcard;
    #endif
}

int main(void)
{   
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    server_init();

    puts("gcoap example app");   
    
    puts("Waiting for address autoconfiguration...");
    xtimer_sleep(3);

    /* print network addresses */
    printf("{\"IPv6 addresses\": [\"");
    netifs_print_ipv6("\", \"");
    puts("\"]}");
    _gnrc_netif_config(0, NULL);

    
    puts("gcoap example app");
    puts("insert SD-card and use 'init' command to set card to spi mode");
    puts("WARNING: using 'write' or 'copy' commands WILL overwrite data on your sd-card and");
    puts("almost for sure corrupt existing filesystems, partitions and contained data!");

    float temperature;

    puts("IO1 Xplained extension test application\n");
    puts("+-------------Initializing------------+\n");

    if (io1_xplained_init(&dev, &io1_xplained_params[0]) != IO1_XPLAINED_OK) {
        puts("[Error] Cannot initialize the IO1 Xplained extension\n");
        // return 1;
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

    
    /*------------Typical file create and write test end----------------*/

    /*-------------CoAP CLient with RTC config and init Start------------*/
/* for the thread running the shell */
    struct tm time = {
        .tm_year = 2020 - TM_YEAR_OFFSET,   /* years are counted from 1900 */
        .tm_mon  =  4,                      /* 0 = January, 11 = December */
        .tm_mday = 28,
        .tm_hour = 23,
        .tm_min  = 58,
        .tm_sec  = 57
    };   
    rtc_init();
    puts("1111111111111111111111111111111111");
    print_time("  Setting clock to ", &time);
    rtc_set_time(&time);
    rtc_get_time(&time);	
    
    print_time("Clock value is now ", &time);
    
    


    puts("Configured rpl:");
    gnrc_rpl_init(7);
    puts("printing route:");
    while (gnrc_ipv6_nib_ft_iter(NULL, iface, &state, &entry)) {
        char addr_str[IPV6_ADDR_MAX_STR_LEN];
        if ((entry.dst_len == 0) || ipv6_addr_is_unspecified(&entry.dst)) {
            printf("default%s ", (entry.primary ? "*" : ""));
             puts("printing route:");   

        }
        else {
            printf("%s/%u ", ipv6_addr_to_str(addr_str, &entry.dst, sizeof(addr_str)),entry.dst_len);
            puts("printing route:");
        }
        if (!ipv6_addr_is_unspecified(&entry.next_hop)) {
           printf("via %s ", ipv6_addr_to_str(addr_str, &entry.next_hop, sizeof(addr_str)));
           puts("printing route:");
        }
        // printf("dev #%u\n", fte->iface);
        // char a[i][4] = entry->iface;
       // i++;
    }

    _gnrc_netif_config(0, NULL);

    _MTD_define();
    /*11111111111111111*/
    vfs_format(&flash_mount);

    vfs_DIR mount = {0};
    

    /* list mounted file systems */

    puts("mount points:");
    while (vfs_iterate_mount_dirs(&mount)) {
        printf("\t%s\n", mount.mp->mount_point);
    }
    
    //printf("\ndata dir: %s\n", VFS_DEFAULT_DATA);
    
    /*------------Typical file create and write test start--------------*/

    /*11111111111111111*/
    vfs_mount(&flash_mount);

    char data_file_path[] = "/sd0/DATA.TXT";
    int fo = open(data_file_path, O_RDWR | O_CREAT, 00777);
    if (fo < 0) {
        printf("error while trying to create %s\n", data_file_path);
        return 1;
    }
    else{
        puts("creating file success");
    }
    char test_data[] = "testtesttesttesttest";


    if (write(fo, test_data, strlen(test_data)) != (ssize_t)strlen(test_data)) {
        puts("Error while writing");
    }
    close(fo);
    int fr = open(data_file_path, O_RDONLY | O_CREAT, 00777);  //before open with O_RDWR which 
                                                               //will conflict with open(file)
                                                               //open(file)will equal 0, have to beb a O_RDPNLY for read
    // char data_buf[sizeof(test_data)];
    // printf("data:[],length=");
    // vfs_read(fo,data_buf,sizeof(test_data));    
    // printf("data:[],length=");
    char c;

    while (read(fr, &c, 1) != 0){
    putchar(c);  //printf won't work here
    }
    puts("\n");
       
    close(fo);
    puts("closing file");

    /*11111111111111111*/
    vfs_umount(&flash_mount);
    puts("flash point umount");


    /* start shell */
    puts("All up, running the shell now");

    puts("RIOT network stack example application");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
