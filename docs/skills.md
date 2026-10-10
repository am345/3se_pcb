# Skills 使用说明

本页登记工程设计与审查时采用的 skill 来源和使用范围。登记不代表已安装或已运行；本仓库没有内置这些第三方 skill 或自动安装脚本。

选型核查日期：2026-10-10。优先考虑持续维护的版本，但更新时间不能代替工程验证，也不要求用 skill 作者当时使用的模型。外部 skill 与用户要求或 [AGENTS.md](../AGENTS.md) 冲突时，遵循用户要求和本仓库规范。

## 主选来源

| 来源与入口 | 本次核查基线 | 用途与边界 |
| --- | --- | --- |
| [easyeda/easyeda-eprj3-skill](https://github.com/easyeda/easyeda-eprj3-skill/blob/40c94e129df18e94232f8c94260134bfcac6cabf/SKILL.md) | `40c94e1`，2026-09-18 | 嘉立创 `.eprj3` 文件操作及格式验证的参考/工具；不支持旧 SQLite 工程，不提供完整差分布线能力 |
| [easyeda/easyeda-format-skill](https://github.com/easyeda/easyeda-format-skill/blob/9e42727cc4cf13a8e442140e8e7794ae94d5f41b/SKILL.md) | `9e42727`，2026-09-28 | 上述 skill 所需的官方格式 companion；旧地址 `easyeda-pro-format-skill` 重定向至本仓库 |
| [fl4p/kicad-design](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/SKILL.md) | `604669c`，2026-10-05 | 原理图、器件、布局、回流与制造检查的规范参考；不整套启用其自动布线流程；尚未确认明确许可证，不复制入库 |
| [Seahan1/kicad-agent](https://github.com/Seahan1/kicad-agent/blob/cbc0aeedcc1a6a7274d8689fa9e4b4305fa5eb9b/.agents/skills/kicad-schematic-design/SKILL.md) | `cbc0aee`，2026-07-13，MIT | KiCad 原理图绘制、接口定义、可读性与验证流程；主要覆盖原理图 |
| [aklofas/kicad-happy：kicad](https://github.com/aklofas/kicad-happy/blob/v2.3.1/skills/kicad/SKILL.md) | `v2.3.1`，2026-10-06，MIT | KiCad 原理图、PCB 和生产资料分析；是审查工具，不是布局布线器 |

以上日期表示所核查版本的提交/发布时间，不保证每份参考文件都在当天更新。实际使用时记录取得的版本；需要更新时先阅读变更再采用，不在工作中无记录地切换版本。

## 当前日志板：嘉立创工程

`boards/serialleg-logger/hardware/` 使用 `.eprj3`，按以下顺序工作：

1. 阅读板 README、设计说明和现有检查记录，确定要修改的范围。
2. 查询已安装嘉立创客户端的 CLI/API 能力；入口参考[官方命令行说明](https://prodocs.lceda.cn/cn/api/guide/cli.html)，不要猜测命令或 API。
3. 用 `easyeda-eprj3` 脚本生成/修改文件前，读取它的 SKILL.md 和格式 companion 的 SKILL.md、对应 schema、图元说明及实际例子。缺少所需格式资料时先补齐，不能凭记忆编造记录。
4. 原理图和 PCB 的设计方法参考下节通用规范，操作仍使用嘉立创原生工具或经过验证的文件修改方式。
5. 在嘉立创中重开/重载、检查实际渲染、网表一致性和规则结果；覆铜须实际填充。脚本格式检查通过不等于电路或 PCB 检查通过。

`easyeda-eprj3` 的脚本不能完成所有 EDA 功能。例如差分成对布线需在原生工具中完成；不能为了绕过限制改用自动布线器。

`kicad-happy` 的 `.kicad_sch` / `.kicad_pcb` 分析器不直接适用于这个工程。它支持的 PDF/Gerber 分析只能检查相应导出物，不能据此宣称检查了嘉立创原始连接、规则或全部设计状态。没有明确迁移需求，不把本板转换为 KiCad。

## 通用设计规范如何参考

按任务选读 `fl4p/kicad-design` 的文件，而不是执行其完整工作流：

- [SCHEMATIC.md](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/SCHEMATIC.md)：器件语义、极性、连接和原理图完成检查。
- [FOOTPRINTS.md](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/FOOTPRINTS.md)：准确封装、焊盘编号和机械范围。
- [PCB.md](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/PCB.md) 与 [ROUTING.md](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/ROUTING.md)：布局、层叠、参考面、关键网络路径与回流；仅取适用于本设计的规范。
- [POWER.md](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/POWER.md)、[THERMALS.md](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/THERMALS.md)、[RELEASE.md](https://github.com/fl4p/kicad-design/blob/604669c50807ae019aa4b5c7e6acacb833035406/RELEASE.md)：有相应电源、热或制造任务时参考。

**跳过自动布线预试探、Freerouting、KRT 及一切自动寻路步骤。** 不以“只测试不保存”为理由运行它们。数值要求需以本板器件手册、层叠与板厂能力为依据；设计参考不能取代这些来源。

## KiCad 工程的使用顺序

1. 创建/修改原理图时，读取 `kicad-schematic-design`，明确接口、电源和库元件，按可读布局绘制并运行原生 ERC。
2. 布局与布线参考上述规范，使用原生交互式布线；检查覆铜、回流、网络一致性和 DRC。
3. 需要完整设计审查时，读取 `kicad-happy` 的 `skills/kicad/SKILL.md` 和报告说明，再按其任务依赖读取 `datasheets`、`emc`、`spice` 等 companion，不只复制一个孤立 SKILL.md。
4. 分析脚本使用实际保存的工程与数据手册。按该版本说明运行适用的原理图/PCB/交叉分析及相关检查，对照原文件核实发现的问题和误报。
5. KiCad 原生 ERC/DRC、第三方分析、可视检查和实测分别记录。没有安装分析工具或模拟器时如实记录未执行项目，不生成假的通过结论。

审查所需的数据手册和供应商资料查询可直接进行；采购下单不属于设计审查。依赖缺失时优先使用已有原生工具完成可做的检查，说明实际缺口。

## 补充资料与启用方式

[Toaster496/PCB-design](https://github.com/Toaster496/PCB-design) 保留为可选参考，主要是 `pcb-layout` 和 `pcb-routing` 的分区、放置及布线思路。该项目目前只看到 2026-09-22 的两次提交，不作为主工具依赖；其中允许剩余网络自动布线的建议在本仓库不适用。

本仓库当前只登记来源与使用约定，未把第三方 skill 复制、注册或安装到 `.agents/skills/`。后续真正启用某工具时，应核对其许可证、依赖与所需脚本，记录版本及运行方式；不要在本仓库文档里写某个用户目录为所有人的固定安装路径。
