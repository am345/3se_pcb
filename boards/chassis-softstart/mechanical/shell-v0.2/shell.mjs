// 3SE 底盘缓启动 V0.2（焊线版，35.5 × 17 mm）的打印保护壳：底壳 + 上盖，在焊线中心高度对扣，不用螺丝。
// 底壳是平底，用 VHB 一类双面胶贴在底盘上；四根 14AWG 焊线从两端水平引出，夹在底壳和上盖的半圆缺口之间。
// 全封闭：底面测试点不开窗；两只指示灯上方的顶板从里面减薄到 0.4，用浅色料打印可以透光。
// 坐标与嘉立创 PCB 相同：原点在板子左下角，x 0～35.5，y 0～17，PCB 占 z 0～1.6，器件在顶面。
// 用法：node shell.mjs <输出目录>（先在本目录 npm i replicad replicad-opencascadejs）
import opencascade from 'replicad-opencascadejs';
import { setOC, makeBox, makeCylinder, drawRoundedRectangle, exportSTEP, measureVolume } from 'replicad';
import { readFileSync, writeFileSync, mkdirSync } from 'fs';
import { createRequire } from 'module';
const require = createRequire(import.meta.url);
setOC(await opencascade({ locateFile: () => require.resolve('replicad-opencascadejs/wasm') }));
const OUT = process.argv[2] || '.';
mkdirSync(OUT, { recursive: true });

// ---------- 参数（mm） ----------
const P = {
  W: 35.5, H: 17, T: 1.6,            // PCB
  c: 0.4,                            // 板边到内壁的间隙
  t: 2.0, tFloor: 1.2, tRoof: 1.2,   // 壁厚
  under: 1.5,                        // 板底到底壳内底：焊线芯从底面伸出和焊锡
  over: 4.6,                         // 板顶到上盖内顶：器件最高约 2.7（C4 1210），焊线顶约 3.9
  r: 2.0, rTop: 0.8,                 // 竖棱圆角；顶面外棱圆角（底面不倒圆，留平面贴胶）
  wire: { od: 3.6, clamp: 0.2, lift: 0.3 }, // 14AWG 硅胶线外径（按实物改）、上盖缺口压线量、线底离板面
  joint: { h: 2.0, tongue: 0.9, gap: 0.1 }, // 止口高、厚，止口与外裙单边间隙
  bead: { h: 0.3, groove: 0.4, z0: 0.9, z1: 1.4, end: 4 }, // 卡筋（离分缝 z0～z1），只做在两条长边
  ledge: { w: 0.5, x0: 5.5, x1: 30.0 },   // 底壳长边托板的台阶：伸到板边内 0.5，避开四角焊线焊盘
  press: { y0: 6.5, y1: 10.5, w: 0.5, dz: 0.05 }, // 上盖两端压板筋：压在板子短边中段 0.5 宽的空带上
  led: { d: 3.0, skin: 0.4 },
};
const PADS = [[2.5, 14.5, 'J1 IN+'], [2.5, 2.5, 'J2 IN−'], [33.0, 14.5, 'J3 OUT+'], [33.0, 2.5, 'J4 OUT−']];
const LEDS = [[24.8, 1.0, 'LED1 绿'], [6.2, 4.6, 'LED2 红']];

const cav = { x0: -P.c, y0: -P.c, x1: P.W + P.c, y1: P.H + P.c, z0: -P.under, z1: P.T + P.over };
const ext = { x0: cav.x0 - P.t, y0: cav.y0 - P.t, x1: cav.x1 + P.t, y1: cav.y1 + P.t, z0: cav.z0 - P.tFloor, z1: cav.z1 + P.tRoof };
const zWire = P.T + P.wire.lift + P.wire.od / 2;   // 焊线中心高度，也是分缝
const zSplit = zWire, zJoint = zSplit + P.joint.h;
const box = (a, b) => makeBox(a, b);
const rr = (inset, za, zb) => drawRoundedRectangle(ext.x1 - ext.x0 - 2 * inset, ext.y1 - ext.y0 - 2 * inset, Math.max(0.2, P.r - inset))
  .sketchOnPlane('XY', [(ext.x0 + ext.x1) / 2, (ext.y0 + ext.y1) / 2, za]).extrude(zb - za);
