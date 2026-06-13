BSP_ROOT ?= D:/RT-ThreadStudio/workspace/Edgi_Talk_M33_Template
RTT_ROOT ?= D:/RT-ThreadStudio/workspace/Edgi_Talk_M33_Template/rt-thread

CROSS_COMPILE ?=C:\\Users\\XXYYZZ\\arm-none-eabi-

CFLAGS := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -g -Wall -pipe -O3 -gdwarf-2 -g
AFLAGS := -c -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -x assembler-with-cpp -Wa,-mimplicit-it=thumb  -gdwarf-2
LFLAGS := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -Wl,--gc-sections,-Map=rtthread.map,-cref,-u,Reset_Handler -T board/linker_scripts/link.ld
CXXFLAGS := -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=softfp -ffunction-sections -fdata-sections -nostartfiles -g -Wall -pipe -O3 -gdwarf-2 -g

CPPPATHS :=-I$(BSP_ROOT)\packages\aht10-latest\inc \
	-I$(BSP_ROOT)\applications \
	-I$(BSP_ROOT)\libraries\Common\board \
	-I$(BSP_ROOT)\libraries\Common\board\ports\fal \
	-I$(RTT_ROOT)\components\libc\compilers\common\include \
	-I$(RTT_ROOT)\libcpu\arm\common \
	-I$(BSP_ROOT)\libraries\components\serial-memory\include \
	-I$(RTT_ROOT)\components\drivers\include \
	-I$(RTT_ROOT)\components\drivers\wlan \
	-I$(BSP_ROOT)\board \
	-I$(BSP_ROOT)\libraries\HAL_Drivers \
	-I$(BSP_ROOT)\libraries\HAL_Drivers\config \
	-I$(RTT_ROOT)\components\fal\inc \
	-I$(RTT_ROOT)\components\dfs\dfs_v1\include \
	-I$(RTT_ROOT)\components\dfs\dfs_v1\filesystems\devfs \
	-I$(RTT_ROOT)\components\finsh \
	-I$(BSP_ROOT) \
	-I$(RTT_ROOT)\include \
	-I$(RTT_ROOT)\libcpu\arm\cortex-m33 \
	-I$(BSP_ROOT)\libraries\components\Infineon_cmsis-latest\Core\Include \
	-I$(BSP_ROOT)\libraries\components\Infineon_cmsis-latest\Core\Include\m-profile \
	-I$(BSP_ROOT)\libraries\components\Infineon_core-lib-latest\include \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\hal\include \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\pdl\drivers\include \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\pdldrivers\third_party\ethernet\include \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\pdl\drivers\third_party\COMPONENT_GFXSS \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\pdl\drivers\third_party\COMPONENT_GFXSS\vsi\dcnano8000 \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\pdl\drivers\third_party\COMPONENT_GFXSS\vsi\gcnano \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\pdl\devices\include \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\pdl\devices\include\ip \
	-I$(BSP_ROOT)\libraries\components\async-transfer\include \
	-I$(BSP_ROOT)\libraries\components\mtb-device-support-pse8xxgp\device-utils\syspm\include \
	-I$(BSP_ROOT)\libraries\components\Infineon_retarget-io-latest \
	-I$(BSP_ROOT)\libraries\components\ASRC\COMPONENT_CM33\inc \
	-I$(BSP_ROOT)\libraries\components\mtb-srf\include \
	-I$(BSP_ROOT)\libraries\components\mtb-srf\include\COMPONENT_NON_SECURE_DEVICE \
	-I$(BSP_ROOT)\libraries\components\mtb-srf\include\COMPONENT_NON_SECURE_DEVICE\COMPONENT_MW_MTB_IPC \
	-I$(BSP_ROOT)\libraries\components\mtb-ipc\include \
	-I$(BSP_ROOT)\libs\TARGET_APP_KIT_PSE84_EVAL_EPC2 \
	-I$(BSP_ROOT)\libs\TARGET_APP_KIT_PSE84_EVAL_EPC2\config\GeneratedSource \
	-I$(BSP_ROOT)\libs\TARGET_APP_KIT_PSE84_EVAL_EPC2\config \
	-I$(BSP_ROOT)\libs\TARGET_APP_KIT_PSE84_EVAL_EPC2\bluetooth \
	-I$(RTT_ROOT)\components\net\lwip\lwip-2.0.3\src\include \
	-I$(RTT_ROOT)\components\net\lwip\lwip-2.0.3\src\include\ipv4 \
	-I$(RTT_ROOT)\components\net\lwip\lwip-2.0.3\src\include\netif \
	-I$(RTT_ROOT)\components\net\lwip\port \
	-I$(BSP_ROOT)\packages\pahomqtt-latest\MQTTPacket\src \
	-I$(BSP_ROOT)\packages\pahomqtt-latest\MQTTClient-RT \
	-I$(RTT_ROOT)\components\libc\posix\io\epoll \
	-I$(RTT_ROOT)\components\libc\posix\io\eventfd \
	-I$(RTT_ROOT)\components\libc\posix\io\poll \
	-I$(RTT_ROOT)\components\libc\posix\ipc \
	-I$(RTT_ROOT)\components\net\netdev\include \
	-I$(RTT_ROOT)\components\net\sal\include \
	-I$(RTT_ROOT)\components\net\sal\include\socket \
	-I$(RTT_ROOT)\components\net\sal\impl \
	-I$(RTT_ROOT)\components\net\sal\include\dfs_net \
	-I$(RTT_ROOT)\components\net\sal\include\socket\sys_socket \
	-I$(RTT_ROOT)\components\utilities\resource \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\wifi-host-driver\WHD\COMPONENT_WIFI6\src \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\wifi-host-driver\WHD\COMPONENT_WIFI6\inc \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\wifi-host-driver\WHD\COMPONENT_WIFI6\src\include \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\wifi-host-driver\WHD\COMPONENT_WIFI6\src\bus_protocols \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\wifi-host-driver\WHD\COMPONENT_WIFI6\resources\resource_imp \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\porting\inc\bsp \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\porting\inc\hal \
	-I$(BSP_ROOT)\libraries\components\wifi-host-driver\porting\inc\rtos 

