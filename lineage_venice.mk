# Boot animation
TARGET_SCREEN_HEIGHT := 2560
TARGET_SCREEN_WIDTH := 1440
TARGET_BOOTANIMATION_HALF_RES := true

# Inherit 64-bit configs
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/product_launched_with_l.mk)

# Inherit some common Lineage stuff.
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

# Inherit device configuration
$(call inherit-product, device/blackberry/venice/device.mk)

## Device identifier. This must come after all inclusions
PRODUCT_BRAND := BlackBerry
PRODUCT_DEVICE := venice
PRODUCT_MODEL := Priv
PRODUCT_MANUFACTURER := BlackBerry
PRODUCT_NAME := lineage_venice

BUILD_FINGERPRINT := blackberry/veniceatt/venice:6.0.1/MMB29M/AAW068:user/release-keys
