find_package(GTest REQUIRED)

# CMake doesn't include easy support for locating GMock
# (see https://gitlab.kitware.com/cmake/cmake/issues/17365),
# so we hack it in by replacing "gtest" with "gmock" in the GTest configuration
# it found. This works because GMock is included in recent versions of GTest.

string(REPLACE "gtest" "gmock" GMOCK_LIBRARY ${GTEST_LIBRARY})
string(REPLACE "gtest" "gmock" GMOCK_INCLUDE_DIRS ${GTEST_INCLUDE_DIRS})

add_library(GMock::GMock ${GTEST_LIBRARY_TYPE} IMPORTED)
set_target_properties(GMock::GMock PROPERTIES
    IMPORTED_LOCATION ${GMOCK_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIRS}"
)
