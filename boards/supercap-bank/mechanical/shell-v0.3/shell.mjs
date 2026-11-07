// 3SE 超级电容组 V0.3 贴身薄壳（PETG/PLA 打印）：下半 + 上半，在 PCB 板边对插，不用螺丝。
// 下半四周壁顶到 PCB 顶面，再往上伸出一圈内止口；上半的外裙从上面套住止口，靠摩擦和一圈小卡筋扣住。
// 上半内部有 5 条马鞍形压槽压住卧放 10 F，两侧各 3 根细筋压住 PCB 板边。装到车上时壳体两侧和支撑架之间垫 EVA 泡棉。
// 坐标与嘉立创导出的 STEP 相同：原点在 PCB 左下角，x 0～55，y 0～48.5（y = 0 是朝装甲的前沿），PCB 占 z 0～1.6，
// 立放单体从板底往下（34 F 到 z −49），卧放 10 F 在板上（顶到 z 13.6）。
// 用法：node shell.mjs <输出目录> layout-v0.3.json（先在本目录 npm i replicad replicad-opencascadejs）
import opencascade from 'replicad-opencascadejs';
import { setOC, makeBox, makeCylinder, draw, drawRoundedRectangle, exportSTEP, measureVolume } from 'replicad';
import { readFileSync, writeFileSync, mkdirSync } from 'fs';
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
setOC(await opencascade({ locateFile: () => require.resolve('replicad-opencascadejs/wasm') }));
const OUT = process.argv[2] || '.';
mkdirSync(OUT, { recursive: true });

// ---------- 参数（mm） ----------
const P = {
  W: 55, H: 48.5, T: 1.6,          // PCB
  zCell: -49, zTop: 13.6,          // 34 F 底端；卧放 10 F 顶
  c: 0.4,                          // 内部间隙
  t: 2.0, tFront: 2.0, tSlant: 1.2, // 壁厚；前壁上部按装甲 15° 斜面切，斜面段壁厚 1.2（再厚会碰到卧放 10 F 前沿弯折的引脚）
  tFloor: 1.6, tRoof: 1.6,
  r: 3.0,                          // 外形竖棱圆角
  rEdge: 1.2,                      // 顶、底外棱圆角
  split: -1.5,                     // 上下两半的分缝高度（PCB 底面下 1.5，避开前沿装甲斜面最紧的位置）
  joint: { h: 3.0, tongue: 0.9, gap: 0.1 }, // 止口高 3、厚 0.9；止口外面与外裙内面单边间隙 0.1
  bead: { h: 0.3, groove: 0.4, z0: 1.6, z1: 2.2, end: 6 }, // 止口外侧卡筋（离分缝 z0～z1），外裙内侧对应凹槽；离各边端头 end 不做
  press: { board: 0.1, cell: 0.15 }, // 压板细筋、卧放压槽的压紧量
  saddle: { y0: 6, y1: 36, halfW: 4.5, zLow: 10.5 },
  holdY: [8, 24.25, 40.5],         // 压 PCB 板边的细筋位置（左右各 3 根，宽 1.0，伸到板边内 0.6）
  lyingAxisZ: 8.35, lyingR: 5.25,
  // 后壁出线：两根焊线从 J1 竖起后弯向后方，并排从后壁的槽里出去。下半后壁在槽下面有一块挡块，焊线夹在挡块顶面和槽顶之间
  wire: { od: 4.0, clamp: 0.2, xs: [23.4, 27.6], zBase: 6.0, gap: 0.1 }, // 线外径 4.0（12AWG 硅胶线常见值，按实物改）、夹紧 0.2；两根线中心 x；挡块顶面高度；挡块与槽单边间隙
  armorGap: 2.0,
  shiftZ: 2.0,                     // 后壁外沿伸出支撑架背面的距离；等于壁厚时 PCB 后沿离支撑架背面 0.4
};
const COLS = [6.3, 16.9, 27.5, 38.1, 48.7];
const S10 = [[27.5, 30.9], [15.35, 30.95], [20.9, 21.9], [39.65, 30.95], [34.1, 21.9]];
const LEDS = [[14.0, 46.9], [8.6, 46.9], [2.1, 46.9], [33.2, 46.9], [44.3, 46.9], [37.5, 46.9], [9.9, 3.6], [20.5, 3.6], [31.1, 3.6], [41.7, 3.6], [52.3, 3.6]];
const J1 = [27.5, 42.05];