DEFINES := -DBLHS_SUPPORT -DCOMPONENT_55500 -DCOMPONENT_55500A1 -DCOMPONENT_APP_KIT_PSE84_EVAL_EPC2 -DCOMPONENT_CM33 -DCOMPONENT_CM33_0 -DCOMPONENT_CYW55513_MOD_PSE84_SOM -DCOMPONENT_Debug -DCOMPONENT_GCC_ARM -DCOMPONENT_GFXSS -DCOMPONENT_HCI_UART -DCOMPONENT_MTB_DEVICE_SUPPORT -DCOMPONENT_MTB_HAL -DCOMPONENT_MW_ASYNC_TRANSFER -DCOMPONENT_MW_BT_FW_IFX_CYW55500A1 -DCOMPONENT_MW_CMSIS -DCOMPONENT_MW_CORE_LIB -DCOMPONENT_MW_CORE_MAKE -DCOMPONENT_MW_MTB_DSL_PSE8XXGP -DCOMPONENT_MW_MTB_IPC -DCOMPONENT_MW_MTB_SRF -DCOMPONENT_MW_SE_RT_SERVICES_UTILS -DCOMPONENT_NON_SECURE_DEVICE -DCOMPONENT_PSE84 -DCOMPONENT_SM -DCOMPONENT_SOFTFP -DCOMPONENT_WIFI_INTERFACE_SDIO -DCOMPONENT_wlbga_iPA_sLNA_ANT0_LHL_XTAL_IN -DCORE_NAME_CM33_0=1 -DCYBSP_MCUBOOT_HEADER_SIZE=0x400 -DCYBSP_WIFI_WL_HOSTWAKE_DRIVE_MODE=MTB_HAL_GPIO_DRIVE_OPENDRAINDRIVESLOW -DCYBSP_WIFI_WL_HOSTWAKE_INIT_STATE=WHD_TRUE -DCY_APPNAME_proj_cm33_ns -DCY_PDL_FLASH_BOOT -DCY_RETARGET_IO_CONVERT_LF_TO_CRLF -DCY_SUPPORTS_DEVICE_VALIDATION -DCY_TARGET_BOARD=APP_KIT_PSE84_EVAL_EPC2 -DCY_WIFI_COUNTRY=WHD_COUNTRY_UNITED_STATES -DDEBUG -DFLASH_BOOT -DPSE846GPS2DBZC4A -DTARGET_APP_KIT_PSE84_EVAL_EPC2 -DTRXV5 -D__RTTHREAD__
