# 关节电容格式迁移记录

- 日期：2026-10-10。
- 软件：嘉立创 EDA 专业版 `4.1.71.7a7261be`。
- 来源文件：`关节电容.eprj2`，4,509,696 字节，由用户提供；原文件保留在下载目录且逐字节核对未变。
- 适用工程：[本次转换的仓库工程](../../../hardware/关节电容.eprj3)，不是生产发布版本。
- 原生检查与迁移对照：[verification.json](verification.json)。

## 转换与完整性

在源文件副本上调用客户端 `sys_FileManager.getProjectFile(..., 'epro2')`，取得 302 个原生文档。按文件夹格式分装原理图、原理图配置和 PCB；其中 300 个文档保存在容器内，另 1 个 BOARD 与 1 个 CONFIG 的数据进入工程索引。297 个库及资源文档随图页与 PCB 容器保存。

保留文档 UUID、图元 ID、记录头和载荷，仅统一行终止与分隔。工程设置的 META 存入索引 `config`，UNIVERSAL 存入 `config.settings.UNIVERSAL`，其中跨页网络、跨层连接、多部件等开关保持原值；通过原生客户端再次导出，逐项核对设置载荷一致。

未重绘、重新布线、重新填充铺铜或修改设计规则。

## 验证范围

1. 在最终仓库路径重开原理图与 PCB，并查看原生导出的预览。
2. 原理图与 PCB 的 JLCEDA 网表转换前后分别一致，仅排除由工程路径决定的 `projectId`。
3. PCB 保持 121 个器件、391 个实体焊盘；器件位置、角度、封装、属性及焊盘网络一致，比较仅排除当前工程的 `libraryUuid`。
4. 逐记录对照容器与源工程的原生导出；工程设置经原生回导确认保持一致。
5. 原理图转换前后均为 2 条 `PIN2PAD` 错误，客户端返回的图元列表为空；PCB 转换前后 DRC 均为 0 项。

两条引脚 / 焊盘映射错误尚未定位，详见[板 README](../../../README.md)。本次未重新完成器件手册、制造约束、回流、装配或实物功能审查。

## 格式依据

- [easyeda-eprj3 skill，40c94e1](https://github.com/easyeda/easyeda-eprj3-skill/blob/40c94e129df18e94232f8c94260134bfcac6cabf/SKILL.md)。
- [官方格式 companion，9e42727](https://github.com/easyeda/easyeda-format-skill/blob/9e42727cc4cf13a8e442140e8e7794ae94d5f41b/SKILL.md)。
- [文件夹工程格式](https://github.com/easyeda/easyeda-pro-eprj3-format)。

两份 skill 作为格式参考使用，未安装或运行其作者脚本；本次验证依据为原生客户端回读、网表与规则检查、渲染，以及原始记录对照。
