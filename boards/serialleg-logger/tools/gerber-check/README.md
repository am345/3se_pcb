# Gerber 制造检查脚本

用 Node.js（本机为 v24）直接检查嘉立创 EDA 专业版导出的 Gerber 与 Excellon 文件，不依赖 Python。用法见各脚本首行注释。

- `gerber.js`：RS-274X 解析（圆形/矩形/RoundRect 宏光圈、直线与圆弧、区域、极性）、栅格化、欧氏距离变换、Excellon 钻孔解析。
- `check.js <gerber 目录> <输出 json>`：检查板框闭合与尺寸、铜到板边、丝印到阻焊开窗、丝印出板、阻焊桥宽度、J6 焊盘开窗、钻孔尺寸与孔到板边。
- `silkclose.js <gerber 目录>`：列出丝印到阻焊开窗小于 0.15 mm 的位置。
- `svgview.js <out.svg> x0 y0 x1 y1 层文件|颜色|透明度 ...`：把若干层画成 SVG（坐标单位 mil）。

栅格分辨率 0.01 mm，量测精度约 ±0.01 mm。结果需对照原文件和数据手册判断，不能替代板厂 DFM 审核。
