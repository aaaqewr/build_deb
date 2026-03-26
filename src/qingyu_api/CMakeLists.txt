cmake_minimum_required(VERSION 3.8)
project(qingyu_api)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/QingyuCommand.msg"
)

add_executable(qingyu_api_node src/qingyu_api_node.cpp)
add_library(serial_port SHARED src/serial_port.cpp)

target_include_directories(serial_port PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(qingyu_api_node rclcpp)
target_include_directories(qingyu_api_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(qingyu_api_node serial_port)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")
target_link_libraries(qingyu_api_node ${cpp_typesupport_target})

install(TARGETS
  qingyu_api_node
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS
  serial_port
  EXPORT export_serial_port
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_targets(export_serial_port HAS_LIBRARY_TARGET)
ament_export_libraries(serial_port)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
