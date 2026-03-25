# qingyu_api_node 自定义接口与运行时切换说明

## 1. 节点功能概览

`qingyu_api_node` 订阅 ROS 2 Topic `qingyu_api`（消息类型 `qingyu_api/msg/QingyuCommand`），将消息字段按固定协议打包后通过串口发送给下位机。

当前打包协议（与 `action_robot_01.cpp` 同一套帧结构）：

- 帧格式：`HEAD1(0xFF) HEAD2(0xFE) CMD LEN PAYLOAD CHECKSUM TAIL(0xFC)`
- `CMD`：由消息字段 `cmd_mode` 决定（0x01 或 0x02）
- `LEN`：固定为 14（7 个 int16 字段，每个 2 字节，大端序）
- `CHECKSUM`：`cmd + len + payload[0..len-1]` 的逐字节求和，取低 8 位

## 2. QingyuCommand 字段与两种模式

消息定义见：`src/qingyu_api/msg/QingyuCommand.msg`

```
float32 x y z roll pitch yaw
uint8 cmd_mode
float32 a1 b1 a2 b2 a3 b3
bool enable
```

### 2.1 cmd_mode = 0x02（逆解/位姿模式）

发送 7 个字段（按顺序）：

1. `x`
2. `y`
3. `z`
4. `roll`
5. `pitch`
6. `yaw`
7. `enable`（true->1.0，false->0.0，再走同样的定点编码）

其它字段（a1/b1/...）不会参与串口发送。

### 2.2 cmd_mode = 0x01（正解/关节模式）

发送 7 个字段（按顺序）：

1. `a1`
2. `b1`
3. `a2`
4. `b2`
5. `a3`
6. `b3`
7. `enable`（同上）

其它字段（x/y/z/roll/pitch/yaw）不会参与串口发送。

## 3. 启动节点与参数配置

常用参数：

- `serial_device`：串口设备路径，例如 `/dev/ttyUSB0`、`/dev/ttyACM0`
- `baudrate`：波特率，例如 `115200`

启动示例：

```bash
ros2 run qingyu_api qingyu_api_node --ros-args \
  -p serial_device:=/dev/ttyUSB0 \
  -p baudrate:=115200
```

说明：

- 模式选择通过消息字段 `cmd_mode` 指定

## 4. 使用 Topic 发送指令（自定义接口）

### 4.1 发送逆解/位姿（cmd_mode=2）

```bash
ros2 topic pub /qingyu_api qingyu_api/msg/QingyuCommand "{
  cmd_mode: 2,
  x: 0.10,
  y: 0.00,
  z: 0.25,
  roll: 0.0,
  pitch: 0.0,
  yaw: 0.0,
  enable: true
}"
```

### 4.2 发送正解/关节（cmd_mode=1）

```bash
ros2 topic pub /qingyu_api qingyu_api/msg/QingyuCommand "{
  cmd_mode: 1,
  a1: 0.10, b1: 0.20,
  a2: 0.30, b2: 0.40,
  a3: 0.50, b3: 0.60,
  enable: true
}"
```

## 5. 节点运行中如何切换模式或串口

模式在运行中通过消息字段 `cmd_mode` 指定（无需修改参数）：

### 5.1 切换发送模式（不重启节点）

```bash
ros2 topic pub /qingyu_api qingyu_api/msg/QingyuCommand "{ cmd_mode: 1, a1: 0.0, b1: 0.0, a2: 0.0, b2: 0.0, a3: 0.0, b3: 0.0, enable: true }"
ros2 topic pub /qingyu_api qingyu_api/msg/QingyuCommand "{ cmd_mode: 2, x: 0.0, y: 0.0, z: 0.0, roll: 0.0, pitch: 0.0, yaw: 0.0, enable: true }"

ros2 topic pub -r 50 /qingyu_api qingyu_api/msg/QingyuCommand "{ cmd_mode: 1, a1: 0.0, b1: 0.0, a2: 0.0, b2: 0.0, a3: 0.0, b3: 0.0, enable: true }"

```
### 5.2 切换串口设备/波特率（不重启节点）

```bash
ros2 param set /qingyu_api_node serial_device /dev/ttyACM0
ros2 param set /qingyu_api_node baudrate 9600
```

行为说明：

- 当 `serial_device` 或 `baudrate` 改变时，节点会尝试关闭并重新打开串口
- 如果重新打开失败，本次参数设置会被拒绝（参数不会生效），节点会尝试恢复原串口配置


# 规范包：干净卸载
```bash
sudo apt remove qingyu-robot-interface
```
# 安装 DEB 包 离线
```bash
sudo dpkg -i /tmp/qingyu-robot-interface_0.0.0-0jammy_arm64.deb 
``` 
# 安装 DEB 包 自动，方便
```bash
sudo apt install /tmp/qingyu-robot-interface_0.0.0-0jammy_arm64.deb
``` 
```bash
dpkg -s qingyu-robot-interface  # 查看包的详细信息（版本、安装路径、依赖等）
dpkg -L qingyu-robot-interface  # 列出包安装的所有文件路径（如可执行文件、配置文件）
``` 
