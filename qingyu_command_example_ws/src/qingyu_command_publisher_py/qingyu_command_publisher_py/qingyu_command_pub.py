from __future__ import annotations

import rclpy
from rclpy.node import Node

from qingyu_api.msg import QingyuCommand


class QingyuCommandPublisher(Node):
    def __init__(self) -> None:
        super().__init__("qingyu_api", namespace="/qingyu_api")

        self.declare_parameter("topic", "/qingyu_api")
        self.declare_parameter("publish_hz", 50.0)

        self.declare_parameter("x", 0.0)
        self.declare_parameter("y", 0.0)
        self.declare_parameter("z", 0.0)
        self.declare_parameter("roll", 0.0)
        self.declare_parameter("pitch", 0.0)
        self.declare_parameter("yaw", 0.0)
        self.declare_parameter("cmd_mode", 1)
        self.declare_parameter("a1", 0.0)
        self.declare_parameter("b1", 0.0)
        self.declare_parameter("a2", 0.0)
        self.declare_parameter("b2", 0.0)
        self.declare_parameter("a3", 0.0)
        self.declare_parameter("b3", 0.0)
        self.declare_parameter("enable", True)

        self._topic_name = self.get_parameter("topic").get_parameter_value().string_value
        publish_hz = self.get_parameter("publish_hz").get_parameter_value().double_value
        period_sec = 1.0 / publish_hz if publish_hz > 0.0 else 0.02

        self._publisher = self.create_publisher(QingyuCommand, self._topic_name, 10)
        self._timer = self.create_timer(period_sec, self._on_timer)

    def _on_timer(self) -> None:
        msg = QingyuCommand()
        msg.x = float(self.get_parameter("x").value)
        msg.y = float(self.get_parameter("y").value)
        msg.z = float(self.get_parameter("z").value)
        msg.roll = float(self.get_parameter("roll").value)
        msg.pitch = float(self.get_parameter("pitch").value)
        msg.yaw = float(self.get_parameter("yaw").value)

        cmd_mode = int(self.get_parameter("cmd_mode").value)
        if cmd_mode < 0:
            cmd_mode = 0
        elif cmd_mode > 255:
            cmd_mode = 255
        msg.cmd_mode = cmd_mode

        msg.a1 = float(self.get_parameter("a1").value)
        msg.b1 = float(self.get_parameter("b1").value)
        msg.a2 = float(self.get_parameter("a2").value)
        msg.b2 = float(self.get_parameter("b2").value)
        msg.a3 = float(self.get_parameter("a3").value)
        msg.b3 = float(self.get_parameter("b3").value)
        msg.enable = bool(self.get_parameter("enable").value)

        self._publisher.publish(msg)


def main() -> None:
    rclpy.init()
    node = QingyuCommandPublisher()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()

