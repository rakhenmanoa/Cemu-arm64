fastboot $* getvar product 2>&1 | grep "^product: *alioth"
if [ $? -ne 0  ] ; then echo "Missmatching image and device"; exit 1; fi

#check anti_version
if [ -e $(dirname $0)/images/anti_version.txt ]; then
CURRENT_ANTI_VER=`cat $(dirname $0)/images/anti_version.txt`
fi
if [ -z "$CURRENT_ANTI_VER" ]; then CURRENT_ANTI_VER=0; fi
ver=`fastboot $* getvar anti 2>&1 | grep -oP "anti: \K[0-9]+"`
if [ -z "$ver" ]; then ver=0; fi
if [ $ver -gt $CURRENT_ANTI_VER ]; then echo "Current device antirollback version is greater than this pakcage"; exit 1; fi

fastboot $* erase boot_ab
if [ $? -ne 0 ] ; then echo "Erase boot_ab error"; exit 1; fi
fastboot $* flash crclist `dirname $0`/images/crclist.txt
if [ $? -ne 0 ] ; then echo "Flash crclist error"; exit 1; fi
fastboot $* flash sparsecrclist `dirname $0`/images/sparsecrclist.txt
if [ $? -ne 0 ] ; then echo "Flash sparsecrclist error"; exit 1; fi
fastboot $* flash xbl_ab `dirname $0`/images/xbl.elf
if [ $? -ne 0 ] ; then echo "Flash xbl_ab error"; exit 1; fi
fastboot $* flash xbl_config_ab `dirname $0`/images/xbl_config.elf
if [ $? -ne 0 ] ; then echo "Flash xbl_config_ab error"; exit 1; fi
fastboot $* flash abl_ab `dirname $0`/images/abl.elf
if [ $? -ne 0 ] ; then echo "Flash abl_ab error"; exit 1; fi
fastboot $* flash tz_ab `dirname $0`/images/tz.mbn
if [ $? -ne 0 ] ; then echo "Flash tz_ab error"; exit 1; fi
fastboot $* flash hyp_ab `dirname $0`/images/hyp.mbn
if [ $? -ne 0 ] ; then echo "Flash hyp_ab error"; exit 1; fi
fastboot $* flash devcfg_ab `dirname $0`/images/devcfg.mbn
if [ $? -ne 0 ] ; then echo "Flash devcfg_ab error"; exit 1; fi
fastboot $* flash storsec `dirname $0`/images/storsec.mbn
if [ $? -ne 0 ] ; then echo "Flash storsec error"; exit 1; fi
fastboot $* flash bluetooth_ab `dirname $0`/images/BTFM.bin
if [ $? -ne 0 ] ; then echo "Flash bluetooth_ab error"; exit 1; fi
fastboot $* flash cmnlib_ab `dirname $0`/images/cmnlib.mbn
if [ $? -ne 0 ] ; then echo "Flash cmnlib_ab error"; exit 1; fi
fastboot $* flash cmnlib64_ab `dirname $0`/images/cmnlib64.mbn
if [ $? -ne 0 ] ; then echo "Flash cmnlib64_ab error"; exit 1; fi
fastboot $* flash modem_ab `dirname $0`/images/NON-HLOS.bin
if [ $? -ne 0 ] ; then echo "Flash modem_ab error"; exit 1; fi
fastboot $* flash dsp_ab `dirname $0`/images/dspso.bin
if [ $? -ne 0 ] ; then echo "Flash dsp_ab error"; exit 1; fi
fastboot $* flash keymaster_ab `dirname $0`/images/km41.mbn
if [ $? -ne 0 ] ; then echo "Flash keymaster_ab error"; exit 1; fi
fastboot $* flash logo `dirname $0`/images/logo.img
if [ $? -ne 0 ] ; then echo "Flash logo error"; exit 1; fi
fastboot $* flash featenabler_ab `dirname $0`/images/featenabler.mbn
if [ $? -ne 0 ] ; then echo "Flash featenabler_ab error"; exit 1; fi
fastboot $* flash aop_ab `dirname $0`/images/aop.mbn
if [ $? -ne 0 ] ; then echo "Flash aop_ab error"; exit 1; fi
fastboot $* flash qupfw_ab `dirname $0`/images/qupv3fw.elf
if [ $? -ne 0 ] ; then echo "Flash qupfw_ab error"; exit 1; fi
fastboot $* flash uefisecapp_ab `dirname $0`/images/uefi_sec.mbn
if [ $? -ne 0 ] ; then echo "Flash uefisecapp_ab error"; exit 1; fi
fastboot $* flash multiimgoem_ab `dirname $0`/images/multi_image.mbn
if [ $? -ne 0 ] ; then echo "Flash multiimgoem_ab error"; exit 1; fi
fastboot $* flash super `dirname $0`/images/super.img
if [ $? -ne 0 ] ; then echo "Flash super error"; exit 1; fi
fastboot $* flash misc `dirname $0`/images/misc.img
if [ $? -ne 0 ] ; then echo "Flash misc error"; exit 1; fi
fastboot $* flash vbmeta_ab `dirname $0`/images/vbmeta.img
if [ $? -ne 0 ] ; then echo "Flash vbmeta_ab error"; exit 1; fi
fastboot $* flash dtbo_ab `dirname $0`/images/dtbo.img
if [ $? -ne 0 ] ; then echo "Flash dtbo_ab error"; exit 1; fi
fastboot $* flash vbmeta_system_ab `dirname $0`/images/vbmeta_system.img
if [ $? -ne 0 ] ; then echo "Flash vbmeta_system_ab error"; exit 1; fi
fastboot $* flash cust `dirname $0`/images/cust.img
if [ $? -ne 0 ] ; then echo "Flash cust error"; exit 1; fi
fastboot $* erase imagefv_ab
fastboot $* flash imagefv_ab `dirname $0`/images/imagefv.elf
if [ $? -ne 0 ] ; then echo "Flash imagefv_ab error"; exit 1; fi
fastboot $* flash rescue `dirname $0`/images/rescue.img
if [ $? -ne 0 ] ; then echo "Flash rescue error"; exit 1; fi
fastboot $* flash spunvm `dirname $0`/images/spunvm.bin
if [ $? -ne 0 ] ; then echo "Flash spunvm error"; exit 1; fi
fastboot $* flash vendor_boot_ab `dirname $0`/images/vendor_boot.img
if [ $? -ne 0 ] ; then echo "Flash vendor_boot_ab error"; exit 1; fi
fastboot $* flash logfs `dirname $0`/images/logfs_ufs_8mb.bin
if [ $? -ne 0 ] ; then echo "Flash logfs error"; exit 1; fi
fastboot $* flash boot_ab `dirname $0`/images/boot.img
if [ $? -ne 0 ] ; then echo "Flash boot_ab error"; exit 1; fi
fastboot $* set_active a
if [ $? -ne 0 ] ; then echo "Set active error"; exit 1; fi
fastboot $* reboot
if [ $? -ne 0 ] ; then echo "Reboot error"; exit 1; fi

