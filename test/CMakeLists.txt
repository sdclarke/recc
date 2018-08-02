set(CMAKE_CXX_STANDARD 11)

include_directories(../src)

add_executable(subprocess_tests subprocess.cpp main.cpp)
target_link_libraries(subprocess_tests GTest::GTest remoteexecution)
add_test(NAME subprocess_tests COMMAND subprocess_tests)

add_executable(parsedcommand_tests parsedcommand.cpp main.cpp)
target_link_libraries(parsedcommand_tests GTest::GTest remoteexecution)
add_test(NAME parsedcommand_tests COMMAND parsedcommand_tests)

add_executable(deps_tests deps.cpp main.cpp)
target_link_libraries(deps_tests GTest::GTest remoteexecution)
add_test(NAME deps_tests
         COMMAND deps_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/deps)

add_executable(merklize_tests merklize.cpp main.cpp)
target_link_libraries(merklize_tests GTest::GTest remoteexecution)
add_test(NAME merklize_tests
         COMMAND merklize_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/merklize)

add_executable(casclient_tests casclient.cpp main.cpp)
target_link_libraries(casclient_tests GMock::GMock remoteexecution)
add_test(NAME casclient_tests COMMAND casclient_tests)

add_executable(env_tests env.cpp main.cpp)
target_link_libraries(env_tests GTest::GTest remoteexecution)
add_test(NAME env_tests COMMAND env_tests)

add_executable(remoteexecutionclient_tests remoteexecutionclient.cpp main.cpp)
target_link_libraries(remoteexecutionclient_tests GMock::GMock remoteexecution)
add_test(NAME remoteexecutionclient_tests COMMAND remoteexecutionclient_tests)

add_executable(fileutils_tests fileutils.cpp main.cpp)
target_link_libraries(fileutils_tests GTest::GTest remoteexecution)
add_test(NAME fileutils_tests COMMAND fileutils_tests)
