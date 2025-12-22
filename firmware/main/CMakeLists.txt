file(GLOB_RECURSE SRC_FILES "*.c")

# Ensure slider_task.c is included even if CMake's glob cache misses it during incremental builds
list(APPEND SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/slider_task.c")

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
    REQUIRES esp_tinyusb
    PRIV_REQUIRES esp_driver_ledc
    PRIV_REQUIRES esp_timer
    PRIV_REQUIRES esp_driver_i2c
    PRIV_REQUIRES esp_adc
    PRIV_REQUIRES driver
    PRIV_REQUIRES u8g2
)