const cav = { x0: -P.c, y0: -P.c, x1: P.W + P.c, y1: P.H + P.c };
const ext = { x0: cav.x0 - P.t, y0: cav.y0 - P.tFront, x1: cav.x1 + P.t, y1: cav.y1 + P.t };
const zFloor = P.zCell - P.tFloor, zSplit = P.split, zJoint = zSplit + P.joint.h;
const zRoofIn = P.zTop + P.c, zRoof = zRoofIn + P.tRoof;
const box = (a, b) => makeBox(a, b);
const T15 = Math.tan(15 * Math.PI / 180);
const cutY = (z, extra = 0) => -67 + T15 * (z - zFloor) + P.armorGap + extra + ext.y1 - P.shiftZ;
const cutter = (extra) => {
  const zA = zFloor - 5, zB = zRoof + 5;
  return draw([cutY(zA, extra), zA]).lineTo([cutY(zB, extra), zB]).lineTo([-60, zB]).lineTo([-60, zA]).close()
    .sketchOnPlane('YZ', [ext.x0 - 5, 0, 0]).extrude(ext.x1 - ext.x0 + 10);
};
// 与外形同心的圆角矩形柱：inset 为向内缩的距离
const rr = (inset, za, zb) => {
  const w = ext.x1 - ext.x0 - 2 * inset, h = ext.y1 - ext.y0 - 2 * inset;
  return drawRoundedRectangle(w, h, Math.max(0.2, P.r - inset)).sketchOnPlane('XY', [(ext.x0 + ext.x1) / 2, (ext.y0 + ext.y1) / 2, za]).extrude(zb - za);
};
const cavity = (za, zb, slant) => { let s = box([cav.x0, cav.y0, za], [cav.x1, cav.y1, zb]); return slant ? s.cut(cutter(P.tSlant / Math.cos(15 * Math.PI / 180))) : s; };
const tongueOut = P.t - P.joint.tongue, skirtIn = tongueOut - P.joint.gap;   // 止口外面 / 外裙内面离外形的距离

// 止口外侧卡筋（下半）与外裙内侧凹槽（上半）：沿四条直边，避开圆角和端头
function beadRing(inset, depth, za, zb) {
  const e = P.bead.end, parts = [];
  const xA = ext.x0 + inset, xB = ext.x1 - inset, yA = ext.y0 + inset, yB = ext.y1 - inset;
  parts.push(box([xA - depth, yA + e, za], [xA, yB - e, zb]), box([xB, yA + e, za], [xB + depth, yB - e, zb]));
  parts.push(box([xA + e, yA - depth, za], [xB - e, yA, zb]), box([xA + e, yB, za], [xB - e, yB + depth, zb]));
  return parts.reduce((a, b) => a.fuse(b));
}

const slotW = 2 * P.wire.od + 0.4, slotC = (P.wire.xs[0] + P.wire.xs[1]) / 2;
const slot = { x0: slotC - slotW / 2, x1: slotC + slotW / 2 }, slotTop = P.wire.zBase + P.wire.od - P.wire.clamp;

// ---------- 下半 ----------
let lower = rr(0, zFloor, zSplit);
try { lower = lower.fillet(P.rEdge, (e) => e.inPlane('XY', zFloor)); } catch (e) { console.log('下半底棱圆角失败'); }
lower = lower.fuse(rr(tongueOut, zSplit - 0.01, zJoint));                        // 止口
lower = lower.cut(cavity(P.zCell, zJoint + 1, false));
lower = lower.fuse(beadRing(tongueOut, P.bead.h, zSplit + P.bead.z0, zSplit + P.bead.z1));
for (const [x, y] of S10) lower = lower.fuse(makeCylinder(4, (-31.5 - 0.1) - P.zCell + 0.01, [x, y, P.zCell - 0.01]).cut(makeCylinder(2.5, 30, [x, y, P.zCell - 0.5])));   // 立放 10 F 支撑管 Ø8/Ø5
lower = lower.fuse(box([slot.x0 + P.wire.gap, cav.y1, zSplit - 0.01], [slot.x1 - P.wire.gap, ext.y1, P.wire.zBase]));   // 出线槽下面的挡块
lower = lower.cut(cutter(0));