const cavity = (za, zb) => box([cav.x0, cav.y0, za], [cav.x1, cav.y1, zb]);
const tongueOut = P.t - P.joint.tongue, skirtIn = tongueOut - P.joint.gap;
// 沿 x 方向穿过两端壁的圆柱（焊线出口），只保留 z 方向的上半或下半由调用处裁切
const endBore = (y, r) => [makeCylinder(r, 10, [ext.x0 - 5, y, zWire], [1, 0, 0]), makeCylinder(r, 10, [ext.x1 - 5, y, zWire], [1, 0, 0])];
const wireYs = [2.5, 14.5];

function beadRing(inset, depth, za, zb) {   // 只做两条长边
  const e = P.bead.end, xA = ext.x0 + inset + e, xB = ext.x1 - inset - e, yA = ext.y0 + inset, yB = ext.y1 - inset;
  return box([xA, yA - depth, za], [xB, yA, zb]).fuse(box([xA, yB, za], [xB, yB + depth, zb]));
}

// ---------- 底壳 ----------
let lower = rr(0, ext.z0, zSplit);
lower = lower.fuse(rr(tongueOut, zSplit - 0.01, zJoint));
lower = lower.cut(cavity(cav.z0, zJoint + 1));
lower = lower.fuse(beadRing(tongueOut, P.bead.h, zSplit + P.bead.z0, zSplit + P.bead.z1));
for (const [ya, yb] of [[cav.y0 - 0.01, P.ledge.w], [P.H - P.ledge.w, cav.y1 + 0.01]]) lower = lower.fuse(box([P.ledge.x0, ya, cav.z0 - 0.01], [P.ledge.x1, yb, 0]));
// 焊线缺口：底壳下半圆 + 止口上一段让开
for (const y of wireYs) for (const b of endBore(y, P.wire.od / 2)) lower = lower.cut(b.cut(box([ext.x0 - 9, y - 5, zSplit + 0.0], [ext.x1 + 9, y + 5, zSplit + 9])));
for (const y of wireYs) for (const b of endBore(y, P.wire.od / 2 + 0.1)) lower = lower.cut(b.cut(box([ext.x0 - 9, y - 5, zSplit - 9], [ext.x1 + 9, y + 5, zSplit])));
for (const y of wireYs) lower = lower.cut(box([ext.x0 - 1, y - P.wire.od / 2 - 0.1, zSplit - 0.01], [ext.x0 + P.t + 0.01, y + P.wire.od / 2 + 0.1, zJoint + 1]))
  .cut(box([ext.x1 - P.t - 0.01, y - P.wire.od / 2 - 0.1, zSplit - 0.01], [ext.x1 + 1, y + P.wire.od / 2 + 0.1, zJoint + 1]));

// ---------- 上盖 ----------
let upper = rr(0, zSplit, ext.z1);
try { upper = upper.fillet(P.rTop, (e) => e.inPlane('XY', ext.z1)); } catch (e) { console.log('上盖顶棱圆角失败'); }
upper = upper.cut(rr(skirtIn, zSplit - 1, zJoint + 0.1));
upper = upper.cut(cavity(zSplit - 1, cav.z1));
upper = upper.cut(beadRing(skirtIn, P.bead.groove, zSplit + P.bead.z0 - 0.05, zSplit + P.bead.z1 + 0.05));
// 焊线缺口：上盖上半圆，半径比线小 clamp，扣上后压住线
for (const y of wireYs) for (const b of endBore(y, P.wire.od / 2 - P.wire.clamp)) upper = upper.cut(b);
// 两端压板筋
for (const [xa, xb] of [[cav.x0, P.press.w], [P.W - P.press.w, cav.x1]]) upper = upper.fuse(box([xa, P.press.y0, P.T - P.press.dz], [xb, P.press.y1, cav.z1 + 0.01]));
// 指示灯透光：顶板从里面减薄
for (const [x, y] of LEDS) upper = upper.cut(makeCylinder(P.led.d / 2, P.tRoof - P.led.skin + 0.01, [x, y, cav.z1 - 0.01]));

