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
	udon-22803-cape-overlay.dtbo \
	senna-22623-ukee-overlay.dtbo \
	monroe-22091-cape-overlay.dtbo \
	monroe-22091-cape-overlay-t0.dtbo \
	daoxiang-23023-cape-overlay.dtbo \
        daoxiang-23023-cape-overlay-V2.dtbo

dtbo-$(CONFIG_ARCH_PARROT) +=  \
	coffee-23611-parrot-overlay.dtbo \
	coffeea-23612-parrot-overlay.dtbo \
	coffeen-23688-parrot-overlay.dtbo

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
senna-22623-ukee-overlay.dtbo-base := ukee.dtb
monroe-22091-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
monroe-22091-cape-overlay-t0.dtbo-base := cape.dtb cape-v2.dtb
daoxiang-23023-cape-overlay.dtbo-base := cape.dtb cape-v2.dtb
daoxiang-23023-cape-overlay-V2.dtbo-base := cape.dtb cape-v2.dtb
coffee-23611-parrot-overlay.dtbo-base := parrot.dtb
coffeea-23612-parrot-overlay.dtbo-base := parrot.dtb
coffeen-23688-parrot-overlay.dtbo-base := parrot.dtb
endif

always-y	:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
