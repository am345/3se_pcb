# SerialLeg 日志板固件

ESP32-S3 作为 USB 主机打开 DM-MC02 的 CDC 虚拟串口，把收到的原始字节写入 microSD。适用于 [日志板](../README.md) 2026-10-16 丝印与 USB 短线修订版的引脚。

**状态：v0.2.0（只在遥控使能时记录、按帧类型统计），已编译和主机测试，未烧录；v0.1.0 已在样板上接 DM-MC02 持续记录。** 前提是 J2 的 VBUS 有偏置（样板为 3V3 经 1 kΩ 飞线，原因见[USB 联调记录](../docs/reviews/2026-10-27-usb-bringup/README.md)）。插拔重连、指示灯、按键暂停、拔卡/断电后的文件完整性还没有实测。

| 项目 | 内容 |
| --- | --- |
| 框架 | ESP-IDF v5.2.x（本机用 v5.2.6 编译通过），目标 `esp32s3` |
| 依赖 | `espressif/usb_host_cdc_acm` 2.1.0，版本锁在 `dependencies.lock` |
| 数据源 | [serialleg2026](https://gitee.com/SEU-3SE/serialleg2026) 主控 USB CDC（STM32 虚拟串口 0483:5740），logger_link 帧协议版本 3（主控 `Custom/app/telemetry/task_logger_link.h`，2026-10-27 前名为 NX 通讯 `task_nx_comm.h`，格式未变）；按其提交 `5b6f03d`、`e3d9167` 核对 STATE 帧。v0.2.0 对应主控 `refactor/layering` 上新增 MOTOR / EVENT / HEALTH / POLICY / CONTROL 帧的版本（帧定义见主控 `Custom/app/telemetry/logger_link.md`），默认约 200 KB/s；旧主控只发 STATE，同样可记录 |
| 镜像 | 约 355 KB，默认分区表单 app 1 MB |

## 功能

- 只读：日志板不向主控发数据，USB OUT 端点不分配缓冲。接上设备后会置一次 DTR，主控的 ST 虚拟串口不依赖它。
- USB 接收回调只把数据非阻塞写入 PSRAM 中 1 MB 的环形缓冲；缓冲满时丢弃，并在丢弃统计里计数。记录任务从缓冲取数据，经 32 KB 内部 DMA 缓冲写卡，每 1 s `fsync` 一次。
- **只在遥控器使能时记录**（默认）：从 STATE 帧读出 `output_enabled`。未使能时数据只进 PSRAM 预缓冲（768 KB，保留最近 3 s），不写卡；使能时新建 `LOGnnnnn.BIN`，先写预缓冲再实时记录；失能后再记 3 s 才关闭，期间重新使能续写同一文件。USB 断开、按键停止、卡错误时立即关闭。menuconfig 可改为每次 USB 连接都记录。
- 文件序号延续卡上已有的最大值，单个文件写到 256 MB 后自动接续下一个序号。
- 同名的 `LOGnnnnn.TXT` 记录固件版本、USB VID/PID、打开时间、记录模式、前后缓冲时长和预缓冲字节数，关闭时追加关闭原因（`disabled`、`usb_disconnect`、`key_stop`、`rotate`、`write_error`）、各类帧数、CRC 错误、跳号、缓冲丢弃字节、缓冲峰值和最长写卡耗时；帧统计从文件打开起算，不含预缓冲。突然断电时 TXT 中没有关闭记录，BIN 中保留到最后一次 `fsync` 的数据。
- 板上解析 logger_link 帧（`main/link_frame.c`）只用于统计，写卡内容始终是原始字节流。帧号跳号也包含主控 USB 忙时、以及刚连上而尚未配置完成时没有发出的帧（实测连接开头常有 1～2 个）；日志板自己丢的数据要看 `logger_drop_bytes`。
- UART0（J4，115200）每 5 s 打印一行统计：`en` 为主控使能，`st/mo/ev/he/po/ct/ot` 为各类帧数，`pre` 为预缓冲字节数； `port=c? p? dp? dm?` 是主机端口寄存器的连接、供电和 D+/D− 电平，用于现场排查 USB。插着设备且连接正常时为 `c1 p1`；不插线应为 `dp0 dm0`。

## 指示灯与按键

| 状态 | D1 记录灯 | D2 错误灯 |
| --- | --- | --- |
| 上电自检 | 亮 0.3 s | 亮 0.3 s |
| 卡就绪，等 USB 设备 | 每秒闪一下 | 灭 |
| USB 已连接，未使能（待命，数据进预缓冲）或 1 s 内没有数据 | 2 Hz 闪 | 灭 |
| 使能中正在写卡（含失能后的 3 s） | 常亮 | 灭；本文件有缓冲丢弃或 CRC 错误时每秒闪一下 |
| 按键暂停 | 每 2 s 双闪 | 灭 |
| 未插卡 | 灭 | 常亮 |
| 挂载或写卡失败（每 2 s 重试） | 灭 | 5 Hz 闪 |
| 剩余空间小于 8 MB | 灭 | 1 Hz 闪 |

SW2（BOOT）在启动后用作“停止/继续”键，**按住约 1 s 生效**：记录时会写完缓冲、关闭文件并进入暂停，这时可以拔卡或断电；再按住 1 s 恢复，下一包数据进新文件。要求按住是因为下载器的 DTR 接在 BOOT 上，串口工具打开/关闭串口时的短脉冲曾被当成按键导致误暂停。复位时按住 SW2 仍然进入下载模式。

## 引脚

引脚定义在 [`main/board.h`](main/board.h)，来源是当前 PCB 中 U1 焊盘的网络：

| 功能 | GPIO |
| --- | --- |
| D1 记录灯 / D2 错误灯（高电平点亮） | 8 / 9 |
| SW2 BOOT 键 | 0 |
| SD CLK / CMD / D0 / D1 / D2 / D3 | 14 / 13 / 15 / 16 / 11 / 12 |
| SD_CD_N | 17 |
| USB D- / D+ | 19 / 20 |
| UART0 TX / RX（J4） | 43 / 44 |

卡检测为“插卡为低”，2026-10-26 已在样板上确认（无卡、插卡均与日志一致）。上电日志会打印“卡检测：有卡/无卡”。

## 构建与烧录

在已导出 ESP-IDF v5.2 环境的终端中运行：

```bash
idf.py set-target esp32s3
```

```bash
idf.py build
```

下载器接 J4：GND–1、下载器 TX–4 PROG_RX、下载器 RX–3 PROG_TX；3V3 脚只作参考，不接下载器电源，板子由 P16 供电。样板用 MuseLab ESPLink（esp-usb-bridge，USB 303A:1002）验证过：它同时出现一个 U 盘和一个 USB 串口，用串口那一路；它的 EN、BOOT 接到 J4 的 5、6 脚后可以自动进下载模式。没接 EN/BOOT 时，按住 SW2、按一下 SW1 手动进入。

```bash
idf.py -p COM18 flash monitor
```

`COM18` 换成实际串口，注意不要选成蓝牙串口。插拔 ESPLink 后若串口上没有任何输出，多半是 DTR/RTS 的初始电平让芯片停在复位或下载模式，按一下 SW1 或用 `idf.py -p COM18 monitor` 复位即可。

首次构建会从乐鑫组件仓库下载 `usb_host_cdc_acm` 到 `managed_components/`，该目录和 `build/`、`sdkconfig` 不入库。`sdkconfig.defaults` 和 `main/Kconfig.projbuild` 保持纯 ASCII，因为中文 Windows 上 kconfgen 按系统代码页读取它们，含中文会报 `UnicodeDecodeError`。

`idf.py menuconfig` → `SerialLeg Logger` 中可以调整：USB VID/PID（都为 0 时打开第一个 CDC 设备）、环形缓冲大小、SD 时钟（默认 20 MHz）、`fsync` 周期、单文件上限、卡检测、统计打印周期，以及记录模式、使能前保留时长、失能后继续时长和预缓冲大小。

## 解码日志

[`tools/link_decode.mjs`](tools/link_decode.mjs) 用 Node.js 把 BIN 转成 CSV，并打印帧统计：

```bash
node tools/link_decode.mjs LOG00001.BIN
```

每种帧输出一个 CSV：`_state`、`_motor`、`_event`（附事件名）、`_health`、`_policy`、`_control`，没有的类型不输出。字段表按主控 `task_logger_link.h` 的版本 3 结构编写，长度与主控的 `_Static_assert` 一致；原 `target_seq`、`target_age_ms`、`target_valid` 三列改名为 `policy_seq`、`policy_age_ms`、`policy_valid`，含义是板端策略目标的序号、新旧和有效性；主控已不发延迟帧，旧日志里若有其它帧类型只计入 `other`。主控改了负载结构后，这个脚本要同步修改。

## 验证记录

| 检查 | 日期 / 版本 | 结果 |
| --- | --- | --- |
| 编译 | 2026-10-26，v0.1.0，ESP-IDF v5.2.6 | 通过，无编译警告 |
| 帧解析（主机） | 2026-10-26 | 用 Node 按主控查表 CRC 生成 1020 帧合成流，其中 1 帧损坏、跳号 3、混入噪声；`link_frame.c`（原 `nx_frame.c`）以不规则分块喂入（Strawberry gcc 编译），与 `link_decode.mjs` 都得到 1019 帧正确、1 个 CRC 错误、4 个跳号 |
| 样板烧录与启动 | 2026-10-26，v0.1.0 | 经 ESPLink（COM18）自动进入下载模式并烧录，哈希校验通过。芯片 ESP32-S3（QFN56）rev v0.2，内置 4 MB Flash、2 MB PSRAM；PSRAM 内存自检通过，1 MB 环形缓冲分配在 PSRAM，启动后运行无复位 |
| 卡检测与 SD 挂载 | 2026-10-26，v0.1.0 | 无卡时为 `NO_CARD`，带电插卡后转为 `IDLE`；SK32G 32 GB SDHC，4 线 20 MHz 挂载成功。首次统计剩余空间约 2.8 s |
| 接 DM-MC02 记录 | 2026-10-27，v0.1.0 | J2 VBUS 无偏置时根端口复位反复失败；经 1 kΩ 接 3V3 后上电约 1 s 枚举成功，连续记录中每 5 s 500 帧，CRC 错误、缓冲丢弃、开头丢弃均为 0，单次写卡最长约 33 ms，环形缓冲峰值约 166 B。详见[联调记录](../docs/reviews/2026-10-27-usb-bringup/README.md) |
| v0.2.0 编译与主机测试 | 2026-10-27 | 编译无警告。预缓冲（`preroll.c`）主机测试：按时间过期、容量溢出、回绕、超大块、累计约 2 亿字节均正确。以主控 `task_logger_link.h` 结构体组帧 507 帧（含 1 帧损坏），`link_decode.mjs` 六种帧逐字段与赋值一致，`link_frame.c` 分类计数与使能边沿正确 |
| 待测 | — | v0.2.0 与新主控固件上板：使能触发与前后 3 s、指示灯、约 200 KB/s 下的丢帧与写卡耗时；以及 | USB 插拔重连、长时间写卡、指示灯、按键暂停、拔卡/断电后的文件完整性；从卡上取 BIN 用 `link_decode.mjs` 核对 |
