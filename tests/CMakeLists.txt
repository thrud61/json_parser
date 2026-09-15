enable_testing()

add_executable(json_test
    json_test.cpp
)

add_executable(json_adversarial_test
    adversarial_test.cpp
)

target_include_directories(json_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(json_adversarial_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(json_test
    PRIVATE
        cxx_std_14
)

target_compile_features(json_adversarial_test
    PRIVATE
        cxx_std_14
)

add_test(
    NAME json_test
    COMMAND json_test
)

add_test(
    NAME json_adversarial_test
    COMMAND json_adversarial_test
)
