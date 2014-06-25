# This file is generated by gyp; do not edit.

include $(CLEAR_VARS)

LOCAL_MODULE_CLASS := GYP
LOCAL_MODULE := android_webview_jarjar_content_resources
LOCAL_MODULE_SUFFIX := .stamp
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_TARGET_ARCH := $(TARGET_$(GYP_VAR_PREFIX)ARCH)
gyp_intermediate_dir := $(call local-intermediates-dir,,$(GYP_VAR_PREFIX))
gyp_shared_intermediate_dir := $(call intermediates-dir-for,GYP,shared,,,$(GYP_VAR_PREFIX))

# Make sure our deps are built first.
GYP_TARGET_DEPENDENCIES :=

### Rules for action "jarjar resources":
$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp: gyp_local_path := $(LOCAL_PATH)
$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp: gyp_var_prefix := $(GYP_VAR_PREFIX)
$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp: gyp_intermediate_dir := $(abspath $(gyp_intermediate_dir))
$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp: gyp_shared_intermediate_dir := $(abspath $(gyp_shared_intermediate_dir))
$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp: export PATH := $(subst $(ANDROID_BUILD_PATHS),,$(PATH))
$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp: $(LOCAL_PATH)/build/android/gyp/util/build_utils.py $(LOCAL_PATH)/build/android/gyp/jarjar_resources.py $(LOCAL_PATH)/content/public/android/java/res/drawable-hdpi/bubble.9.png $(LOCAL_PATH)/content/public/android/java/res/drawable-hdpi/bubble_arrow_up.png $(LOCAL_PATH)/content/public/android/java/res/drawable-hdpi/ic_menu_search_holo_light.png $(LOCAL_PATH)/content/public/android/java/res/drawable-hdpi/ic_menu_share_holo_light.png $(LOCAL_PATH)/content/public/android/java/res/drawable-hdpi/ondemand_overlay.9.png $(LOCAL_PATH)/content/public/android/java/res/drawable-hdpi/pageinfo_warning_major.png $(LOCAL_PATH)/content/public/android/java/res/drawable-xhdpi/bubble.9.png $(LOCAL_PATH)/content/public/android/java/res/drawable-xhdpi/bubble_arrow_up.png $(LOCAL_PATH)/content/public/android/java/res/drawable-xhdpi/ic_menu_search_holo_light.png $(LOCAL_PATH)/content/public/android/java/res/drawable-xhdpi/ic_menu_share_holo_light.png $(LOCAL_PATH)/content/public/android/java/res/drawable-xhdpi/ondemand_overlay.9.png $(LOCAL_PATH)/content/public/android/java/res/drawable-xhdpi/pageinfo_warning_major.png $(LOCAL_PATH)/content/public/android/java/res/layout-land/date_time_picker_dialog.xml $(LOCAL_PATH)/content/public/android/java/res/layout/date_time_picker_dialog.xml $(LOCAL_PATH)/content/public/android/java/res/layout/multi_field_time_picker_dialog.xml $(LOCAL_PATH)/content/public/android/java/res/layout/two_field_date_picker.xml $(LOCAL_PATH)/content/public/android/java/res/layout/validation_message_bubble.xml $(LOCAL_PATH)/content/public/android/java/res/menu/select_action_menu.xml $(LOCAL_PATH)/content/public/android/java/res/mipmap-hdpi/app_icon.png $(LOCAL_PATH)/content/public/android/java/res/mipmap-mdpi/app_icon.png $(LOCAL_PATH)/content/public/android/java/res/mipmap-xhdpi/app_icon.png $(LOCAL_PATH)/content/public/android/java/res/mipmap-xxhdpi/app_icon.png $(LOCAL_PATH)/content/public/android/java/res/values-v17/styles.xml $(LOCAL_PATH)/content/public/android/java/res/values/attrs.xml $(LOCAL_PATH)/content/public/android/java/res/values/dimens.xml $(LOCAL_PATH)/content/public/android/java/res/values/strings.xml $(LOCAL_PATH)/content/public/android/java/res/drawable-mdpi/pageinfo_warning_major.png $(LOCAL_PATH)/content/public/android/java/res/drawable-mdpi/ondemand_overlay.9.png $(LOCAL_PATH)/content/public/android/java/res/drawable-mdpi/ic_menu_share_holo_light.png $(LOCAL_PATH)/content/public/android/java/res/drawable-mdpi/ic_menu_search_holo_light.png $(LOCAL_PATH)/content/public/android/java/res/drawable-mdpi/bubble_arrow_up.png $(LOCAL_PATH)/content/public/android/java/res/drawable-mdpi/bubble.9.png $(GYP_TARGET_DEPENDENCIES)
	@echo "Gyp action: Copying and jar-jaring resources for android_webview_jarjar_content_resources ($@)"
	$(hide)cd $(gyp_local_path)/android_webview; mkdir -p $(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources; python ../build/android/gyp/jarjar_resources.py --input-dir ../content/public/android/java/res --output-dir "$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_res" --rules-path ../android_webview/build/jarjar-rules.txt --stamp "$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp"



GYP_GENERATED_OUTPUTS := \
	$(gyp_shared_intermediate_dir)/android_webview_jarjar_content_resources/jarjar_resources.stamp

# Make sure our deps and generated files are built first.
LOCAL_ADDITIONAL_DEPENDENCIES := $(GYP_TARGET_DEPENDENCIES) $(GYP_GENERATED_OUTPUTS)

### Rules for final target.
# Add target alias to "gyp_all_modules" target.
.PHONY: gyp_all_modules
gyp_all_modules: android_webview_jarjar_content_resources

LOCAL_MODULE_PATH := $(PRODUCT_OUT)/gyp_stamp
LOCAL_UNINSTALLABLE_MODULE := true
LOCAL_2ND_ARCH_VAR_PREFIX := $(GYP_VAR_PREFIX)

include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): $(LOCAL_ADDITIONAL_DEPENDENCIES)
	$(hide) echo "Gyp timestamp: $@"
	$(hide) mkdir -p $(dir $@)
	$(hide) touch $@

LOCAL_2ND_ARCH_VAR_PREFIX :=
