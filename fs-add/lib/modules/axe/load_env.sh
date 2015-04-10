#!/bin/sh

###############################################################################
##                SETUP STAPI DEVICE NAME ENVIRONMENT VARIABLES              ##
###############################################################################

## Export LD_LIBRARY_PATH for shared libraries
## -------------------------------------------
export LD_LIBRARY_PATH=/usr/local/lib/libstsdk:$LD_LIBRARY_PATH

## STAPI Root Device Name
## ----------------------
ST_DEV_ROOT_NAME=stapi
export ST_DEV_ROOT_NAME

## STAVMEM Device Name
## -------------------
STAVMEM_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stavmem_ioctl
export STAVMEM_IOCTL_DEV_PATH

## STAUDLX Device Name
## -------------------
STAUDLX_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/staudlx_ioctl
export STAUDLX_IOCTL_DEV_PATH

## STBLAST Device Name
## -------------------
STBLAST_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stblast_ioctl
export STBLAST_IOCTL_DEV_PATH

## STBLIT Device Name
## ------------------
STBLIT_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stblit_ioctl
export STBLIT_IOCTL_DEV_PATH

## STBUFFER Device Name
## --------------------
STBUFFER_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stbuffer_ioctl
export STBUFFER_IOCTL_DEV_PATH

## STCC Device Name
## ----------------
STCC_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stcc_ioctl
export STCC_IOCTL_DEV_PATH

## STCLKRV Device Name
## -------------------
STCLKRV_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stclkrv_ioctl
export STCLKRV_IOCTL_DEV_PATH

## STCOMMON Device Name
## --------------------
STCOMMON_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stcommon_ioctl
export STCOMMON_IOCTL_DEV_PATH

## STDENC Device Name
## ------------------
STDENC_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stdenc_ioctl
export STDENC_IOCTL_DEV_PATH

## STDRMCRYPTO Device Name
## -----------------------
STDRMCRYPTO_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stdrmcrypto_ioctl
export STDRMCRYPTO_IOCTL_DEV_PATH

## STEVT Device Name
## -----------------
STEVT_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stevt_ioctl
export STEVT_IOCTL_DEV_PATH

## STFASTFILTER Device Name
## ------------------------
STFASTFILTER_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stfastfilter_ioctl
export STFASTFILTER_IOCTL_DEV_PATH

## STFDMA Device Name
## ------------------
STFDMA_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stfdma_ioctl
export STFDMA_IOCTL_DEV_PATH

## STFRONTEND Device Name
## ----------------------
STFRONTEND_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stfrontend_ioctl
export STFRONTEND_IOCTL_DEV_PATH

## STFSK Device Name
## -----------------
STFSK_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stfsk_ioctl
export STFSK_IOCTL_DEV_PATH

## STGFB Device Name
## -----------------
STGFB_CORE_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stgfb_core
export STGFB_CORE_DEV_PATH
STGFB_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stgfb_ioctl
export STGFB_IOCTL_DEV_PATH

## STGXOBJ Device Name
## -------------------
STGXOBJ_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stgxobj_ioctl
export STGXOBJ_IOCTL_DEV_PATH

## STHDMI Device Name
## ------------------
STHDMI_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/sthdmi_ioctl
export STHDMI_IOCTL_DEV_PATH

## STI2C Device Name
## -----------------
STI2C_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/sti2c_ioctl
export STI2C_IOCTL_DEV_PATH

## STINJECT Device Name
## --------------------
STINJECT_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stinject_ioctl
export STINJECT_IOCTL_DEV_PATH

## STIPRC Device Name
## ------------------
STIPRC_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stiprc_ioctl
export STIPRC_IOCTL_DEV_PATH

## STKEYSCN Device Name
## --------------------
STKEYSCN_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stkeyscn_ioctl
export STKEYSCN_IOCTL_DEV_PATH

## STLAYER Device Name
## -------------------
STLAYER_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stlayer_ioctl
export STLAYER_IOCTL_DEV_PATH

## STMERGE Device Name
## -------------------
STMERGE_IOCTL_DEV_PATH="/dev/${ST_DEV_ROOT_NAME}/stmerge_ioctl"
export STMERGE_IOCTL_DEV_PATH

