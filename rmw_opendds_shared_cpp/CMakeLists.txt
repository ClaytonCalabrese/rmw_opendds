cmake_minimum_required(VERSION 3.5)

project(rmw_opendds_shared_cpp)

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(opendds_cmake_module REQUIRED)

find_package(OpenDDS QUIET)

if(NOT OpenDDS_FOUND)
  ament_package()
  message(WARNING "Could not find OpenDDS - skipping '${PROJECT_NAME}'")
  return()
endif()
find_package(rcutils REQUIRED)

# rcutils package changes CXX standard to 11
if(${CMAKE_CXX_STANDARD} LESS 14)
  set(CMAKE_CXX_STANDARD 14)
endif()

find_package(rmw REQUIRED)

set(opendds_libs_1
  OpenDDS::Rtps_Udp
)

include_directories(include)
ament_export_include_directories(include)
ament_export_dependencies(rmw)

add_library(
  rmw_opendds_shared_cpp
  SHARED
  src/condition_error.cpp
  src/count.cpp
  src/demangle.cpp
  src/guard_condition.cpp
  src/init.cpp
  src/namespace_prefix.cpp
  src/node.cpp
  src/node_names.cpp
  src/qos.cpp
  src/names_and_types_helpers.cpp
  src/node_info_and_types.cpp
  src/service_names_and_types.cpp
  src/topic_names_and_types.cpp
  src/trigger_guard_condition.cpp
  src/wait_set.cpp
  src/types/custom_data_reader_listener.cpp
  src/types/custom_publisher_listener.cpp
  src/types/custom_subscriber_listener.cpp)

target_link_libraries(rmw_opendds_shared_cpp ${opendds_libs_1})

ament_target_dependencies(rmw_opendds_shared_cpp
  "rcutils"
  "rmw")
ament_export_libraries(rmw_opendds_shared_cpp ${opendds_libs_1})

if(WIN32)
  # Causes the visibility macros to use dllexport rather than dllimport
  # which is appropriate when building the dll but not consuming it.
  target_compile_definitions(rmw_opendds_shared_cpp
    PRIVATE "RMW_OPENDDS_SHARED_CPP_BUILDING_DLL"
            "_CRT_NONSTDC_NO_DEPRECATE")
endif()

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()

install(
  DIRECTORY include/
  DESTINATION include
)

install(
  TARGETS rmw_opendds_shared_cpp
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
