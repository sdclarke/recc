set(GTEST_SOURCE_ROOT CACHE FILEPATH "Path to Google Test source checkout (if unset, CMake will search for a binary copy)")

if(GTEST_SOURCE_ROOT)
    add_subdirectory(${GTEST_SOURCE_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/googletest)
    add_library(GTest::GTest ALIAS gtest)
    add_library(GMock::GMock ALIAS gmock)
else()
    find_package(GTest REQUIRED)

    # CMake doesn't include easy support for locating GMock
    # (see https://gitlab.kitware.com/cmake/cmake/issues/17365),
    # so we hack it in by replacing "gtest" with "gmock" in the GTest configuration
    # it found. This works because GMock is included in recent versions of GTest.

    string(REPLACE "gtest" "gmock" GMOCK_LIBRARY ${GTEST_LIBRARY})
    string(REPLACE "gtest" "gmock" GMOCK_INCLUDE_DIRS ${GTEST_INCLUDE_DIRS})

    add_library(GMock::GMock UNKNOWN IMPORTED)
    set_target_properties(GMock::GMock PROPERTIES
       IMPORTED_LOCATION ${GMOCK_LIBRARY}
       INTERFACE_INCLUDE_DIRECTORIES "${GTEST_INCLUDE_DIRS}"
    )
endif()
