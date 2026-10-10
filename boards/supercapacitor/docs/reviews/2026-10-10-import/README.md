# 超级电容格式迁移记录

- 日期：2026-10-10。
- 软件：嘉立创 EDA 专业版 `4.1.71.7a7261be`。
- 来源文件：`3SE 26超级电容.eprj2`，5,648,384 字节，由用户提供；原文件保留在下载目录且逐字节核对未变。
- 适用工程：[本次转换的仓库工程](<../../../hardware/3SE 26超级电容.eprj3>)，不是生产发布版本。
- 原生检查与迁移对照：[verification.json](verification.json)。

## 转换与完整性

在源文件副本上调用客户端 `sys_FileManager.getProjectFile(..., 'epro2')`，取得 331 个原生文档。按文件夹格式分装原理图、原理图配置和 PCB；其中 328 个文档保存在容器内，另 2 个 BOARD 与 1 个 CONFIG 的数据进入工程索引。322 个库及资源文档随图页与 PCB 容器保存，保留源数据中的库绑定和 BLOB。

文档 UUID、图元 ID、记录头与载荷全部保留，只统一行终止与分隔。不重绘、不重新布线、不重新填充铺铜，也不修改规则。工程中的控制板和功率板仍属同一个项目。

## 验证范围

1. 在最终仓库路径重开两页原理图和两块 PCB，并查看原生导出的预览。
2. 四份 JLCEDA 网表转换前后分别一致，仅排除由工程路径决定的 `projectId`。
3. 功率板 70 个器件 / 193 个实体焊盘、控制板 116 个器件 / 347 个实体焊盘的数量一致；器件位置、角度、封装、属性及焊盘网络一致，比较仅排除当前工程的 `libraryUuid`。
4. 逐记录比较容器与转换前的原生导出；原生客户端再次导出的 CONFIG 设置载荷与源工程一致。
5. 两块 PCB 转换前后 DRC 均为 0 项；功率板原理图均为 1 条警告，控制板均为 5 条警告，条目及图元明细见 JSON。

本次是格式迁移验证，未重新核对全部器件手册、制造约束、回流、装配或实物功能。控制板电源网络冲突等原有问题见[板 README](../../../README.md)。

## 格式依据

- [easyeda-eprj3 skill，40c94e1](https://github.com/easyeda/easyeda-eprj3-skill/blob/40c94e129df18e94232f8c94260134bfcac6cabf/SKILL.md)。
- [官方格式 companion，9e42727](https://github.com/easyeda/easyeda-format-skill/blob/9e42727cc4cf13a8e442140e8e7794ae94d5f41b/SKILL.md)。
- [文件夹工程格式](https://github.com/easyeda/easyeda-pro-eprj3-format)。

两份 skill 作为格式参考使用，未安装或运行其作者脚本；本次验证依据为原生客户端回读、网表与规则检查、渲染，以及原始记录对照。