// ---------- 输出 ----------
writeFileSync(`${OUT}/lower.step`, await lower.blobSTEP().text());
writeFileSync(`${OUT}/upper.step`, await upper.blobSTEP().text());
writeFileSync(`${OUT}/lower.stl`, Buffer.from(await lower.blobSTL({ tolerance: 0.01, angularTolerance: 0.1, binary: true }).arrayBuffer()));
writeFileSync(`${OUT}/upper.stl`, Buffer.from(await upper.blobSTL({ tolerance: 0.01, angularTolerance: 0.1, binary: true }).arrayBuffer()));
writeFileSync(`${OUT}/shell_assembly.step`, await exportSTEP([{ shape: lower, name: 'lower', color: '#56606e' }, { shape: upper, name: 'upper', color: '#d9a441' }]).text());
const bb = (s) => s.boundingBox.bounds.map((p) => p.map((v) => +v.toFixed(2)));
console.log(JSON.stringify({ vol: { lower: +measureVolume(lower).toFixed(0), upper: +measureVolume(upper).toFixed(0) }, lowerBox: bb(lower), upperBox: bb(upper),
  外形: [+(ext.x1 - ext.x0).toFixed(2), +(ext.y1 - ext.y0).toFixed(2), +(ext.z1 - ext.z0).toFixed(2)], 分缝z: +zSplit.toFixed(2) }));

// ---------- 检查：器件按封装最大外形估的方块 ----------
// [位号, x0, y0, x1, y1, 高]；Q1 TOLL 按 Gerber 焊盘外沿量，其余按位置和封装尺寸算
const H = [
  ['Q1', 16.9, 5.7, 30.6, 16.7, 2.4], ['C4', 8.4, 13.85, 11.6, 16.35, 2.7], ['R1', 8.85, 9.9, 15.15, 13.1, 0.7],
  ['D1', 1.2, 5.9, 3.8, 11.1, 2.4], ['D2', 31.7, 5.9, 34.3, 11.1, 2.4], ['U1', 9.75, 2.75, 12.75, 7.65, 1.1],
  ['Q2', 16.1, 0.95, 18.5, 3.85, 1.1], ['D3', 13.6, 1.75, 15.4, 3.05, 1.0],
];
for (const [d, x, y, rot] of [['C2', 8.6, 3.0, 0], ['C3', 15.55, 0.95, 0], ['R2', 8.8, 8.8, 0], ['R3', 8.6, 1.9, 0], ['R4', 28.6, 5.6, 1], ['R5', 19.8, 3.63, 0],
  ['R6', 27.6, 1.0, 0], ['R7', 6.2, 7.0, 1], ['R8', 19.0, 1.0, 0], ['LED1', 24.8, 1.0, 0], ['LED2', 6.2, 4.6, 1]]) {
  const [hx, hy] = rot ? [0.45, 0.85] : [0.85, 0.45]; H.push([d, x - hx, y - hy, x + hx, y + hy, 0.9]);
}
const bank = [['PCB', box([0, 0, 0], [P.W, P.H, P.T])]];
for (const [d, a, b, c2, e, h] of H) bank.push([d, box([a, b, P.T], [c2, e, P.T + h])]);
for (const [x, y] of PADS) {
  bank.push(['焊线芯和底面焊点', makeCylinder(1.6, 1.0, [x, y, -1.0])]);
  bank.push(['焊线', makeCylinder(P.wire.od / 2, zWire - P.T, [x, y, P.T])]);
  const outward = x < P.W / 2 ? -1 : 1, len = (outward < 0 ? x : P.W - x) + 8;
  bank.push(['焊线', makeCylinder(P.wire.od / 2, len, [x, y, zWire], [outward, 0, 0])]);
}
const hits = {};
for (const [pn, piece] of [['底壳', lower], ['上盖', upper]]) for (const [n, s] of bank) {
  let v = 0; try { v = measureVolume(piece.intersect(s)); } catch (e) { v = 0; }
  if (v > 1e-4) { const k = `${pn} × ${n}`; hits[k] = +((hits[k] || 0) + v).toFixed(3); }
}
console.log('与板子重叠（mm³）：', JSON.stringify(hits));
let ov = 0; try { ov = measureVolume(lower.intersect(upper)); } catch (e) { ov = -1; }
console.log('底壳与上盖重叠（mm³）：', +ov.toFixed(4));
if (process.env.PREVIEW) { const { makeCompound } = await import('replicad'); writeFileSync(`${OUT}/board.stl`, Buffer.from(await makeCompound(bank.map((b) => b[1])).blobSTL({ tolerance: 0.02, binary: true }).arrayBuffer())); }
