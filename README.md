# 3SE PCB

3SE 的 PCB 设计仓库。按板子组织完整的可编辑工程、设计说明和验证记录，兼容嘉立创 EDA 专业版与 KiCad。

## 板卡索引

| 板子 | 用途 | 当前设计版本 | 状态 | 最近投板版本 |
| --- | --- | --- | --- | --- |
| [SerialLeg Logger](boards/serialleg-logger/README.md) | USB / UART 接收机器人日志，写入 microSD | R0.1 草稿 | 设计中；尚有 DRC 问题 | 无 |
| [F103 工控板](boards/f103-industrial/README.md) | STM32F103 工控 / 培训板 | V1，已转 `.eprj3` | 设计中；PCB DRC 0 项，原理图有警告 | 未登记 |
| [电容计](boards/capacitance-meter/README.md) | 8–50 F / 3 V 电容测量核心 | 原理图 V0.4 / PCB V0.1 | 设计中；布线未完成 | 无 |
| [3SE 26 超级电容](boards/supercapacitor/README.md) | 超级电容控制板与功率板 | 原工程，已转 `.eprj3`；版本待确认 | 设计中；原理图保留 6 条原有警告 | 待确认 |
| [关节电容](boards/joint-capacitor/README.md) | 关节电容工程 | 原工程，已转 `.eprj3`；版本待确认 | 设计中；原理图保留 2 条原有错误 | 待确认 |

状态统一使用：`设计中`、`待投板`、`已投板`、`已验证`、`停用`。已投板不代表已验证；状态和问题以各板 README 为准。

## 开始使用

```bash
git clone https://github.com/am345/3se_pcb.git
cd 3se_pcb
```

1. 从上面的索引进入对应板子的 README，查看软件版本、接口和已知问题。
2. 在 EDA 中打开该板 `hardware/` 内的原生工程入口。
3. 修改前阅读 [AGENTS.md](AGENTS.md)；新增板子和投板归档遵循[工作流程](docs/workflow.md)，工具选择见 [Skills 使用说明](docs/skills.md)。

嘉立创 `.eprj3` 是文件夹工程：索引、原理图、PCB 和相关资源必须一起保留，不能只下载单个 `.eprj3` 文件。KiCad 工程也须保留其项目库和相对路径引用。

## 目录约定

```text
3se_pcb/
├── README.md                    # 板卡索引
├── AGENTS.md                    # Agent 设计和验证约定
├── docs/
│   ├── workflow.md              # 新建、迁移、协作、投板归档流程
│   └── skills.md                # Skill 来源、版本、用途和限制
├── templates/
│   └── board-readme.md          # 新板 README 模板
└── boards/
    └── <board-id>/
        ├── README.md            # 本板入口及当前状态
        ├── hardware/            # 完整的可编辑工程
        ├── docs/                # 设计依据、接口说明、验证记录
        ├── images/              # 原理图、PCB、实物预览
        └── mechanical/          # 安装尺寸和外壳；需要时创建
```

`libraries/` 仅在多个工程确实需要共享自建库时创建；`scripts/` 仅存放已实际使用的共用工具。板级专用库优先随板保存，不依赖某台电脑的个人库目录。

## 版本与生产文件

- 每块板用一个稳定的英文目录名，例如 `serialleg-logger`。普通迭代由 Git 记录，不复制“最终版”“新最终版”目录。
- 正式投板版本用板名加版本的 Git 标签，例如 `serialleg-logger/r1.0`；标签指向实际生成生产文件的提交。
- 临时导出放各板 `outputs/`，Git 默认忽略。用于支撑结论的检查报告和实测记录放 `docs/`，保留日期、适用版本和检查范围。
- 当前导入的日志板是设计草稿，没有生产发布；导入时的 BOM 和检查报告是历史快照。

## 设计原则

**禁止自动布线，包括自动布线试探。** 先审清原理图，再规划布局、层叠、回流和关键网络路径；使用交互式布线，或按已经逐网确定的路径落图。

ERC（电气规则检查）、DRC（设计规则检查）、数据手册核对和渲染图检查分别记录。仅有零 DRC 错误不能证明功能、制造或实机测试已经通过。

## 参考

- [嘉立创文件夹工程格式](https://github.com/easyeda/easyeda-pro-eprj3-format)
- [嘉立创专业版文件格式文档](https://prodocs.lceda.cn/cn/format/index/)
- [KiCad 官方文档](https://docs.kicad.org/)
