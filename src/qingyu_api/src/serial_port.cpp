#include "qingyu_api/serial_port.hpp"

#include <cerrno> // 系统错误码
#include <cstring> // strerror() - 错误码转字符串

#include <fcntl.h> // open() 标志位
#include <termios.h> // 串口配置核心结构
#include <unistd.h> // read/write/close 系统调用

namespace serial_sender_cpp
{

namespace
{

speed_t toTermiosBaud(int baudrate)
{
  // 将常见整数波特率转换为termios的speed_t常量。
  // 未覆盖到的值统一回退到115200，避免上层配置错误导致无法打开。
  switch (baudrate) {
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    case 230400:
      return B230400;
    case 460800:
      return B460800;
    case 921600:
      return B921600;
    default:
      return B115200;
  }
}

}  // namespace

SerialPort::~SerialPort()
{
  close();
}

bool SerialPort::open(const std::string & device, int baudrate)
{
  // 打开前先关闭，保证重复open不会泄漏fd或残留旧状态。
  close();
  last_error_.clear();

  //    O_RDWR   = 读写模式
  //    O_NOCTTY = 不作为控制终端（防止Ctrl+C等信号干扰）
  //    O_SYNC   = 同步写入（数据立即刷到硬件）
  fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
  if (fd_ < 0) {
    last_error_ = std::string("open: ") + std::strerror(errno);
    return false;
  }

  termios tty{};// 配置结构体清零
  // 读取当前串口配置，后续在其基础上修改。
  if (tcgetattr(fd_, &tty) != 0) {
    last_error_ = std::string("tcgetattr: ") + std::strerror(errno);
    close();
    return false;
  }

  const speed_t speed = toTermiosBaud(baudrate); // 设置波特率
  cfsetospeed(&tty, speed); // 输出波特率
  cfsetispeed(&tty, speed); // 输入波特率

  // 8N1：8bit数据位、无校验、1个停止位；关闭硬件流控；关闭软件流控；
  // 非规范模式：不做行缓冲/回显等，按字节原样收发。
  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 清除数据位大小位，设8位
  tty.c_iflag &= ~IGNBRK; // 不忽略BREAK信号
  tty.c_lflag = 0; // 关闭：回显(ECHO)、规范输入(ICANON)、信号(ISIG)
  tty.c_oflag = 0; // 关闭：输出处理
  // 读超时策略（这里主要用写，不依赖读）：VMIN=0 + VTIME=1 => 最多等待0.1s返回。
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1;
  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // 软件流控（XON/XOFF）关闭
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~(PARENB | PARODD); // 关闭奇偶校验
  tty.c_cflag &= ~CSTOPB; // 1个停止位（非2个）
  tty.c_cflag &= ~CRTSCTS; // 硬件流控（RTS/CTS）关闭

  // 应用配置到串口设备。
  if (tcsetattr(fd_, TCSANOW, &tty) != 0) { // TCSANOW = 立即生效
    last_error_ = std::string("tcsetattr: ") + std::strerror(errno);
    close();
    return false;
  }

  return true;
}

void SerialPort::close()
{
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool SerialPort::isOpen() const
{
  return fd_ >= 0;
}

std::string SerialPort::lastError() const
{
  return last_error_;
}

bool SerialPort::writeBytes(const uint8_t * data, size_t size)
{
  // 未打开直接失败，避免向无效fd写入。
  if (fd_ < 0) {
    last_error_ = "write: not open";
    return false;
  }

  // write(2) 可能出现“部分写入”，循环直到写完全部数据或失败。
  size_t written_total = 0;
  while (written_total < size) {
    const ssize_t n = ::write(fd_, data + written_total, size - written_total);
    if (n < 0) {
      last_error_ = std::string("write: ") + std::strerror(errno);
      return false;
    }
    written_total += static_cast<size_t>(n);
  }

  // 等待输出缓冲区数据真正发出，保证“实时点下发”的时序更确定。
  if (tcdrain(fd_) != 0) {
    last_error_ = std::string("tcdrain: ") + std::strerror(errno);
    return false;
  }

  return true;
}

bool SerialPort::writeString(const std::string & data)
{
  return writeBytes(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

ssize_t SerialPort::readBytes(uint8_t * data, size_t max_size)
{
  if (fd_ < 0) {
    last_error_ = "read: not open";
    return -1;
  }
  const ssize_t n = ::read(fd_, data, max_size); // 非阻塞/超时由VTIME控制
  if (n < 0) {
    last_error_ = std::string("read: ") + std::strerror(errno);
  }
  return n;
}

}
