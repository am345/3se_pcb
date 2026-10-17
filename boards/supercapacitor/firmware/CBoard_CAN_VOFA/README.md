# CBoard CAN1 to UART VOFA bridge

This project turns the RoboMaster C board into a CAN-to-serial bridge. 本文档已按 2026-08-16 当前源码同步；该子工程用于接收超级电容控制板遥测并输出 VOFA 可直接读取的 CSV，不是主功率固件。

## Connections

- C board shell CAN1: Pin1 = CANL, Pin2 = CANH.
- Diagnostic firmware sends the same serial data on shell UART1 and shell UART2.
- Connect UART TXD to USB-TTL RX, and connect GND.

## Serial

- 115200, 8N1.
- One CSV line every 10 ms.

## CAN frame and VOFA channels

诊断固件接收经典 CAN 标准数据帧 ID `0x211`。bxCAN 硬件过滤器只允许该 ID 的标准数据帧进入 FIFO0，接收回调仍会再次校验标准帧、数据帧、ID 及 DLC 至少为 8 字节。四个字段均为小端 `int16_t`：

| CAN 字节 | VOFA 通道 | 内容 |
|---|---|---|
| `[0:1]` | CH0 | input power `Pin`, W |
| `[2:3]` | CH1 | output power `Pout`, W |
| `[4:5]` | CH2 | capacitor voltage `Vcap × 100`，串口输出时还原为 V |
| `[6:7]` | CH3 | actual capacitor power `Pcap`, W；正值充电、负值放电 |

CSV channel order:

- CH0: input power Pin, W.
- CH1: output power Pout, W.
- CH2: capacitor voltage Vcap, V.
- CH3: actual capacitor power Pcap, W. Positive means charging, negative means discharging.

## Build and flash

工具脚本只使用 Python 标准库，并根据脚本自身位置解析子工程和 `.build/debug/`，不依赖当前工作目录。以下命令以 `CBoard_CAN_VOFA/` 为当前目录；从任意其他目录运行时，改用指向这些脚本的相对路径或绝对路径即可。

```powershell
# 配置并编译
python tools/一键编译.py

# 默认先编译，再以 4000kHz SWD 烧录、校验并复位
python tools/一键烧录.py

# 可选修改 SWD 频率
python tools/一键烧录.py --freq 4000

# 只读检查解释器和全部外部工具
python tools/tool.py doctor

# 显示解析后的子工程、构建、工具链和 HEX 路径
python tools/tool.py paths
```

外部依赖 `cmake`、`ninja`、`arm-none-eabi-gcc`、`arm-none-eabi-objcopy`、`arm-none-eabi-size` 和 `STM32_Programmer_CLI` 必须位于当前默认环境的 `PATH` 中。可先运行 `doctor`；该命令不会修改工程。高级入口仍支持 `python tools/tool.py configure/build/flash/all`，底层 `flash` 默认烧录已有 HEX，而 `一键烧录.py` 默认先编译。

构建产物：

```text
.build/debug/CBoard_CAN_VOFA.elf
.build/debug/CBoard_CAN_VOFA.hex
.build/debug/CBoard_CAN_VOFA.bin
.build/debug/CBoard_CAN_VOFA.map
```

2026-08-16 当前 `arm-none-eabi-size` ELF 字段统计：

```text
text: 14840 B
data:   100 B
bss:   2132 B
```
