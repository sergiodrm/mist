

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)
add_executable(sandbox src/main.cpp)
set_property(TARGET sandbox PROPERTY CXX_STANDARD 20)

if (RBE_VK)
add_compile_definitions(RBE_VK)
endif(RBE_VK)
target_link_libraries(sandbox PUBLIC mist_engine)