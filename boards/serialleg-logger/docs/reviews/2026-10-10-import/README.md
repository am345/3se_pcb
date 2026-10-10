# R0.1 草稿导入快照

日期：2026-10-10。对象：首次导入本仓库的 SerialLeg Logger R0.1 草稿；对应首次导入提交中的工程文件。

这些文件来自原工程的检查与导出，迁移时未重新运行 ERC/DRC。后续设计变化不能继续沿用这里的结论。原软件报告未完整记录工具版本和全部启用规则，需结合[原检查说明](先读我-检查结果.txt)阅读。

| 文件 | 用途 |
| --- | --- |
| [先读我-检查结果.txt](先读我-检查结果.txt) | 原交付状态、检查范围和未完成项 |
| [BOM.csv](BOM.csv) | 草稿位号清单，非已核准的生产 BOM |
| [design-pin-map.json](design-pin-map.json) | 当时使用的设计连接定义 |
| [connectivity-check.json](connectivity-check.json) | 设计定义与原理图引脚核对结果 |
| [schematic-pcb-check.json](schematic-pcb-check.json) | 原理图与 PCB 网络核对结果 |
| [schematic-netlist.json](schematic-netlist.json) | 原理图网表 |
| [pcb-netlist.json](pcb-netlist.json) | PCB 网表 |
| [schematic-drc.json](schematic-drc.json) | 原理图检查报告，含料号匹配警告 |
| [pcb-drc.json](pcb-drc.json) | PCB 检查报告，含两项 USB 差分长度差错误 |

原说明中提及的设计说明现位于 [design-notes.txt](../../design-notes.txt)，预览位于[原理图图片](../../../images/schematic-preview.png)和 [PCB 图片](../../../images/pcb-preview.png)。

工程包括首轮自动布线，仅作为未完成草稿保存。尚无已核准生产包、日志固件或样板实测，详见[本板状态](../../../README.md)。
