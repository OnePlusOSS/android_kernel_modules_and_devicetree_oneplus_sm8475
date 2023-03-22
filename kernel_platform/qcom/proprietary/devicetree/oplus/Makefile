ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_CAPE) +=  \
	kunpeng-21009-cape-overlay.dtbo \
	jennie-21605-cape-overlay.dtbo \
	jennie-21606-cape-overlay.dtbo \
	ovaltine-21841-cape-overlay.dtbo \
	ovaltine-21845-cape-overlay.dtbo \
	senna-22624-cape-overlay.dtbo \
	senna-22625-cape-overlay.dtbo \
	whiteswan-22001-cape-overlay.dtbo \
	lvzhou-22015-cape-overlay.dtbo \
	udon-22803-cape-overlay.dtbo

kunpeng-21009-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
jennie-21605-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
jennie-21606-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
ovaltine-21841-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
ovaltine-21845-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
senna-22624-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
senna-22625-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
whiteswan-22001-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
lvzhou-22015-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
udon-22803-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
endif

always-y	:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
