set(GTEST_SOURCE_ROOT CACHE FILEPATH "Path to Google Test source checkout (if unset, CMake will search for a binary copy)")

if(GTEST_SOURCE_ROOT)
    # Build GTest from the provided source directory
    add_subdirectory(${GTEST_SOURCE_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/googletest)
    set(GTEST_TARGET gtest)
    set(GMOCK_TARGET gmock)
else()
    # Attempt to locate GTest using the CMake config files introduced in 1.8.1
    find_package(GTest CONFIG)
    if (GTest_FOUND)
        set(GTEST_TARGET GTest::gtest)
        set(GMOCK_TARGET GTest::gmock)
    else()
        # Attempt to locate GTest using CMake's FindGTest module
        find_package(GTest MODULE REQUIRED)
        find_package(Threads REQUIRED)

        # CMake's FindGTest module doesn't include easy support for locating GMock
        # (see https://gitlab.kitware.com/cmake/cmake/issues/17365),
        # so we hack it in by replacing "gtest" with "gmock" in the GTest configuration
        # it found. This works because GMock is included in recent versions of GTest.

        string(REPLACE "gtest" "gmock" GMOCK_LIBRARY ${GTEST_LIBRARY})
        string(REPLACE "gtest" "gmock" GMOCK_INCLUDE_DIRS ${GTEST_INCLUDE_DIRS})

        add_library(GMock::GMock UNKNOWN IMPORTED)
        set_target_properties(GMock::GMock PROPERTIES
            IMPORTED_LOCATION ${GMOCK_LIBRARY}
            INTERFACE_INCLUDE_DIRECTORIES "${GMOCK_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES Threads::Threads
        )

        set(GTEST_TARGET GTest::GTest)
        set(GMOCK_TARGET GMock::GMock)
    endif()
endif()
