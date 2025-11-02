

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
add_executable(sandbox src/main.cpp)
set_property(TARGET sandbox PROPERTY CXX_STANDARD 20)

target_link_libraries(sandbox PUBLIC mist_engine)