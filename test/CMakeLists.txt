set(CMAKE_CXX_STANDARD 14)

include_directories(../src)
include_directories(fixtures/)

set(FIXT
    fixtures/authsession_fixture.cpp
)

add_library(fixtures ${FIXT})
target_include_directories(fixtures PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
target_link_libraries(fixtures remoteexecution ${GTEST_TARGET})

add_executable(subprocess_tests subprocess.cpp main.cpp)
target_link_libraries(subprocess_tests ${GTEST_TARGET} remoteexecution)
add_test(NAME subprocess_tests COMMAND subprocess_tests)

add_executable(parsedcommand_tests parsedcommand.cpp main.cpp)
target_link_libraries(parsedcommand_tests ${GTEST_TARGET} remoteexecution)
add_test(NAME parsedcommand_tests COMMAND parsedcommand_tests)

add_executable(deps_tests deps.cpp main.cpp)
target_link_libraries(deps_tests ${GTEST_TARGET} remoteexecution)
add_test(NAME deps_tests
         COMMAND deps_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/deps)

add_executable(merklize_tests merklize.cpp main.cpp)
target_link_libraries(merklize_tests ${GTEST_TARGET} remoteexecution)
add_test(NAME merklize_tests
         COMMAND merklize_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/merklize)

add_executable(casclient_tests casclient.cpp main.cpp)
target_link_libraries(casclient_tests ${GMOCK_TARGET} ${GTEST_TARGET} remoteexecution)
add_test(NAME casclient_tests COMMAND casclient_tests)

add_executable(env_set_test env/env_set.cpp main.cpp)
target_link_libraries(env_set_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_set_test COMMAND env_set_test)

add_executable(env_default_cas_test env/env_default_cas.cpp main.cpp)
target_link_libraries(env_default_cas_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_default_cas_test COMMAND env_default_cas_test)

add_executable(env_different_cas_test env/env_different_cas.cpp main.cpp)
target_link_libraries(env_different_cas_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_different_cas_test COMMAND env_different_cas_test)

add_executable(env_from_file_override_test env/env_from_file_override.cpp main.cpp)
target_link_libraries(env_from_file_override_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_from_file_override_test
         COMMAND env_from_file_override_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/)

add_executable(env_from_file_test env/env_from_file.cpp main.cpp)
target_link_libraries(env_from_file_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_from_file_test
         COMMAND env_from_file_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/)

add_executable(env_multiple_configs_test env/env_multiple_configs.cpp main.cpp)
target_link_libraries(env_multiple_configs_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_multiple_configs_test
         COMMAND env_multiple_configs_test
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/)

add_executable(env_file_advertised_order_test env/env_file_advertised_order.cpp main.cpp)
target_link_libraries(env_file_advertised_order_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_file_advertised_order_test COMMAND env_file_advertised_order_test)

add_executable(env_check_order_test env/env_check_order.cpp main.cpp)
target_link_libraries(env_check_order_test ${GTEST_TARGET} remoteexecution)
add_test(NAME env_check_order_test COMMAND env_check_order_test)

add_executable(remoteexecutionclient_tests remoteexecutionclient.cpp main.cpp)
target_link_libraries(remoteexecutionclient_tests ${GMOCK_TARGET} ${GTEST_TARGET} remoteexecution)
add_test(NAME remoteexecutionclient_tests COMMAND remoteexecutionclient_tests)

add_executable(recccounterguard_tests recccounterguard.cpp main.cpp)
target_link_libraries(recccounterguard_tests ${GMOCK_TARGET} ${GTEST_TARGET} remoteexecution)
add_test(NAME recccounterguard_tests COMMAND recccounterguard_tests)

add_executable(fileutils_tests fileutils.cpp main.cpp)
target_link_libraries(fileutils_tests ${GTEST_TARGET} remoteexecution)
add_test(NAME fileutils_tests COMMAND fileutils_tests)

add_executable(actionbuilder_tests actionbuilder.cpp main.cpp)
target_link_libraries(actionbuilder_tests ${GTEST_TARGET} remoteexecution)
add_test(NAME actionbuilder_tests
         COMMAND actionbuilder_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/actionbuilder)

add_executable(grpcretry_tests grpcretry.cpp main.cpp)
target_link_libraries(grpcretry_tests ${GTEST_TARGET} remoteexecution fixtures)
add_test(NAME grpcretry_tests 
	 COMMAND grpcretry_tests
	 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/authsession_fixture)

add_executable(grpccontext_tests grpccontext.cpp main.cpp)
target_link_libraries(grpccontext_tests ${GMOCK_TARGET} ${GTEST_TARGET} remoteexecution fixtures)
add_test(NAME grpccontext_tests 
	 COMMAND grpccontext_tests
	 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/authsession_fixture)

add_executable(authsession_tests authsession.cpp main.cpp)
target_link_libraries(authsession_tests ${GMOCK_TARGET} ${GTEST_TARGET} remoteexecution fixtures)
add_test(NAME authsession_tests
         COMMAND authsession_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/authsession_fixture)

add_executable(jsonfilemanager_tests jsonfilemanager.cpp main.cpp)
target_link_libraries(jsonfilemanager_tests ${GTEST_TARGET} remoteexecution)
add_test(NAME jsonfilemanager_tests
         COMMAND jsonfilemanager_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data/authsession_fixture)

add_executable(commandlineutils_tests commandlineutils.cpp main.cpp)
target_link_libraries(commandlineutils_tests ${GMOCK_TARGET} ${GTEST_TARGET} remoteexecution)
add_test(NAME commandlineutils_tests COMMAND commandlineutils_tests)

add_executable(formpost_test formpost.cpp main.cpp)
target_link_libraries(formpost_test ${GTEST_TARGET} remoteexecution)
add_test(NAME formpost_test COMMAND formpost_test)
