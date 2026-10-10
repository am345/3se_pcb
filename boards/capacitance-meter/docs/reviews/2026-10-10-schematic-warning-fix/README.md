# 2026-10-10 电容计原理图警告修复

接续同日 [PCB 布线修复](../2026-10-10-pcb-repair/README.md)，完成剩余 11 条原理图警告的处理。保存并关闭编辑会话后，使用新的嘉立创客户端会话重开：**原理图 0 错误、0 警告，PCB DRC 0 项**。没有关闭检查或放宽规则。

## 范围与版本

- 软件：嘉立创 EDA 专业版 `4.1.71.7a7261be`，原生 CLI/API。
- Git 基线：`1c5adfb12cb6c7954a1988aa6d8e730d7dd5e1e3`；分支 `codex/capacitance-pcb-fix`。本记录对应接续 PCB 修复后的未提交工作区，原理图 V0.4 / PCB V0.1。
- 工程：[完整原生入口](../../../hardware/电容计-测量核心-PCB-V0.1.eprj3)，须连同整个 `hardware/` 保存。
- 修改范围：NC/DNC 符号和标记、器件物料属性、相关文字排版、PCB 属性同步；电路网络和 PCB 物理布局布线保持不变。

## 警告原因与修复

| 原有警告 | 原因 | 修复 |
| --- | --- | --- |
| 9 条 `NETOBJECTONGRID` | U4、U1、U6 的 1/8 脚为零长度 NC/DNC 引脚，符号内相对 X 为 ±4，与当前 5 单位网格不符；包含引脚组和对应未连接标记 | 两种嵌入符号的 1/8 脚改为相对 X = ±5，同步移动 6 个未连接标记；保留明确的未连接状态 |
| 2 组 `ATTRMATCHCODE`，涉及 38 个器件 | 显示名称与供应商物料不匹配；J3/J4 已填 C124387，但缺少厂家、准确型号及供应商 | 统一 36 个器件的标准值或完整型号，补齐 J3/J4 属性；随后用原生“从原理图导入变更”同步 PCB |

按准确型号手册核对，OPA197IDR 的 1/5/8 脚为 NC。REF5025AIDR 的 1/8 脚为 DNC，7 脚为 NC，因此将 U4.7 原有的 DNC 名称修正为 NC；3 脚 TEMP 保留未使用。没有将这些引脚接入地或其他网络。相同符号修改同步到原理图与 PCB 内嵌库。

标准化后的示例包括 `OPA197IDR`、`REF5025AIDR`、`MCP3424-E/SL`、`MCP4921-E/SN`、`CD74HCT4066M`、`2N3904BU`、`100mΩ`、`0Ω`。原有阻容值、精度、厂家订货号和供应商编号保留；J3/J4 只补齐原有 C124387 对应的 `B-2100S10P-B110` / `Ckmtw(灿科盟)` / `LCSC`，没有换封装。库查询所得封装仍为 `HDR-TH_10P-P2.54-V-M-R2-C5-S2.54`。

接口用途分别保留为 `CONTROL · 控制`、`CALIBRATION · 校准` 图面文字；保留 C5 补偿待验证说明，补充 C8 的 NR 去噪文字、R1 的 ±1% 显示以及分流电阻的 1% 说明。恢复属性位置并检查字号，避免完整型号覆盖引脚或地符号。客户端保存同时规范化了部分隐藏属性的对齐、显示字段及文档时间戳，见源记录对比。

## 验证结果

| 检查 | 结果 |
| --- | --- |
| 原生保存与重开 | 完成；最终规则检查来自新的客户端会话 |
| 原理图规则检查 | 0 错误、0 警告，原始 11 条均已消除 |
| 原生 PCB DRC | 0 项，包含原理图/PCB 同步检查 |
| 逐引脚网络 | 55 个器件、174 个引脚；164 个接网、10 个有意悬空；相对修改前及 PCB 的差异均为 0 |
| 原理图导线及器件位置 | 源记录对比一致；仅未连接引脚/标记和文字发生预期调整 |
| PCB 物理数据 | 布局、焊盘、走线、过孔、板框、覆铜及禁布区不变；173 条铜层折线、91 个过孔 |
| 覆铜状态 | 两层已填充地铜在重开后仍存在，不保留孤岛；67 个禁止覆铜区域保留 |
| 规则 | PCB 规则、网络规则分配和原理图配置不变；无新增规则豁免 |
| 渲染 | 已检查新原理图和 PCB 顶层、底层、丝印；三张 PCB 新导出图与同日 PCB 修复预览逐字节一致 |

10 个有意悬空引脚为 U4.1/3/7/8、U1.1/5/8、U6.1/5/8。所有原有电气连接和 PCB 取样/回流路径均保留。

本次没有重新布线，已有覆铜源数据逐项一致，并由原生检查确认保存的填充结果。没有运行自动布线。官方格式说明和对应 schema 已阅读；独立上游 validator 因缺少 Ajv 未执行，不将原生验证写成其通过记录。

## 证据与预览

- [汇总检查记录](verification.json)、[原有 11 条警告](schematic-drc-before.json)、[原理图最终检查](schematic-drc.json)、[PCB 最终 DRC](pcb-drc.json)。
- [逐引脚网表对比](pin-net-comparison.json)、[38 个器件的属性变化](component-attribute-changes.json)、[源记录与规则对比](source-comparison.json)、[J3/J4 库查询依据](header-library-reference.json)。
- [当前原理图预览](../../../images/schematic-erc-fixed-20261010.png)。[PCB 顶层](../../../images/pcb-top-repaired-20261010.png)、[底层](../../../images/pcb-bottom-repaired-20261010.png)、[丝印](../../../images/pcb-silkscreen-repaired-20261010.png)已用本次新导出核对，图像内容一致。

旧 PCB 修复报告及原理图预览保留为该阶段的历史记录；其“仍有 11 条警告”不再代表当前工程。临时脚本和修改前完整副本位于已忽略的 `outputs/schematic-warning-fix-20261010/`，上述归档证据可独立阅读。

## 未验证项目与依据

本次结果属于源工程规则和一致性检查，电容计仍未实测。Q1 线性 SOA 与散热、恒流回路稳定性、电压/电流/时基校准、≤3% 精度目标及实际装配空间继续按[板 README](../../../README.md)验证。未重新发布生产输出、下单或创建投板标签。

- [TI OPA197 数据手册](https://www.ti.com/lit/ds/symlink/opa197.pdf)：SBOS737C，SOIC-8 引脚。
- [TI REF50 数据手册](https://www.ti.com/lit/gpn/REF50)：SBOS410O，表 5-1 的 REF50xxAI 引脚；没有套用 EI 型号的 1 脚定义。
- [嘉立创器件标准化说明](https://prodocs.lceda.cn/cn/schematic/side-panel-left-device-standardization/)、[原理图设计规则说明](https://prodocs.lceda.cn/cn/schematic/design-design-rule/)。
- 格式与规范参考遵循[仓库 Skills 使用说明](../../../../../docs/skills.md)：`easyeda-eprj3` 的 `40c94e1`、官方格式 companion 的 `9e42727`，以及 `fl4p/kicad-design` 的 `604669c` 原理图规范；没有安装或运行自动布线工具。
