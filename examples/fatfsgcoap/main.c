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
#ifdef MODULE_PERIPH_GPIO
#include "board.h"
#include "periph/gpio.h"
#endif
#ifdef MODULE_PM_LAYERED
#ifdef MODULE_PERIPH_RTC
#include "periph/rtc.h"
#endif
#include "pm_layered.h"
#endif

/*Radio netif*/
#include "net/gnrc/netif.h"
#include "net/netif.h"
#include "net/gnrc/netapi.h"

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

/*Radio netif*/
gnrc_netif_t* netif = NULL;

static io1_xplained_t dev;

int wakeup_gap =60;
static unsigned cnt = 0;

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

// static void cb_rtc(void *arg)
// {
//     puts(arg);
// }
// // static void cb_rtc_sleep(void *arg)
// // {
// //     puts(arg);
// // }

void radio_off(gnrc_netif_t *netif){
    netopt_state_t state = NETOPT_STATE_SLEEP;
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }
}
void radio_on(gnrc_netif_t *netif){
    netopt_state_t state = NETOPT_STATE_IDLE;
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }
}



static int check_mode_duration(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <power mode> <duration (s)>\n", argv[0]);
        return -1;
    }

    return 0;
}

static int parse_mode(char *argv)
{
    uint8_t mode = atoi(argv);

    if (mode >= PM_NUM_MODES) {
        printf("Error: power mode not in range 0 - %d.\n", PM_NUM_MODES - 1);
        return -1;
    }

    return mode;
}

static int parse_duration(char *argv)
{
    int duration = atoi(argv);

    if (duration < 0) {
        puts("Error: duration must be a positive number.");
        return -1;
    }

    return duration;
}

static void cb_rtc(void *arg)
{
    int level = (int)arg;

    pm_set(level);
}

static void cb_rtc_puts(void *arg)
{
    puts(arg);
}
// static void cb_rtc_put(void *arg)
// {
//     puts(arg);
// }

static int cmd_unblock_rtc(int argc, char **argv)
{
    if (check_mode_duration(argc, argv) != 0) {
        return 1;
    }

    int mode = parse_mode(argv[1]);
    int duration = parse_duration(argv[2]);

    if (mode < 0 || duration < 0) {
        return 1;
    }

    pm_blocker_t pm_blocker = pm_get_blocker();
    if (pm_blocker.blockers[mode] == 0) {
        printf("Mode %d is already unblocked.\n", mode);
        return 1;
    }

    printf("Unblocking power mode %d for %d seconds.\n", mode, duration);
    fflush(stdout);

    struct tm time;

    rtc_get_time(&time);
    time.tm_sec += duration;
    rtc_set_alarm(&time, cb_rtc, (void *)mode);

    pm_unblock(mode);

    return 0;
}

static int cmd_set_rtc(int argc, char **argv)
{
    if (check_mode_duration(argc, argv) != 0) {
        return 1;
    }

    int mode = parse_mode(argv[1]);
    int duration = parse_duration(argv[2]);

    if (mode < 0 || duration < 0) {
        return 1;
    }

    printf("Setting power mode %d for %d seconds.\n", mode, duration);
    fflush(stdout);

    struct tm time;

    rtc_get_time(&time);
    time.tm_sec += duration;
    rtc_set_alarm(&time, cb_rtc_puts, "The alarm rang");

    pm_set(mode);
    radio_off(netif);

    return 0;
}


// static void _sd_card_cid(void)
// {
//     puts("SD Card CID info:");
//     printf("MID: %d\n", dev.sdcard.cid.MID);
//     printf("OID: %c%c\n", dev.sdcard.cid.OID[0], dev.sdcard.cid.OID[1]);
//     printf("PNM: %c%c%c%c%c\n",
//            dev.sdcard.cid.PNM[0], dev.sdcard.cid.PNM[1], dev.sdcard.cid.PNM[2],
//            dev.sdcard.cid.PNM[3], dev.sdcard.cid.PNM[4]);
//     printf("PRV: %u\n", dev.sdcard.cid.PRV);
//     printf("PSN: %" PRIu32 "\n", dev.sdcard.cid.PSN);
//     printf("MDT: %u\n", dev.sdcard.cid.MDT);
//     printf("CRC: %u\n", dev.sdcard.cid.CID_CRC);
//     puts("+----------------------------------------+\n");
// }

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

static const shell_command_t shell_commands[] = {
    { "cat", "print the content of a file", _cat },
    { "tee", "write a string in a file", _tee },
    { "mount", "mount flash filesystem", _mount },
    { "format", "format flash file system", _format },
    { "umount", "unmount flash filesystem", _umount },
    { "coap", "CoAP example", gcoap_cli_cmd },
    { "set_rtc", "temporary set power mode", cmd_set_rtc },
    { "unblock_rtc", "temporarily unblock power mode", cmd_unblock_rtc },
    { NULL, NULL, NULL }
};

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
        // _sd_card_cid();
        // ztimer_sleep(ZTIMER_MSEC, DELAY_1S);
    }

    // uint16_t light;
    // io1_xplained_read_light_level(&light);
    // printf("Light level: %i\n"
    //         "+-------------------------------------+\n",
    //         light);
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
    
    

    /*RPL config and print*/
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

    // _gnrc_netif_config(0, NULL);

    


    /*FS part*/
    _MTD_define();
    
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

    while (1) {
        ++cnt;
        rtc_get_time(&current_time);
        print_time("currenttime:\n", &current_time);
        int current_timestamp= mktime(&current_time);
        printf("current time stamp: %d\n", current_timestamp);
        int alarm_timestamp = 0;
        if ((int)(current_timestamp % 3600) < (wakeup_gap*1)){
            puts("111");
            pm_set(1);
            radio_on(netif);
            int chance = ( wakeup_gap ) - ( current_timestamp % 3600 );
            alarm_timestamp = (current_timestamp / 3600) *3600+ (wakeup_gap * 1);
            alarm_timestamp = alarm_timestamp- 1577836800;
            rtc_localtime(alarm_timestamp, &alarm_time);
            /*RTC SET ALARM*/
            rtc_set_alarm(&alarm_time, cb_rtc_puts, "Time to sleep");
            print_time("alarm time:\n", &alarm_time);
            printf("---------%ds\n",chance);
            puts("xtimer sleep");
                        
            xtimer_sleep(chance);

            // rtc_set_alarm(&alarm_time, cb_rtc, "111");
            

            /*源代码*/
            // rtc_get_alarm(&time);
            // inc_secs(&time, PERIOD);
            // rtc_set_alarm(&time, cb, &rtc_mtx);
        }
        else{
            //printf("fflush");
            puts("222");
            fflush(stdout);
            
            radio_off(netif);
            alarm_timestamp =  current_timestamp + (3600- (current_timestamp % 3600));
            // alarm_timestamp = (current_timestamp / 360) *360+ (wakeup_gap * 1);
            alarm_timestamp = alarm_timestamp - 1577836800;
            rtc_localtime(alarm_timestamp, &alarm_time);
            print_time("alarm time:\n", &alarm_time);
            
            /*RTC SET ALARM*/
            rtc_set_alarm(&alarm_time, cb_rtc_puts, "TIme to wake up");//(void *)modetest);
            // rtc_set_alarm(&alarm_time, cb_rtc, (void *)modetest);
            pm_set(0);
            // pm_set(0);
        }
    }


    /* start shell */
    puts("All up, running the shell now");

    puts("RIOT network stack example application");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
