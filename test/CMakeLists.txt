find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)
include(GoogleTest)

add_executable(test_oclea_log
    test_logger.cpp
)

target_link_libraries(test_oclea_log PRIVATE
    OcleaLog
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    Threads::Threads
)

gtest_discover_tests(test_oclea_log)
