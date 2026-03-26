#include "rclcpp/rclcpp.hpp"
#include "qingyu_api/msg/qingyu_command.hpp"
#include "qingyu_api/serial_port.hpp"
#include <cstdint>
#include <vector>
#include <mutex>
#include <limits>
#include <cmath>
#include <algorithm>
#include <functional>
#include <string>
#include <optional>

namespace
{
static constexpr uint8_t kPacketHead1 = 0xFF;
static constexpr uint8_t kPacketHead2 = 0xFE;
static constexpr uint8_t kPacketTail = 0xFC;
static constexpr size_t kPointPayloadLen = 14;

uint8_t calc_checksum(uint8_t cmd, uint8_t len, const uint8_t * payload)
{
  uint32_t sum = static_cast<uint32_t>(cmd) + static_cast<uint32_t>(len);
  for (size_t i = 0; i < static_cast<size_t>(len); ++i) {
    sum += static_cast<uint32_t>(payload[i]);
  }
  return static_cast<uint8_t>(sum & 0xFF);
}
}

class Qingyu_Api_Node : public rclcpp::Node
{
public:
  Qingyu_Api_Node(std::string name)
  : Node(name)
  {
    command_subscribe_ =
      this->create_subscription<qingyu_api::msg::QingyuCommand>(
      "qingyu_api",
      10,
      std::bind(&Qingyu_Api_Node::command_callback, this, std::placeholders::_1));
    // 串口参数
    serial_device_ = declare_parameter<std::string>("serial_device", "/dev/ttyUSB0");
    baudrate_ = declare_parameter<int>("baudrate", 115200);
    
    if (!port_.open(serial_device_, baudrate_)) {
      RCLCPP_ERROR(
        this->get_logger(), "打开失败: %s",
        port_.lastError().c_str());
      return;
    }

    params_cb_handle_ = this->add_on_set_parameters_callback(
      std::bind(&Qingyu_Api_Node::onSetParameters, this, std::placeholders::_1));
  }

private:
  rclcpp::Subscription<qingyu_api::msg::QingyuCommand>::SharedPtr command_subscribe_;
  qingyu_api::msg::QingyuCommand api_cmd_{};
  serial_sender_cpp::SerialPort port_;
  std::string serial_device_;
  int baudrate_;
  std::mutex serial_mutex_;
  rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr params_cb_handle_;

  rcl_interfaces::msg::SetParametersResult onSetParameters(const std::vector<rclcpp::Parameter> & params)
  {
    rcl_interfaces::msg::SetParametersResult result;
    result.successful = true;

    std::optional<std::string> new_device;
    std::optional<int> new_baudrate;

    for (const auto & p : params) {
      if (p.get_name() == "serial_device") {
        if (p.get_type() != rclcpp::ParameterType::PARAMETER_STRING) {
          result.successful = false;
          result.reason = "串口设备必须是字符串";
          return result;
        }
        new_device = p.as_string();
      } else if (p.get_name() == "baudrate") {
        if (p.get_type() != rclcpp::ParameterType::PARAMETER_INTEGER) {
          result.successful = false;
          result.reason = "波特率必须是整数";
          return result;
        }
        const int v = p.as_int();
        if (v <= 0) {
          result.successful = false;
          result.reason = "波特率必须 > 0";
          return result;
        }
        new_baudrate = v;
      }
    }

    if (!new_device && !new_baudrate) {
      return result;
    }

    std::lock_guard<std::mutex> lock(serial_mutex_);
    const std::string old_device = serial_device_;
    const int old_baudrate = baudrate_;

    const std::string target_device = new_device ? *new_device : old_device;
    const int target_baudrate = new_baudrate ? *new_baudrate : old_baudrate;

    if (!port_.open(target_device, target_baudrate)) {
      result.successful = false;
      result.reason = std::string("无法打开串行端口: ") + port_.lastError();
      port_.open(old_device, old_baudrate);
      return result;
    }

    serial_device_ = target_device;
    baudrate_ = target_baudrate;
    return result;
  }

  std::vector<uint8_t> buildPointPacket(const qingyu_api::msg::QingyuCommand & msg)
  {
    std::vector<uint8_t> frame;
    frame.reserve(2 + 1 + 1 + kPointPayloadLen + 1 + 1);
    frame.push_back(kPacketHead1);
    frame.push_back(kPacketHead2);
    frame.push_back(msg.cmd_mode);
    frame.push_back(static_cast<uint8_t>(kPointPayloadLen));

    auto encode_to_u16 = [](float v, float scale) -> uint16_t {
      const long scaled_long = std::lround(static_cast<double>(v) * static_cast<double>(scale));
      const int32_t scaled = static_cast<int32_t>(scaled_long);
      const int32_t clamped = std::min<int32_t>(std::max<int32_t>(scaled, std::numeric_limits<int16_t>::min()),
        std::numeric_limits<int16_t>::max());
      const int16_t s16 = static_cast<int16_t>(clamped);
      return static_cast<uint16_t>(s16);
    };

    auto push_u16_be = [&](uint16_t v) {
      frame.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
      frame.push_back(static_cast<uint8_t>(v & 0xFF));
    };
    auto encode_bool_u16 = [](bool v) -> uint16_t {
      return static_cast<uint16_t>(v ? 1 : 0);
    };
    
    if(msg.cmd_mode == 0x01)
    {
      const float kAngleScale = 100.0f;
      push_u16_be(encode_to_u16(static_cast<float>(msg.a1), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.b1), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.a2), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.b2), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.a3), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.b3), kAngleScale));
      push_u16_be(encode_bool_u16(msg.enable));
    }
    else if(msg.cmd_mode == 0x02)
    {
      const float kPosScale = 1000.0f;
      const float kRpyScale = 100.0f;
      push_u16_be(encode_to_u16(static_cast<float>(msg.x), kPosScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.y), kPosScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.z), kPosScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.roll), kRpyScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.pitch), kRpyScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.yaw), kRpyScale));
      push_u16_be(encode_bool_u16(msg.enable));
    }

    const uint8_t checksum =
      calc_checksum(static_cast<uint8_t>(msg.cmd_mode), static_cast<uint8_t>(kPointPayloadLen), frame.data() + 4);
    frame.push_back(checksum);
    frame.push_back(kPacketTail);
    return frame;
  }
  
  bool sendPointToSerial(const qingyu_api::msg::QingyuCommand & msg)
  {
    std::lock_guard<std::mutex> lock(serial_mutex_);

    if (!port_.isOpen()) {
      RCLCPP_ERROR(
        this->get_logger(), "串口打开失败 device=%s baudrate=%d err=%s",
        serial_device_.c_str(), baudrate_, port_.lastError().c_str());
      return false;
    }

    const std::vector<uint8_t> packet = buildPointPacket(msg);
    if (!port_.writeBytes(packet.data(), packet.size())) {
      RCLCPP_ERROR(this->get_logger(), "串口发送失败 err=%s", port_.lastError().c_str());
      return false;
    }
    return true;
  }

  void command_callback(const qingyu_api::msg::QingyuCommand::SharedPtr msg)
  {
    if (!sendPointToSerial(*msg)) {
      RCLCPP_ERROR(this->get_logger(), "发送失败");
    }
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<Qingyu_Api_Node>("qingyu_api_node");
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
