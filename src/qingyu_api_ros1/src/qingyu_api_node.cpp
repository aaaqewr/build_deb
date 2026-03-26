#include <ros/ros.h>
#include <qingyu_api_ros1/QingyuCommand.h>
#include "qingyu_api_ros1/serial_port.hpp"
#include <cstdint>
#include <vector>
#include <mutex>
#include <limits>
#include <cmath>
#include <algorithm>
#include <functional>
#include <string>

namespace {
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

class QingyuApiNode
{
public:
  QingyuApiNode(ros::NodeHandle& nh, ros::NodeHandle& pnh)
  {
    sub_ = nh.subscribe("qingyu_api", 10, &QingyuApiNode::commandCallback, this);
    pnh.param<std::string>("serial_device", serial_device_, std::string("/dev/ttyUSB0"));
    pnh.param<int>("baudrate", baudrate_, 115200);

    if (!port_.open(serial_device_, baudrate_)) {
      ROS_ERROR("打开失败: %s", port_.lastError().c_str());
    }
  }

private:
  ros::Subscriber sub_;
  qingyu_api_ros1::QingyuCommand api_cmd_{};
  serial_sender_cpp::SerialPort port_;
  std::string serial_device_;
  int baudrate_;
  std::mutex serial_mutex_;

  std::vector<uint8_t> buildPointPacket(const qingyu_api_ros1::QingyuCommand & msg)
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

    if (msg.cmd_mode == 0x01) {
      const float kAngleScale = 100.0f;
      push_u16_be(encode_to_u16(static_cast<float>(msg.a1), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.b1), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.a2), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.b2), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.a3), kAngleScale));
      push_u16_be(encode_to_u16(static_cast<float>(msg.b3), kAngleScale));
      push_u16_be(encode_bool_u16(msg.enable));
    } else if (msg.cmd_mode == 0x02) {
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

  bool sendPointToSerial(const qingyu_api_ros1::QingyuCommand & msg)
  {
    std::lock_guard<std::mutex> lock(serial_mutex_);

    if (!port_.isOpen()) {
      ROS_ERROR("串口打开失败 device=%s baudrate=%d err=%s",
        serial_device_.c_str(), baudrate_, port_.lastError().c_str());
      return false;
    }

    const std::vector<uint8_t> packet = buildPointPacket(msg);
    if (!port_.writeBytes(packet.data(), packet.size())) {
      ROS_ERROR("串口发送失败 err=%s", port_.lastError().c_str());
      return false;
    }
    return true;
  }

  void commandCallback(const qingyu_api_ros1::QingyuCommand::ConstPtr& msg)
  {
    if (!sendPointToSerial(*msg)) {
      ROS_ERROR("发送失败");
    }
  }
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "qingyu_api_node");
  ros::NodeHandle nh;
  ros::NodeHandle pnh("~");
  QingyuApiNode node(nh, pnh);
  ros::spin();
  return 0;
}
