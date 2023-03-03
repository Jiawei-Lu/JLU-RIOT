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

#include "fs/fatfs.h"
#include "vfs.h"
#include "mtd.h"
#include "board.h"
#include "vfs_default.h"



#include "kernel_defines.h"

#ifdef MODULE_MTD_SDCARD
#include "mtd_sdcard.h"
#include "sdcard_spi.h"
#include "sdcard_spi_params.h"
#endif

#if FATFS_FFCONF_OPT_FS_NORTC == 0
#include "periph/rtc.h"
#endif


#include "at30tse75x.h"
#include "io1_xplained.h"
#include "io1_xplained_params.h"
#include "fmt.h"

#include "periph/gpio.h"

#include "timex.h"
#include "ztimer.h"




#define DELAY_1S   (1U) /* 1 seconds delay between each test */


#define FLASH_MOUNT_POINT  "/sd0"
#define FNAME1 "TEST.TXT"
#define FNAME2 "NEWFILE.TXT"
#define FNAME_RNMD  "RENAMED.TXT"
#define FNAME_NXIST "NOFILE.TXT"
#define FULL_FNAME1 (FLASH_MOUNT_POINT "/" FNAME1)
#define FULL_FNAME2 (FLASH_MOUNT_POINT "/" FNAME2)
#define FULL_FNAME_RNMD  (FLASH_MOUNT_POINT "/" FNAME_RNMD)
#define FULL_FNAME_NXIST (FLASH_MOUNT_POINT "/" FNAME_NXIST)
#define DIR_NAME "SOMEDIR"

static const char test_txt[]  = "the test file content 123 abc";
static const char test_txt2[] = "another text";
static const char test_txt3[] = "hello world for vfs";

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

static io1_xplained_t dev;

extern sdcard_spi_t sdcard_spi_devs[SDCARD_SPI_NUM];
mtd_sdcard_t mtd_sdcard_devs[SDCARD_SPI_NUM];

/* always default to first sdcard*/
static mtd_dev_t *mtd_sdcard = (mtd_dev_t*)&mtd_sdcard_devs[0];

#define FLASH_AND_FILESYSTEM_PRESENT    1

#endif

// #if !defined(MTD_0) && MODULE_MTD_SDCARD
// #include "mtd_sdcard.h"
// #include "sdcard_spi.h"
// #include "sdcard_spi_params.h"
// #include "io1_xplained.h"
// #include "io1_xplained_params.h"

// #include "vfs_default.h"

// #define SDCARD_SPI_NUM ARRAY_SIZE(sdcard_spi_params)
// static io1_xplained_t dev;
// /* SD card devices are provided by drivers/sdcard_spi/sdcard_spi.c */
// extern sdcard_spi_t sdcard_spi_devs[SDCARD_SPI_NUM];

// /* Configure MTD device for the first SD card */
// static mtd_sdcard_t mtd_sdcard_dev = {
//     .base = {
//         .driver = &mtd_sdcard_driver
//     },
//     .sd_card = &dev.sdcard,
//     .params = &sdcard_spi_params[0],
// };
// static mtd_dev_t *mtd0 = (mtd_dev_t*)&mtd_sdcard_dev;
// #define MTD_0 mtd0
// #endif
// #define FLASH_MOUNT_POINT   "/sd0"


// // /********[Start]LittlFS2 define*******/
// // #include "fs/littlefs2_fs.h"

// // /* file system specific descriptor
// //  * for littlefs2, some fields can be tweaked to define the size
// //  * of the partition, see header documentation.
// //  * In this example, default behavior will be used, i.e. the entire
// //  * memory will be used (parameters come from mtd) */
// // static littlefs2_desc_t fs_desc = {
// //     .lock = MUTEX_INIT,
// // };
// // /* littlefs file system driver will be used */
// // #define FS_DRIVER littlefs2_file_system

// // /********[End]LittlFS2 define*******/



// /********[Start]FAT FS define*******/
// /* include file system header */
// #ifdef MTD_0
// #if defined(MODULE_FATFS_VFS)
// /* include file system header */
// #include "fs/fatfs.h"

// /* file system specific descriptor
//  * as for littlefs, some fields can be changed if needed,
//  * this example focus on basic usage, i.e. entire memory used */
// static fatfs_desc_t fs_desc;

// /* provide mtd devices for use within diskio layer of fatfs */
// mtd_dev_t *fatfs_mtd_devs[FF_VOLUMES];

// /* fatfs driver will be used */
// #define FS_DRIVER fatfs_file_system
// #endif

