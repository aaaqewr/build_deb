cmake_minimum_required(VERSION 3.8)
project(qingyu_command_publisher_cpp)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(qingyu_api REQUIRED)

add_executable(qingyu_command_pub src/qingyu_command_pub.cpp)
ament_target_dependencies(qingyu_command_pub rclcpp qingyu_api)

install(TARGETS qingyu_command_pub
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