// ---------- 上半 ----------
let upper = rr(0, zSplit, zRoof);
try { upper = upper.fillet(P.rEdge, (e) => e.inPlane('XY', zRoof)); } catch (e) { console.log('上半顶棱圆角失败'); }
upper = upper.cut(rr(skirtIn, zSplit - 1, zJoint + 0.1));                         // 外裙里面让出止口
upper = upper.cut(cavity(zSplit - 1, zRoofIn, true));
upper = upper.cut(beadRing(skirtIn, P.bead.groove, zSplit + P.bead.z0 - 0.05, zSplit + P.bead.z1 + 0.05));
// 卧放 10 F 压槽
for (const c of COLS) {
  upper = upper.fuse(box([c - P.saddle.halfW, P.saddle.y0, P.saddle.zLow], [c + P.saddle.halfW, P.saddle.y1, zRoofIn + 0.01])
    .cut(makeCylinder(P.lyingR, P.saddle.y1 - P.saddle.y0 + 2, [c, P.saddle.y0 - 1, P.lyingAxisZ - P.press.cell], [0, 1, 0])));
}
// 压 PCB 板边的细筋：从顶板垂下，压在 PCB 顶面左右边缘 0.6 宽的空带上
for (const y of P.holdY) for (const [xa, xb] of [[cav.x0, 0.6], [P.W - 0.6, cav.x1]]) upper = upper.fuse(box([xa, y - 1.5, P.T - P.press.board], [xb, y + 1.5, zRoofIn + 0.01]));
// 后壁出线槽：下沿开口，扣上半时从上面把两根线压进槽里
upper = upper.cut(box([slot.x0, cav.y1 - 0.01, zSplit - 1], [slot.x1, ext.y1 + 1, slotTop]));
for (const [x, y] of LEDS) upper = upper.cut(makeCylinder(1.0, P.tRoof + 2, [x, y, zRoofIn - 1]));
// 撬口：左右两侧外裙下沿各一个 8 × 1.2 缺口，拆的时候用指甲或一字起插进去
for (const [xa, xb] of [[ext.x0 - 1, ext.x0 + skirtIn + 0.01], [ext.x1 - skirtIn - 0.01, ext.x1 + 1]]) upper = upper.cut(box([xa, 24.25 - 4, zSplit - 1], [xb, 24.25 + 4, zSplit + 1.2]));
upper = upper.cut(cutter(0));

// ---------- 输出 ----------
writeFileSync(`${OUT}/lower.step`, await lower.blobSTEP().text());
writeFileSync(`${OUT}/upper.step`, await upper.blobSTEP().text());
writeFileSync(`${OUT}/lower.stl`, Buffer.from(await lower.blobSTL({ tolerance: 0.02, angularTolerance: 0.1, binary: true }).arrayBuffer()));
writeFileSync(`${OUT}/upper.stl`, Buffer.from(await upper.blobSTL({ tolerance: 0.02, angularTolerance: 0.1, binary: true }).arrayBuffer()));
writeFileSync(`${OUT}/shell_assembly.step`, await exportSTEP([{ shape: lower, name: 'lower_half', color: '#56606e' }, { shape: upper, name: 'upper_half', color: '#d9a441' }]).text());
const bb = (s) => s.boundingBox.bounds.map((p) => p.map((v) => +v.toFixed(2)));
console.log(JSON.stringify({ vol: { lower: +measureVolume(lower).toFixed(0), upper: +measureVolume(upper).toFixed(0) }, lowerBox: bb(lower), upperBox: bb(upper), cutAtJoint: +cutY(zJoint).toFixed(2) }));