// /* this structure defines the vfs mount point:
//  *  - fs field is set to the file system driver
//  *  - mount_point field is the mount point name
//  *  - private_data depends on the underlying file system. For both spiffs and
//  *  littlefs, it needs to be a pointer to the file system descriptor */
// static vfs_mount_t flash_mount = {
//     .fs = &FS_DRIVER,
//     .mount_point = FLASH_MOUNT_POINT,
//     .private_data = &fs_desc,
// };
// #endif /* MTD_0 */
// /********[End]FAT FS define*******/

// //#endif

// //#if  defined(MTD_0) && defined(MODULE_LITTLEFS2)  //for Littlefs2

// #if  defined(MTD_0) && defined(MODULE_FATFS_VFS)  //for FATfs

// #define FLASH_AND_FILESYSTEM_PRESENT    1
// #else
// #define FLASH_AND_FILESYSTEM_PRESENT    0
// #endif



/* Flash mount point */



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


static void print_test_result(const char *test_name, int ok)
{
    printf("%s:[%s]\n", test_name, ok ? "OK" : "FAILED");
}


static void test_format(void)
{
#ifdef MODULE_FATFS_VFS_FORMAT
    print_test_result("test_format__format", vfs_format(&flash_mount) == 0);
#endif
}

static void test_mount(void)
{
    print_test_result("test_mount__mount", vfs_mount(&flash_mount) == 0);
    print_test_result("test_mount__umount", vfs_umount(&flash_mount) == 0);
}

static void test_open(void)
{
    int fd;
    print_test_result("test_open__mount", vfs_mount(&flash_mount) == 0);

    /* try to open file that doesn't exist */
    fd = vfs_open(FULL_FNAME_NXIST, O_RDONLY, 0);
    print_test_result("test_open__open", fd == -ENOENT);

    /* open file with RO, WO and RW access */
    fd = vfs_open(FULL_FNAME1, O_RDONLY, 0);
    print_test_result("test_open__open_ro", fd >= 0);
    print_test_result("test_open__close_ro", vfs_close(fd) == 0);
    fd = vfs_open(FULL_FNAME1, O_WRONLY, 0);
    print_test_result("test_open__open_wo", fd >= 0);
    print_test_result("test_open__close_wo", vfs_close(fd) == 0);
    fd = vfs_open(FULL_FNAME1, O_RDWR, 0);
    print_test_result("test_open__open_rw", fd >= 0);
    print_test_result("test_open__close_rw", vfs_close(fd) == 0);

    print_test_result("test_open__umount", vfs_umount(&flash_mount) == 0);
}

static void test_rw(void)
{
    char buf[sizeof(test_txt) + sizeof(test_txt2)];
    int fd;
    ssize_t nr, nw;
    off_t new_pos;

    print_test_result("test_rw__mount", vfs_mount(&flash_mount) == 0);

    fd = vfs_open(FULL_FNAME1, O_RDONLY, 0);
    print_test_result("test_rw__open_ro", fd >= 0);

    /* compare file content with expected value */
    memset(buf, 0, sizeof(buf));
    nr = vfs_read(fd, buf, sizeof(test_txt));
    print_test_result("test_rw__read_ro", (nr == sizeof(test_txt)) &&
                      (strncmp(buf, test_txt, sizeof(test_txt)) == 0));

    /* try to write to RO file (success if no bytes are actually written) */
    nw = vfs_write(fd, test_txt2, sizeof(test_txt2));
    print_test_result("test_rw__write_ro", nw <= 0);
    print_test_result("test_rw__close_ro", vfs_close(fd) == 0);

    fd = vfs_open("/test/test.txt", O_WRONLY, 0);
    print_test_result("test_rw__open_wo", fd >= 0);

    /* try to read from WO file (success if no bytes are actually read) */
    nr = vfs_read(fd, buf, sizeof(test_txt));
    print_test_result("test_rw__read_wo", nr <= 0);

    print_test_result("test_rw__close_wo", vfs_close(fd) == 0);

    memset(buf, 0, sizeof(buf));
    fd = vfs_open(FULL_FNAME1, O_RDWR, 0);
    print_test_result("test_rw__open_rw", fd >= 0);

    /* read file content and compare it to the expected value */
    nr = vfs_read(fd, buf, sizeof(test_txt));
    print_test_result("test_rw__read_rw", (nr == sizeof(test_txt)) &&
                      (strncmp(buf, test_txt, sizeof(test_txt)) == 0));

    /* write to the file (text should be appended to the end of file) */
    nw = vfs_write(fd, test_txt2, sizeof(test_txt2));
    print_test_result("test_rw__write_rw", nw == sizeof(test_txt2));

    /* seek to start of file */
    new_pos = vfs_lseek(fd, 0, SEEK_SET);
    print_test_result("test_rw__lseek", new_pos == 0);

    /* read file content and compare to expected value */
    memset(buf, 0, sizeof(buf));
    nr = vfs_read(fd, buf, sizeof(buf));
    print_test_result("test_rw__read_rw", (nr == sizeof(buf)) &&
                      (strncmp(buf, test_txt, sizeof(test_txt)) == 0) &&
                      (strncmp(&buf[sizeof(test_txt)],
                               test_txt2,
                               sizeof(test_txt2)) == 0));

    print_test_result("test_rw__close_rw", vfs_close(fd) == 0);

    /* create new file */
    fd = vfs_open(FULL_FNAME2, O_RDWR | O_CREAT, 0);
    print_test_result("test_rw__open_rwc", fd >= 0);

    /* write to the new file, read it's content and compare to expected value */
    nw = vfs_write(fd, test_txt3, sizeof(test_txt3));
    print_test_result("test_rw__write_rwc", nw == sizeof(test_txt3));

    new_pos = vfs_lseek(fd, 0, SEEK_SET);
    print_test_result("test_rw__lseek_rwc", new_pos == 0);

    memset(buf, 0, sizeof(buf));
    nr = vfs_read(fd, buf, sizeof(test_txt3));
    print_test_result("test_rw__read_rwc", (nr == sizeof(test_txt3)) &&
                      (strncmp(buf, test_txt3, sizeof(test_txt3)) == 0));

    print_test_result("test_rw__close_rwc", vfs_close(fd) == 0);
    print_test_result("test_rw__umount", vfs_umount(&flash_mount) == 0);
}

