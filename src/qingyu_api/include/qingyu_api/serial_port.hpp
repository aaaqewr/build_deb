#pragma once

#include <cstdint>
#include <sys/types.h>
#include <string>
#include <vector>

namespace serial_sender_cpp
{

// Linux串口封装（termios）。
// 目标：给上层提供最小且稳定的接口（打开/关闭/写），避免节点代码被系统细节污染。
class SerialPort
{
public:
  SerialPort() = default;
  ~SerialPort();

  SerialPort(const SerialPort &) = delete;
  SerialPort & operator=(const SerialPort &) = delete;
  SerialPort(SerialPort &&) = delete;
  SerialPort & operator=(SerialPort &&) = delete;

  // 打开串口设备（例如/dev/ttyUSB0），并配置为8N1、无流控等常见设置。
  // baudrate：常用波特率会映射到termios常量；未知值默认回退到115200。
  bool open(const std::string & device, int baudrate);
  // 关闭串口（可重复调用）。
  void close();
  // 当前串口是否处于打开状态。
  bool isOpen() const;
  // 最近一次错误信息（打开/写失败等）。
  std::string lastError() const;

  // 原始字节写入；内部保证尽量写完并tcdrain等待发送完成。
  bool writeBytes(const uint8_t * data, size_t size);
  ssize_t readBytes(uint8_t * data, size_t max_size);
  // 便捷接口：写入字符串（按字节原样发送）。
  bool writeString(const std::string & data);

private:
  // 串口文件描述符；<0表示未打开。
  int fd_{-1};
  // 保存最近一次错误，便于上层日志输出。
  std::string last_error_;
};

}
