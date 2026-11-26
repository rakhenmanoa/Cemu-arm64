fastboot %* getvar product 2>&1 | findstr /r /c:"^product: *alioth" || echo Missmatching image and device
fastboot %* getvar product 2>&1 | findstr /r /c:"^product: *alioth" || exit /B 1

::check anti_version
if exist %~dp0images\anti_version.txt (for /f "delims==" %%a in (%~dp0images\anti_version.txt) do (set CURRENT_ANTI_VER=%%a))
if [%CURRENT_ANTI_VER%] EQU [] set CURRENT_ANTI_VER=0
for /f "tokens=2 delims=: " %%i in ('fastboot %* getvar anti 2^>^&1 ^| findstr /r /c:"anti:"') do (set version=%%i)
if [%version%] EQU [] set version=0
set anticheck="antirollback check pass"
if %version% GTR %CURRENT_ANTI_VER% set anticheck="Current device antirollback version is greater than this pakcage"
echo %anticheck% | findstr /r /c:"pass" || @echo "Antirollback check error" && exit /B 1

fastboot %* erase boot_ab || @echo "Erase boot_ab error" && exit /B 1
fastboot %* flash crclist %~dp0images\crclist.txt || @echo "Flash crclist error" && exit /B 1
fastboot %* flash sparsecrclist %~dp0images\sparsecrclist.txt || @echo "Flash sparsecrclist error" && exit /B 1
fastboot %* flash xbl_ab %~dp0images\xbl.elf || @echo "Flash xbl_ab error" && exit /B 1
fastboot %* flash xbl_config_ab %~dp0images\xbl_config.elf || @echo "Flash xbl_config_ab error" && exit /B 1
fastboot %* flash abl_ab %~dp0images\abl.elf || @echo "Flash abl_ab error" && exit /B 1
fastboot %* flash tz_ab %~dp0images\tz.mbn || @echo "Flash tz_ab error" && exit /B 1
fastboot %* flash hyp_ab %~dp0images\hyp.mbn || @echo "Flash hyp_ab error" && exit /B 1
fastboot %* flash devcfg_ab %~dp0images\devcfg.mbn || @echo "Flash devcfg_ab error" && exit /B 1
fastboot %* flash storsec %~dp0images\storsec.mbn || @echo "Flash storsec error" && exit /B 1
fastboot %* flash bluetooth_ab %~dp0images\BTFM.bin || @echo "Flash bluetooth_ab error" && exit /B 1
fastboot %* flash cmnlib_ab %~dp0images\cmnlib.mbn || @echo "Flash cmnlib_ab error" && exit /B 1
fastboot %* flash cmnlib64_ab %~dp0images\cmnlib64.mbn || @echo "Flash cmnlib64_ab error" && exit /B 1
fastboot %* flash modem_ab %~dp0images\NON-HLOS.bin || @echo "Flash modem_ab error" && exit /B 1
fastboot %* flash dsp_ab %~dp0images\dspso.bin || @echo "Flash dsp_ab error" && exit /B 1
fastboot %* flash keymaster_ab %~dp0images\km41.mbn || @echo "Flash keymaster_ab error" && exit /B 1
fastboot %* flash logo %~dp0images\logo.img || @echo "Flash logo error" && exit /B 1
fastboot %* flash featenabler_ab %~dp0images\featenabler.mbn || @echo "Flash featenabler_ab error" && exit /B 1
fastboot %* flash aop_ab %~dp0images\aop.mbn || @echo "Flash aop_ab error" && exit /B 1
fastboot %* flash qupfw_ab %~dp0images\qupv3fw.elf || @echo "Flash qupfw_ab error" && exit /B 1
fastboot %* flash uefisecapp_ab %~dp0images\uefi_sec.mbn || @echo "Flash uefisecapp_ab error" && exit /B 1
fastboot %* flash multiimgoem_ab %~dp0images\multi_image.mbn || @echo "Flash multiimgoem_ab error" && exit /B 1
fastboot %* flash super %~dp0images\super.img || @echo "Flash super error" && exit /B 1
fastboot %* flash misc %~dp0images\misc.img || @echo "Flash misc error" && exit /B 1
fastboot %* flash vbmeta_ab %~dp0images\vbmeta.img || @echo "Flash vbmeta_ab error" && exit /B 1
fastboot %* flash dtbo_ab %~dp0images\dtbo.img || @echo "Flash dtbo_ab error" && exit /B 1
fastboot %* flash vbmeta_system_ab %~dp0images\vbmeta_system.img || @echo "Flash vbmeta_system_ab error" && exit /B 1
fastboot %* flash cust %~dp0images\cust.img || @echo "Flash cust error" && exit /B 1
fastboot %* erase imagefv_ab || @echo "Erase imagefv_ab error" && exit /B 1
fastboot %* flash imagefv_ab %~dp0images\imagefv.elf || @echo "Flash imagefv_ab error" && exit /B 1
fastboot %* flash rescue %~dp0images\rescue.img || @echo "Flash rescue error" && exit 1
fastboot %* flash spunvm %~dp0images\spunvm.bin || @echo "Flash spunvm error" && exit /B 1
fastboot %* flash vendor_boot_ab %~dp0images\vendor_boot.img || @echo "Flash vendor_boot_ab error" && exit /B 1
fastboot %* flash logfs %~dp0images\logfs_ufs_8mb.bin || @echo "Flash logfs error" && exit /B 1
fastboot %* flash boot_ab %~dp0images\boot.img || @echo "Flash boot_ab error" && exit /B 1
fastboot %* set_active a || @echo "Set active error" && exit /B 1
fastboot %* reboot || @echo "Reboot error" && exit /B 1

