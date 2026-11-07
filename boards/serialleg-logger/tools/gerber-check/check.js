// 制造检查：node check.js <gerber 目录> <输出 json>
'use strict';
const fs = require('fs'); const path = require('path');
const { parseGerber, Raster, render, edt, parseDrill } = require('./gerber.js');
const D = process.argv[2]; const f = (n) => path.join(D, n);
const res = 0.01;
const out = {};
// 1. 板框：把 GKO 的线段串成多边形
const ko = parseGerber(f('Gerber_BoardOutlineLayer.GKO')).ops.filter((o) => o.k === 'stroke');
const segs = ko.map((o) => [[o.x1, o.y1], [o.x2, o.y2]]);
const near = (p, q) => Math.hypot(p[0] - q[0], p[1] - q[1]) < 1e-3;
const poly = [segs[0][0], segs[0][1]]; const used = new Set([0]);
for (let guard = 0; guard < segs.length; guard++) {
  const last = poly[poly.length - 1]; let found = false;
  for (let k = 0; k < segs.length; k++) {
    if (used.has(k)) continue; const [a, b] = segs[k];
    if (near(a, last)) { poly.push(b); used.add(k); found = true; break; }
    if (near(b, last)) { poly.push(a); used.add(k); found = true; break; }
  }
  if (!found) break;
}
const closed = near(poly[0], poly[poly.length - 1]);
let area = 0; for (let i = 0; i + 1 < poly.length; i++) area += poly[i][0] * poly[i + 1][1] - poly[i + 1][0] * poly[i][1];
const xs = poly.map((p) => p[0]), ys = poly.map((p) => p[1]);
const bb = [Math.min(...xs), Math.min(...ys), Math.max(...xs), Math.max(...ys)];
out.outline = { segments: segs.length, used: used.size, closed, vertices: poly.length - 1, bbox_mm: [+(bb[2] - bb[0]).toFixed(3), +(bb[3] - bb[1]).toFixed(3)], area_mm2: +(Math.abs(area) / 2).toFixed(1) };
const X0 = bb[0] - 2, Y0 = bb[1] - 2, X1 = bb[2] + 2, Y1 = bb[3] + 2;
const mk = () => new Raster(X0, Y0, X1, Y1, res);
const inside = mk(); inside.poly(poly.slice(0, -1), 1);
const W = inside.W, H = inside.H, N = W * H;
const toMM = (k) => [+(X0 + (k % W + 0.5) * res).toFixed(3), +(Y0 + (Math.floor(k / W) + 0.5) * res).toFixed(3)];
const toMil = (p) => [+(p[0] / 0.0254).toFixed(1), +(p[1] / 0.0254).toFixed(1)];
const outsideMask = new Uint8Array(N); for (let k = 0; k < N; k++) outsideMask[k] = inside.a[k] ? 0 : 1;
const dOut = edt(outsideMask, W, H);
// 2. 铜到板边
const layers = { top: 'Gerber_TopLayer.GTL', bottom: 'Gerber_BottomLayer.GBL', inner1: 'Gerber_InnerLayer1.G1', inner2: 'Gerber_InnerLayer2.G2' };
out.copper = {};
for (const [name, file] of Object.entries(layers)) {
  const R = mk(); render(f(file), R);
  let min = Infinity, at = -1, outside = 0;
  for (let k = 0; k < N; k++) if (R.a[k]) { if (!inside.a[k]) { outside++; continue; } if (dOut[k] < min) { min = dOut[k]; at = k; } }
  out.copper[name] = { min_to_edge_mm: +(min * res).toFixed(3), at_mil: toMil(toMM(at)), pixels_outside: outside };
}
// 3. 丝印与阻焊
const sides = { top: ['Gerber_TopSolderMaskLayer.GTS', 'Gerber_TopSilkscreenLayer.GTO'], bottom: ['Gerber_BottomSolderMaskLayer.GBS', 'Gerber_BottomSilkscreenLayer.GBO'] };
out.silk = {}; out.maskDam = {};
const blobsOf = (sel, maxN) => {
  const seen = new Uint8Array(N); const res2 = [];
  for (let k = 0; k < N; k++) {
    if (!sel(k) || seen[k]) continue; const q = [k]; seen[k] = 1; let n = 0; let bx = [1e9, 1e9, -1e9, -1e9];
    while (q.length) { const c = q.pop(); n++; const i = c % W, j = (c - i) / W; bx = [Math.min(bx[0], i), Math.min(bx[1], j), Math.max(bx[2], i), Math.max(bx[3], j)];
      for (const kk of [c + 1, c - 1, c + W, c - W]) if (kk >= 0 && kk < N && !seen[kk] && sel(kk)) { seen[kk] = 1; q.push(kk); } }
    res2.push({ px: n, from_mil: toMil(toMM(bx[1] * W + bx[0])), to_mil: toMil(toMM(bx[3] * W + bx[2])) });
  }
  res2.sort((a, b) => b.px - a.px); return { count: res2.length, list: res2.slice(0, maxN) };
};
for (const [side, [mf, sf]] of Object.entries(sides)) {
  const M = mk(); render(f(mf), M); const S = mk(); render(f(sf), S);
  const dM = edt(M.a, W, H);
  let min = Infinity, at = -1, overlap = 0, offBoard = 0;
  for (let k = 0; k < N; k++) if (S.a[k]) { if (!inside.a[k]) { offBoard++; continue; } if (dM[k] === 0) overlap++; else if (dM[k] < min) { min = dM[k]; at = k; } }
  const off = blobsOf((k) => S.a[k] && !inside.a[k], 10);
  const ov = blobsOf((k) => S.a[k] && inside.a[k] && dM[k] === 0, 10);
  out.silk[side] = { min_to_mask_opening_mm: +(min * res).toFixed(3), at_mil: at >= 0 ? toMil(toMM(at)) : null, overlap_pixels: overlap, overlap_blobs: ov, off_board_pixels: offBoard, off_board_blobs: off };
  // 阻焊桥：板内阻焊材料里窄于 0.1 mm 的部分（半径 0.05 mm 开运算的残余），排除板边附近
  const r = 0.05 / res;
  const mat = new Uint8Array(N); for (let k = 0; k < N; k++) mat[k] = inside.a[k] && !M.a[k] ? 1 : 0;
  const eroded = new Uint8Array(N); for (let k = 0; k < N; k++) eroded[k] = mat[k] && dM[k] > r ? 1 : 0;
  const dEr = edt(eroded, W, H);
  const thin = blobsOf((k) => mat[k] && dEr[k] > r + 0.5 && dOut[k] > 0.15 / res, 12);
  out.maskDam[side] = { narrower_than_0_1mm_regions: thin.count, largest: thin.list };
}
// 4. J6 焊盘：铜与阻焊开窗尺寸
const pick = (file) => parseGerber(f(file)).ops.filter((o) => o.k === 'flash' && Math.abs(o.x - 3724.5 * 0.0254) < 0.05).map((o) => ({ y_mil: +(o.y / 0.0254).toFixed(1), ap: o.ap.type + ' ' + o.ap.p.join('x') }));
out.j6 = { copper: pick('Gerber_TopLayer.GTL'), mask: pick('Gerber_TopSolderMaskLayer.GTS'), paste: pick('Gerber_TopPasteMaskLayer.GTP') };
// 5. 钻孔
const segD = (p, a, b) => { const dx = b[0] - a[0], dy = b[1] - a[1], L = dx * dx + dy * dy || 1; let s = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / L; s = Math.max(0, Math.min(1, s)); return Math.hypot(p[0] - a[0] - s * dx, p[1] - a[1] - s * dy); };
const P = poly.slice(0, -1);
const inPoly = (p) => { let c = false; for (let i = 0, j = P.length - 1; i < P.length; j = i++) { const [xi, yi] = P[i], [xj, yj] = P[j]; if (((yi > p[1]) !== (yj > p[1])) && (p[0] < (xj - xi) * (p[1] - yi) / (yj - yi) + xi)) c = !c; } return c; };
out.drill = {};
for (const fn of fs.readdirSync(D).filter((n) => /\.DRL$/i.test(n))) {
  const d = parseDrill(f(fn)); const sizes = {};
  let minEdge = Infinity, minAt = null, outsideHoles = 0;
  for (const h of d.hits) {
    sizes[h.d] = (sizes[h.d] || 0) + 1; const p = [h.x, h.y]; if (!inPoly(p)) outsideHoles++;
    let e = Infinity; for (let i = 0; i + 1 < poly.length; i++) e = Math.min(e, segD(p, poly[i], poly[i + 1])); e -= h.d / 2;
    if (e < minEdge) { minEdge = e; minAt = toMil(p); }
  }
  out.drill[fn] = { holes: d.hits.length, sizes_mm: sizes, min_hole_mm: Math.min(...Object.keys(sizes).map(Number)), min_hole_wall_to_edge_mm: +minEdge.toFixed(3), at_mil: minAt, outside_board: outsideHoles };
}
fs.writeFileSync(process.argv[3], JSON.stringify(out, null, 1));
console.log(JSON.stringify(out, null, 1));