static void test_dir(void)
{
    vfs_DIR dir;
    vfs_dirent_t entry;
    vfs_dirent_t entry2;

    print_test_result("test_dir__mount", vfs_mount(&flash_mount) == 0);
    print_test_result("test_dir__opendir", vfs_opendir(&dir, FLASH_MOUNT_POINT) == 0);
    print_test_result("test_dir__readdir1", vfs_readdir(&dir, &entry) == 1);
    print_test_result("test_dir__readdir2", vfs_readdir(&dir, &entry2) == 1);

    print_test_result("test_dir__readdir_name",
                      ((strncmp(FNAME1, entry.d_name, sizeof(FNAME1)) == 0) &&
                       (strncmp(FNAME2, entry2.d_name, sizeof(FNAME2)) == 0))
                      ||
                      ((strncmp(FNAME1, entry2.d_name, sizeof(FNAME1)) == 0) &&
                       (strncmp(FNAME2, entry.d_name, sizeof(FNAME2)) == 0)));

    print_test_result("test_dir__readdir3", vfs_readdir(&dir, &entry2) == 0);
    print_test_result("test_dir__closedir", vfs_closedir(&dir) == 0);
    print_test_result("test_dir__umount", vfs_umount(&flash_mount) == 0);
}

static void test_rename(void)
{
    vfs_DIR dir;
    vfs_dirent_t entry;
    vfs_dirent_t entry2;

    print_test_result("test_rename__mount", vfs_mount(&flash_mount) == 0);

    print_test_result("test_rename__rename",
                      vfs_rename(FULL_FNAME1, FULL_FNAME_RNMD) == 0);

    print_test_result("test_rename__opendir", vfs_opendir(&dir, FLASH_MOUNT_POINT) == 0);
    print_test_result("test_rename__readdir1", vfs_readdir(&dir, &entry) == 1);
    print_test_result("test_rename__readdir2", vfs_readdir(&dir, &entry2) == 1);

    print_test_result("test_rename__check_name",
              ((strncmp(FNAME_RNMD, entry.d_name, sizeof(FNAME_RNMD)) == 0) &&
               (strncmp(FNAME2, entry2.d_name, sizeof(FNAME2)) == 0))
              ||
              ((strncmp(FNAME_RNMD, entry2.d_name, sizeof(FNAME_RNMD)) == 0) &&
               (strncmp(FNAME2, entry.d_name, sizeof(FNAME2)) == 0)));

    print_test_result("test_rename__readdir3", vfs_readdir(&dir, &entry2) == 0);
    print_test_result("test_rename__closedir", vfs_closedir(&dir) == 0);
    print_test_result("test_rename__umount", vfs_umount(&flash_mount) == 0);
}

static void test_unlink(void)
{
    vfs_DIR dir;
    vfs_dirent_t entry;

    print_test_result("test_unlink__mount", vfs_mount(&flash_mount) == 0);
    print_test_result("test_unlink__unlink1", vfs_unlink(FULL_FNAME2) == 0);
    print_test_result("test_unlink__unlink2", vfs_unlink(FULL_FNAME_RNMD) == 0);
    print_test_result("test_unlink__opendir", vfs_opendir(&dir, FLASH_MOUNT_POINT) == 0);
    print_test_result("test_unlink__readdir", vfs_readdir(&dir, &entry) == 0);
    print_test_result("test_unlink__closedir", vfs_closedir(&dir) == 0);
    print_test_result("test_unlink__umount", vfs_umount(&flash_mount) == 0);
}