## MME Device Name
## ---------------
MME_IOCTL_DEV_PATH="/dev/${ST_DEV_ROOT_NAME}/mme"
export MME_IOCTL_DEV_PATH

## MME Device Name
## ---------------
MME_IOCTL_DEV_PATH="/dev/${ST_DEV_ROOT_NAME}/mme"
export MME_IOCTL_DEV_PATH

## STNET Device Name
## -----------------
STNET_IOCTL_DEV_PATH="/dev/${ST_DEV_ROOT_NAME}/stnet_ioctl"
export STNET_IOCTL_DEV_PATH

## STPCPD Device Name
## ------------------
STPCPD_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpcpd_ioctl
export STPCPD_IOCTL_DEV_PATH

## STPCCRD Device Name
## -------------------
STPCCRD_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpccrd_ioctl
export STPCCRD_IOCTL_DEV_PATH

## STPDCRYPTO Device Name
## ----------------------
STPDCRYPTO_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpdcrypto_ioctl
export STPDCRYPTO_DEV_PATH

## STPIO Device Name
## -----------------
STPIO_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpio_ioctl
export STPIO_IOCTL_DEV_PATH

## STPOD Device Name
## -----------------
STPOD_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpod_ioctl
export STPOD_IOCTL_DEV_PATH

## STPOWER Device Name
## -------------------
STPOWER_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpower_ioctl
export STPOWER_IOCTL_DEV_PATH

## STPTI4 Device Name
## ------------------
STPTI4_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpti4_ioctl
export STPTI4_IOCTL_DEV_PATH

## STPTI5 Device Name
## ------------------
STPTI5_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpti5_ioctl
export STPTI5_IOCTL_DEV_PATH

## STPWM Device Name
## -----------------
STPWM_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stpwm_ioctl
export STPWM_IOCTL_DEV_PATH

## STRM Device Name
## ----------------
STRM_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/strm_ioctl
export STRM_IOCTL_DEV_PATH

## STSCART Device Name
## -------------------
STSCART_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stscart_ioctl
export STSCART_IOCTL_DEV_PATH

## STSKB Device Name
## -----------------
STSKB_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stskb_ioctl
export STSKB_IOCTL_DEV_PATH

## STSMART Device Name
## -------------------
STSMART_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stsmart_ioctl
export STSMART_IOCTL_DEV_PATH

## STSPI Device Name
## -----------------
STSPI_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stspi_ioctl
export STSPI_IOCTL_DEV_PATH

## STSUBT Device Name
## ------------------
STSUBT_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stsubt_ioctl
export STSUBT_IOCTL_DEV_PATH

## STSYS Device Name
## -----------------
STSYS_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stsys_ioctl
export STSYS_IOCTL_DEV_PATH

## STTBX Device Name
## -----------------
STTBX_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/sttbx_ioctl
export STTBX_IOCTL_DEV_PATH

## STTKDMA Device Name
## -------------------
STTKDMA_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/sttkdma_ioctl
export STTKDMA_IOCTL_DEV_PATH

## STTTX Device Name
## -----------------
STTTX_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stttx_ioctl
export STTTX_IOCTL_DEV_PATH

## STTUNER Device Name
## -------------------
STTUNER_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/sttuner_ioctl
export STTUNER_DEV_PATH

## STUART Device Name
## ------------------
STUART_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stuart_ioctl
export STUART_IOCTL_DEV_PATH

## STVBI Device Name
## -----------------
STVBI_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stvbi_ioctl
export STVBI_IOCTL_DEV_PATH

## STVID Device Name
## -----------------
STVID_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stvid_ioctl
export STVID_IOCTL_DEV_PATH

## STVIN Device Name
## -----------------
STVIN_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stvin_ioctl
export STVIN_IOCTL_DEV_PATH

## STVMIX Device Name
## ------------------
STVMIX_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stvmix_ioctl
export STVMIX_IOCTL_DEV_PATH

## STVOUT Device Name
## ------------------
STVOUT_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stvout_ioctl
export STVOUT_IOCTL_DEV_PATH

## STVTG Device Name
## -----------------
STVTG_IOCTL_DEV_PATH=/dev/${ST_DEV_ROOT_NAME}/stvtg_ioctl
export STVTG_IOCTL_DEV_PATH
