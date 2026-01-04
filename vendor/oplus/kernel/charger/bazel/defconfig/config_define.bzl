load(":defconfig/oplus_canoe_perf_config.bzl", "oplus_canoe_perf_config")
load(":defconfig/oplus_canoe_consolidate_config.bzl", "oplus_canoe_consolidate_config")
load(":defconfig/oplus_k6993v1_user_config.bzl", "oplus_k6993v1_user_config")
load(":defconfig/oplus_k6993v1_userdebug_config.bzl", "oplus_k6993v1_userdebug_config")
load(":defconfig/oplus_k6789v1_user_config.bzl", "oplus_k6789v1_user_config")
load(":defconfig/oplus_k6789v1_userdebug_config.bzl", "oplus_k6789v1_userdebug_config")
load(":defconfig/oplus_k6895v1_user_config.bzl", "oplus_k6895v1_user_config")
load(":defconfig/oplus_k6895v1_userdebug_config.bzl", "oplus_k6895v1_userdebug_config")

oplus_config = {
    "qcom": {
        "canoe": {
            "perf": oplus_canoe_perf_config,
            "consolidate": oplus_canoe_perf_config | oplus_canoe_consolidate_config
        }
    },
    "mtk": {
        "k6993v1": {
            "user": oplus_k6993v1_user_config,
            "userdebug": oplus_k6993v1_user_config | oplus_k6993v1_userdebug_config
        },
        "k6789v1": {
            "user": oplus_k6789v1_user_config,
            "userdebug": oplus_k6789v1_user_config | oplus_k6789v1_userdebug_config
        },
        "k6895v1": {
            "user": oplus_k6895v1_user_config,
            "userdebug": oplus_k6895v1_user_config | oplus_k6895v1_userdebug_config
        },
    }
}
