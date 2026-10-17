# 固件并入前的 git 历史

[`SuperCapFINALSTATE.bundle`](SuperCapFINALSTATE.bundle) 是 [firmware/](../../firmware/) 并入本仓库前的完整 git 历史，2026-10-17 用 `git bundle create --all` 打包，`git bundle verify` 通过。

| 提交 | 日期 | 作者 | 说明 |
| --- | --- | --- | --- |
| `0eaaa59` | 2026-06-04 | Heisenberg | 全新升级功率控制板V0 |
| `394d551` | 2026-06-04 | Heisenberg | 改版V1 |
| `db06a26` | 2026-06-05 | Heisenberg | 修正参数，等待测试动能回收是否可行 |
| `b9847b5` | 2026-07-10 | Heisenberg | Update power control and CAN VOFA tooling |
| `1fafeae` | 2026-10-17 | am345 | 并入前提交原工作区中未提交的改动、文档与调试数据 |

原仓库没有远程地址，来源是解压到 `boards/SuperCap-测试版/SuperCapFINALSTATE/` 的目录。`1fafeae` 的文件树与 `firmware/` 逐文件一致（按 git blob 对比）。

查看历史时克隆到仓库之外的目录：

```bash
git clone SuperCapFINALSTATE.bundle supercap-firmware-history
```

以后的固件修改直接在本仓库的 `firmware/` 中提交，不再更新这个 bundle。
