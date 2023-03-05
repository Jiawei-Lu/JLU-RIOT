# name of your application
APPLICATION = filesystem

# If no BOARD is found in the environment, use this default:
BOARD ?= native


# Blacklisting msp430-based boards, as file syscalls are not supported
FEATURES_BLACKLIST += arch_msp430

USEMODULE += mtd_sdcard
USEMODULE += io1_xplained
USEMODULE += ztimer_msec
USEMODULE += sdcard_spi
USEMODULE += fmt
USEMODULE += shell


# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../..

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
# Activate ICMPv6 error messages
USEMODULE += gnrc_icmpv6_error
# Specify the mandatory networking module for a IPv6 routing node
USEMODULE += gnrc_ipv6_router_default
# Add a routing protocol
USEMODULE += gnrc_rpl
USEMODULE += auto_init_gnrc_rpl
# Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo
USEMODULE += shell_cmd_gnrc_udp
# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps
USEMODULE += netstats_l2
USEMODULE += netstats_ipv6
USEMODULE += netstats_rpl

USEMODULE += io1_xplained
USEMODULE += ztimer

# Modules to include:
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps

# Use the default file system
USEMODULE += vfs_default
USEMODULE += vfs_auto_format
USEMODULE += vfs
USEMODULE += constfs
USEMODULE += fatfs_vfs

FEATURES_OPTIONAL += periph_rtc
FEATURES_REQUIRED += periph_spi

USEMODULE += fatfs_vfs_format
CFLAGS += -DCONFIG_FATFS_FORMAT_ALLOC_STATIC=1

FATFS_IMAGE_FILE_SIZE_MIB ?= 128
# MODULE := fatfs_vfs
# USEMODULE += devfs

# USEMODULE += littlefs2
# Enable to automatically format if mount fails
#USEMODULE += vfs_auto_format

USE_ZEP ?= 0

# set the ZEP port for native
ZEP_PORT_BASE ?= 17754
ifeq (1,$(USE_ZEP))
  TERMFLAGS += -z [::1]:$(ZEP_PORT_BASE)
  USEMODULE += socket_zep

  ifneq (,$(ZEP_MAC))
    TERMFLAGS += --eui64=$(ZEP_MAC)
  endif
endif


include $(RIOTBASE)/Makefile.include

# Set a custom channel if needed
include $(RIOTMAKE)/default-radio-settings.inc.mk
