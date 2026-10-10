# F103 V1 格式迁移记录

- 日期：2026-10-10。
- 客户端：嘉立创 EDA 专业版 `4.1.71.7a7261be`。
- 来源：`D:/RoboMaster/2027_train/train/f103工控板V1.eprj2`，7,417,856 字节；源文件留在原位置，迁移结束时与开始时的副本逐字节相同。
- 维护入口：[仓库内工程](../../../hardware/F103-Industrial-V1.eprj3)。
- 原生检查与对照结果：[verification.json](verification.json)。

## 转换方式

先在副本上调用客户端 `sys_FileManager.getProjectFile(..., 'epro2')` 导出原生记录。按官方文件夹工程结构，将记录拆成原理图、PCB 和原理图规则文件；原 BOARD / CONFIG 数据进入工程索引，符号、封装、器件与 BLOB 资源嵌入文档。

保留了原来的文档 UUID、图元 ID、库绑定、网名与全部图元载荷。仅规范化 DOCHEAD 外壳和文档间分隔符；没有重新绘制、重新布线、重新铺铜或更改规则。原生导出共 300 个文档，298 个文档保存在容器中，另 2 个 BOARD / CONFIG 文档对应工程索引。原理图坐标沿用导出记录的坐标标记语义，由原生客户端读取。

未采用客户端“导入到新建工程”的中间结果：该方式会重建内部 ID，并在本板产生新的网表不匹配提示。最终入库的是保留 ID 的版本。

## 验证范围

1. 在最终仓库路径以原生 CLI 重开原理图和 PCB。
2. 对照原 `.eprj2` 副本与最终工程的 JLCEDA 网表；仅忽略由工程路径决定的 `projectId`，原理图网表和 PCB 网表分别完全一致。
3. 对照 64 个 PCB 器件的全部 API 属性、位置、角度和焊盘网络；仅忽略指向当前工程的 `libraryUuid`，其余完全一致。
4. 原理图与 PCB 的 228 个编号引脚网络一致；API 返回 232 个实体焊盘，不能把实体数量与编号引脚数量混为一谈。
5. 逐记录对照原生导出与最终容器，除上述 DOCHEAD 外壳与分隔符外，记录头和载荷完全一致，覆盖走线、过孔、铺铜、规则及嵌入库。
6. 转换前后原生 PCB DRC 均为 0 项；原理图均为 20 条 `NETOBJECTONGRID` 与 1 条 `ATTRMATCHCODE` 警告，明细见 JSON。
7. 从最终工程导出原理图和 PCB 预览并查看，确认能正常渲染；这不等于重新完成全部电路设计审查。

官方 authoring skill 的静态脚本未通过本板的旧格式记录：脚本对 16 位 UUID、删除占位和 PAD_NET 引用有与历史原生数据不一致的假设。本次没有改写原数据迎合脚本，也没有声称该校验通过；以上原生检查和逐记录对照作为迁移依据。

本次未做实物测试、重新核对全部数据手册、生产导出或板厂 DFM。9 月历史审查独立保存，不能视为当前版本的实物验收结论。

## 参考

- [嘉立创原生 CLI](https://prodocs.lceda.cn/cn/api/guide/cli.html)
- [eprj3 工程结构](https://github.com/easyeda/easyeda-pro-eprj3-format)
- [官方格式文档与坐标标记](https://github.com/easyeda/easyeda-format-skill)
