file(GLOB_RECURSE SRC_FILES "*.c")

# Ensure slider_task.c is included even if CMake's glob cache misses it during incremental builds
list(APPEND SRC_FILES "${CMAKE_CURRENT_LIST_DIR}/slider_task.c")

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "."
)
