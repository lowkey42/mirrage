# additional target to perform clang-format run, requires clang-format

# get all project files
file(GLOB_RECURSE ALL_SOURCE_FILES "${MIRRAGE_ROOT_DIR}/src/*.cpp" "${MIRRAGE_ROOT_DIR}/src/*.h*")

add_custom_target(
        clangformat
        COMMAND /usr/bin/clang-format
        -i
        ${ALL_SOURCE_FILES}
)

