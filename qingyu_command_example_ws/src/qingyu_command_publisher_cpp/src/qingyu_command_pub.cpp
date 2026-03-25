#include <chrono>
#include <string>

#include <rclcpp/rclcpp.hpp>

#include "qingyu_api/msg/qingyu_command.hpp"

using namespace std::chrono_literals;

class QingyuCommandPublisher final : public rclcpp::Node
{
public:
  explicit QingyuCommandPublisher(const rclcpp::NodeOptions & options)
  : rclcpp::Node("qingyu_api", "/qingyu_api", options)
  {
    topic_name_ = this->declare_parameter<std::string>("topic", "/qingyu_api");
    publish_hz_ = this->declare_parameter<double>("publish_hz", 50.0);

    this->declare_parameter<double>("x", 0.0);
    this->declare_parameter<double>("y", 0.0);
    this->declare_parameter<double>("z", 0.0);
    this->declare_parameter<double>("roll", 0.0);
    this->declare_parameter<double>("pitch", 0.0);
    this->declare_parameter<double>("yaw", 0.0);
    this->declare_parameter<int>("cmd_mode", 1);
    this->declare_parameter<double>("a1", 0.0);
    this->declare_parameter<double>("b1", 0.0);
    this->declare_parameter<double>("a2", 0.0);
    this->declare_parameter<double>("b2", 0.0);
    this->declare_parameter<double>("a3", 0.0);
    this->declare_parameter<double>("b3", 0.0);
    this->declare_parameter<bool>("enable", true);

    publisher_ = this->create_publisher<qingyu_api::msg::QingyuCommand>(topic_name_, rclcpp::QoS(10));

    const auto period = publish_hz_ > 0.0 ? std::chrono::duration<double>(1.0 / publish_hz_) : 20ms;
    timer_ = this->create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      std::bind(&QingyuCommandPublisher::onTimer, this));
  }

private:
  void onTimer()
  {
    qingyu_api::msg::QingyuCommand msg;
    msg.x = static_cast<float>(this->get_parameter("x").as_double());
    msg.y = static_cast<float>(this->get_parameter("y").as_double());
    msg.z = static_cast<float>(this->get_parameter("z").as_double());
    msg.roll = static_cast<float>(this->get_parameter("roll").as_double());
    msg.pitch = static_cast<float>(this->get_parameter("pitch").as_double());
    msg.yaw = static_cast<float>(this->get_parameter("yaw").as_double());

    msg.cmd_mode = static_cast<uint8_t>(this->get_parameter("cmd_mode").as_int());

    msg.a1 = static_cast<float>(this->get_parameter("a1").as_double());
    msg.b1 = static_cast<float>(this->get_parameter("b1").as_double());
    msg.a2 = static_cast<float>(this->get_parameter("a2").as_double());
    msg.b2 = static_cast<float>(this->get_parameter("b2").as_double());
    msg.a3 = static_cast<float>(this->get_parameter("a3").as_double());
    msg.b3 = static_cast<float>(this->get_parameter("b3").as_double());
    msg.enable = this->get_parameter("enable").as_bool();

    publisher_->publish(msg);
  }

  std::string topic_name_;
  double publish_hz_{50.0};
  rclcpp::Publisher<qingyu_api::msg::QingyuCommand>::SharedPtr publisher_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<QingyuCommandPublisher>(rclcpp::NodeOptions{}));
  rclcpp::shutdown();
  return 0;
}
