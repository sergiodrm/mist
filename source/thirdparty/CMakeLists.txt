
add_subdirectory(cppcoda)

# fetch yamp-cpp
include(FetchContent)

FetchContent_Declare(
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG a83cd31
)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "")
FetchContent_MakeAvailable(yaml-cpp)

option(TRACY_ENABLE "" ON)
option(TRACY_ON_DEMMAND "" ON)
add_subdirectory(tracy)


set_target_properties(cppcoda_lib PROPERTIES FOLDER "ThirdParty")
set_target_properties(yaml-cpp PROPERTIES FOLDER "ThirdParty")
set_target_properties(yaml-cpp-parse PROPERTIES FOLDER "ThirdParty")
set_target_properties(yaml-cpp-read PROPERTIES FOLDER "ThirdParty")
set_target_properties(yaml-cpp-sandbox PROPERTIES FOLDER "ThirdParty")