// ---------- 检查 ----------
const E = JSON.parse(readFileSync(process.argv[3], 'utf8'));
const H_SMD = (d) => (/^R\d+04$/.test(d) ? 0.6 : /^R/.test(d) ? 0.5 : /^C/.test(d) ? 0.9 : /^U/.test(d) ? 1.45 : /^Q/.test(d) ? 1.25 : /^D\d+0[34]$/.test(d) ? 1.1 : /^D/.test(d) ? 0.8 : /^F/.test(d) ? 2.7 : 0.5);
const bank = [['PCB', box([0, 0, 0], [P.W, P.H, P.T])]];
for (const [x, y] of Object.values(E.S34)) bank.push(['34F', makeCylinder(6.45, 49, [x, y, -49])]);
for (const [x, y] of Object.values(E.S10)) bank.push(['10F立', makeCylinder(5.25, 31.5, [x, y, -31.5])]);
for (const c of E.COLS) {
  bank.push(['10F卧', makeCylinder(5.25, 31.5, [c, 5.25, 8.35], [0, 1, 0])]);
  for (const dx of [-2.5, 2.5]) { bank.push(['卧放引脚', makeCylinder(0.3, 9.65, [c + dx, 1.8, -1.0])]); bank.push(['卧放引脚', makeCylinder(0.3, 3.45, [c + dx, 1.8, 8.35], [0, 1, 0])]); }
}
for (const [x, y] of E.THT) bank.push(['单体焊点', makeCylinder(1.0, 1.5, [x, y, P.T])]);
for (const p of E.parts) { const [a, b, c2, d] = p.body; bank.push([p.des, box([a, b, P.T], [c2, d, P.T + H_SMD(p.des)])]); }
// 焊线：从焊盘竖起，到槽的高度后先横向偏到各自的线位，再往后穿出后壁
const wz = P.wire.zBase + P.wire.od / 2, wr = P.wire.od / 2;
for (const [py, wx] of [[39.55, P.wire.xs[0]], [44.55, P.wire.xs[1]]]) {
  bank.push(['焊线', makeCylinder(wr, wz - P.T, [J1[0], py, P.T])]);
  if (Math.abs(wx - J1[0]) > 0.01) bank.push(['焊线', makeCylinder(wr, Math.abs(wx - J1[0]), [Math.min(wx, J1[0]), py, wz], [1, 0, 0])]);
  bank.push(['焊线', makeCylinder(wr, ext.y1 + 5 - py, [wx, py, wz], [0, 1, 0])]);
}
const hits = {};
for (const [pn, piece] of [['下半', lower], ['上半', upper]]) for (const [n, s] of bank) {
  let v = 0; try { v = measureVolume(piece.intersect(s)); } catch (e) { v = 0; }
  if (v > 1e-4) { const k = `${pn} × ${n}`; hits[k] = +((hits[k] || 0) + v).toFixed(3); }
}
console.log('与电容组重叠（mm³）：', JSON.stringify(hits), '— 上半 × PCB 是压板细筋 0.1、上半 × 10F卧 是压槽 0.15，都是有意的');
let ov = 0; try { ov = measureVolume(lower.intersect(upper)); } catch (e) { ov = -1; }
console.log('上下两半重叠（mm³）：', +ov.toFixed(4));
const verts = (buf) => { const n = buf.readUInt32LE(80), out = []; for (let i = 0; i < n; i++) for (let k = 0; k < 3; k++) { const o = 84 + i * 50 + 12 + k * 12; out.push([buf.readFloatLE(o), buf.readFloatLE(o + 4), buf.readFloatLE(o + 8)]); } return out; };
let gap = Infinity, at = null, top = -Infinity, back = -Infinity;
for (const f of ['lower', 'upper']) for (const [x, y, z] of verts(readFileSync(`${OUT}/${f}.stl`))) {
  const Y = z - zFloor, Z = y - ext.y1 + P.shiftZ, g = Z - (-67 + T15 * Y);
  if (g < gap) { gap = g; at = [f, +x.toFixed(2), +y.toFixed(2), +z.toFixed(2)]; }
  top = Math.max(top, Y); back = Math.max(back, Z);
}
console.log(JSON.stringify({ 离装甲最小: +gap.toFixed(2), 位置: at, 总高: +top.toFixed(2), 伸进车内: +back.toFixed(2), 宽: +(ext.x1 - ext.x0).toFixed(2), 两侧到支撑架各: +((71 - (ext.x1 - ext.x0)) / 2).toFixed(2), 前后深: +(ext.y1 - ext.y0).toFixed(2) }));