static void test_mkrmdir(void)
{
    vfs_DIR dir;

    print_test_result("test_mkrmdir__mount", vfs_mount(&flash_mount) == 0);

    print_test_result("test_mkrmdir__mkdir",
                      vfs_mkdir(FLASH_MOUNT_POINT"/"DIR_NAME, 0) == 0);

    print_test_result("test_mkrmdir__opendir1",
                      vfs_opendir(&dir, FLASH_MOUNT_POINT"/"DIR_NAME) == 0);

    print_test_result("test_mkrmdir__closedir", vfs_closedir(&dir) == 0);

    print_test_result("test_mkrmdir__rmdir",
                      vfs_rmdir(FLASH_MOUNT_POINT"/"DIR_NAME) == 0);

    print_test_result("test_mkrmdir__opendir2",
                      vfs_opendir(&dir, FLASH_MOUNT_POINT"/"DIR_NAME) < 0);

    print_test_result("test_mkrmdir__umount",
                      vfs_umount(&flash_mount) == 0);
}

static void test_create(void)
{
    int fd;
    ssize_t nw;
    print_test_result("test_create__mount", vfs_mount(&flash_mount) == 0);

    fd = vfs_open(FULL_FNAME1, O_WRONLY | O_CREAT, 0);
    print_test_result("test_create__open_wo", fd >= 0);

    nw = vfs_write(fd, test_txt, sizeof(test_txt));
    print_test_result("test_create__write_wo", nw == sizeof(test_txt));
    print_test_result("test_create__close_wo", vfs_close(fd) == 0);

    /* test create if file exists */
    fd = vfs_open(FULL_FNAME1, O_WRONLY | O_CREAT, 0);
    print_test_result("test_create__open_wo2", fd >= 0);

    nw = vfs_write(fd, test_txt, sizeof(test_txt));
    print_test_result("test_create__write_wo2", nw == sizeof(test_txt));
    print_test_result("test_create__close_wo2", vfs_close(fd) == 0);

    print_test_result("test_create__umount", vfs_umount(&flash_mount) == 0);
}

static void test_fstat(void)
{
    int fd;
    struct stat stat_buf;

    print_test_result("test_stat__mount", vfs_mount(&flash_mount) == 0);

    fd = vfs_open(FULL_FNAME1, O_WRONLY | O_TRUNC, 0);
    print_test_result("test_stat__open", fd >= 0);
    print_test_result("test_stat__write",
            vfs_write(fd, test_txt, sizeof(test_txt)) == sizeof(test_txt));
    print_test_result("test_stat__close", vfs_close(fd) == 0);

    print_test_result("test_stat__direct", vfs_stat(FULL_FNAME1, &stat_buf) == 0);

    fd = vfs_open(FULL_FNAME1, O_RDONLY, 0);
    print_test_result("test_stat__open", fd >= 0);
    print_test_result("test_stat__stat", vfs_fstat(fd, &stat_buf) == 0);
    print_test_result("test_stat__close", vfs_close(fd) == 0);
    print_test_result("test_stat__size", stat_buf.st_size == sizeof(test_txt));
    print_test_result("test_stat__umount", vfs_umount(&flash_mount) == 0);
}


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

// static vfs_mount_t flash_mount = {
//     .fs = &FS_DRIVER,
//     .mount_point = FLASH_MOUNT_POINT,
//     .private_data = &fs_desc,
// };
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
    { "umount", "unmount flash filesystem", _umount },
    { NULL, NULL, NULL }
};


int main(void)
{
    // #if defined(MTD_0) && defined(MODULE_LITTLEFS2)
    // /* spiffs and littlefs need a mtd pointer
    //  * by default the whole memory is used */
    // fs_desc.dev = MTD_0;
    // #elif defined(MTD_0) && defined(MODULE_FATFS_VFS)
    // fatfs_mtd_devs[fs_desc.vol_idx] = MTD_0;
    // #endif
    // int res = vfs_mount(&const_mount);
    // if (res < 0) {
    //     puts("Error while mounting constfs");
    // }
    // else {
    //     puts("constfs mounted successfully");
    // }
    
    // dev.sdcard.init_done = false;
   
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

    int res = vfs_mount(&const_mount);
    if (res < 0) {
        puts("Error while mounting constfs");
    }
    else {
        puts("constfs mounted successfully");
    }

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

    test_format();
    test_mount();
    test_open();
    test_rw();
    test_dir();
    test_rename();
    test_unlink();
    test_mkrmdir();
    test_create();
    test_fstat();
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
    vfs_umount(&flash_mount);
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
