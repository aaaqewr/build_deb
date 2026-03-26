```bash
rostopic pub -r 50 /qingyu_api qingyu_api_ros1/QingyuCommand "{x: 0.0, y: 0.0, z: 0.0, roll: 0.0, pitch: 0.0, yaw: 0.0, cmd_mode: 1, a1: 10.5, b1: 20.0, a2: -15.2, b2: 30.1, a3: 0.0, b3: 5.5, enable: true}" -1
```

```bash
rostopic pub -r 50 /qingyu_api qingyu_api_ros1/QingyuCommand "{x: 0.5, y: 0.0, z: 0.2, roll: 0.0, pitch: 0.1, yaw: 0.0, cmd_mode: 2, a1: 0.0, b1: 0.0, a2: 0.0, b2: 0.0, a3: 0.0, b3: 0.0, enable: true}" -1
```

-说明： -1 表示只发送一次消息然后退出。
