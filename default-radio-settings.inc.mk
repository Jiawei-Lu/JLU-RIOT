# Set a custom channel if needed
ifneq (,$(DEFAULT_CHANNEL))
  ifneq (,$(filter cc110x,$(USEMODULE)))        # radio is cc110x sub-GHz
    CFLAGS += -DCC110X_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)
  endif

  ifneq (,$(filter at86rf212b,$(USEMODULE)))    # radio is IEEE 802.15.4 sub-GHz
  
    CFLAGS += -DIEEE802154_DEFAULT_SUBGHZ_CHANNEL=$(DEFAULT_CHANNEL)
      ifeq (868,$(ISM))
        DEFAULT_PAGE ?= 0
        CFLAGS += -DIEEE802154_DEFAULT_SUBGHZ_PAGE=$(DEFAULT_PAGE)
        ifeq (,)
          #DEFAULT_SCLK ?= SPI_CLK_1MHZ
          CFLAGS += -DAT86RF2XX_PARAM_SPI_CLK = $(DEFAULT_SCLK)
        endif
      endif
  else
    CFLAGS += -DIEEE802154_DEFAULT_CHANNEL=$(DEFAULT_CHANNEL)
  endif
endif

# Set a custom PAN ID if needed
ifneq (,$(DEFAULT_PAN_ID))
  CFLAGS += -DIEEE802154_DEFAULT_PANID=$(DEFAULT_PAN_ID)
endif

                                              # radio is IEEE 802.15.4 2.4 GHz
   # ifneq (,&(ISM))
    #  ifeq (868,&(ISM))
     #   DEFAULT_CHANNEL ?= 0
       # CFLAGS += -DIEEE802154_DEFAULT_SUBGHZ_CHANNEL=$(DEFAULT_CHANNEL)
        #DEFAULT_PAGE ?= 0
        #CFLAGS += -DIEEE802154_DEFAULT_SUBGHZ_PAGE=$(DEFAULT_PAGE)
        #DEFAULT_SPI_CLK ?= SPI_CLK_1MHZ
        #CFLAGS += -DAT86RF2XX_PARAM_SPI_CLK = $(DEFAULT_SPI_CLK)
