find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

add_executable(test_oclea_log
    test_logger.cpp
)

target_link_libraries(test_oclea_log PRIVATE
    OcleaLog
    GTest::GTest
    GTest::Main
    Threads::Threads
)

gtest_discover_tests(test_oclea_log)
