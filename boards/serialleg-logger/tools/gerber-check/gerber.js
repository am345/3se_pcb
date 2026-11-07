// 简易 RS-274X 解析与栅格化（嘉立创专业版导出格式）：圆/矩形/宏光圈、直线与圆弧、区域、极性
'use strict';
const fs = require('fs');

function evalExpr(s, vars) {
  const e = s.replace(/\$(\d+)/g, (_, n) => '(' + String(vars[+n] ?? 0) + ')').replace(/x/gi, '*');
  if (!/^[-+*/().\d\s]*$/.test(e)) throw new Error('宏表达式不支持: ' + s);
  return Function('return (' + e + ')')();
}

function parseGerber(file) {
  const t = fs.readFileSync(file, 'latin1');
  let scale = 1e-5, unit = 1; // FSLAX45 → 5 位小数
  const aps = {}, macros = {};
  const ops = [];
  let x = 0, y = 0, cur = null, interp = 1, region = false, pol = 1, contour = null;
  const blocks = [];
  // 拆分：% 扩展块整体保留，其余按 * 拆分
  for (let i = 0; i < t.length;) {
    if (/\s/.test(t[i])) { i++; continue; }
    if (t[i] === '%') { const j = t.indexOf('%', i + 1); if (j < 0) break; blocks.push({ ext: t.slice(i + 1, j) }); i = j + 1; continue; }
    const j = t.indexOf('*', i); if (j < 0) break; const s = t.slice(i, j).trim(); if (s) blocks.push({ cmd: s }); i = j + 1;
  }
  for (const b of blocks) {
    if (b.ext !== undefined) {
      const e = b.ext.replace(/\s+/g, '');
      let m;
      if ((m = e.match(/^FSLAX(\d)(\d)Y\d\d\*$/))) scale = Math.pow(10, -(+m[2]));
      else if (/^MOIN\*$/.test(e)) unit = 25.4;
      else if (/^MOMM\*$/.test(e)) unit = 1;
      else if ((m = e.match(/^LP([DC])\*$/))) pol = m[1] === 'D' ? 1 : 0;
      else if ((m = e.match(/^ADD(\d+)([A-Za-z_][\w.]*),?(.*)\*$/))) {
        const params = m[3] ? m[3].split('X').map(Number) : [];
        aps[m[1]] = { type: m[2], p: params };
      } else if ((m = e.match(/^AM([\w.]+)\*(.*)$/))) {
        macros[m[1]] = m[2].split('*').filter((s) => s.length).map((s) => s.split(','));
      }
      continue;
    }
    let s = b.cmd;
    if (/^G04/.test(s)) continue;
    let m;
    if ((m = s.match(/^G0?([123])(?!\d)/))) { interp = +m[1]; s = s.slice(m[0].length); }
    if (s === 'G36') { region = true; contour = null; continue; }
    if (s === 'G37') { if (contour && contour.length > 2) ops.push({ k: 'region', pts: contour, pol }); contour = null; region = false; continue; }
    if (/^G7[45]$/.test(s) || /^M0?2$/.test(s) || s === '') continue;
    if ((m = s.match(/^D(\d+)$/)) && +m[1] >= 10) { cur = m[1]; continue; }
    if ((m = s.match(/^G54D(\d+)$/))) { cur = m[1]; continue; }
    m = s.match(/^(?:X(-?\d+))?(?:Y(-?\d+))?(?:I(-?\d+))?(?:J(-?\d+))?D0?([123])$/);
    if (!m) continue;
    const nx = m[1] !== undefined ? +m[1] * scale * unit : x, ny = m[2] !== undefined ? +m[2] * scale * unit : y;
    const ii = m[3] !== undefined ? +m[3] * scale * unit : 0, jj = m[4] !== undefined ? +m[4] * scale * unit : 0;
    const d = +m[5];
    const pts = interp === 1 ? [[nx, ny]] : arcPts(x, y, nx, ny, x + ii, y + jj, interp === 2);
    if (region) {
      if (d === 2) { if (contour && contour.length > 2) ops.push({ k: 'region', pts: contour, pol }); contour = [[nx, ny]]; }
      else if (d === 1) { if (!contour) contour = [[x, y]]; contour.push(...pts); }
    } else if (d === 1) {
      let px = x, py = y; for (const q of pts) { ops.push({ k: 'stroke', ap: aps[cur], x1: px, y1: py, x2: q[0], y2: q[1], pol }); px = q[0]; py = q[1]; }
    } else if (d === 3) ops.push({ k: 'flash', ap: aps[cur], x: nx, y: ny, pol });
    x = nx; y = ny;
  }
  return { ops, aps, macros };
}

function arcPts(x1, y1, x2, y2, cx, cy, cw) {
  const r = Math.hypot(x1 - cx, y1 - cy);
  let a1 = Math.atan2(y1 - cy, x1 - cx), a2 = Math.atan2(y2 - cy, x2 - cx);
  if (cw) { while (a2 >= a1) a2 -= 2 * Math.PI; } else { while (a2 <= a1) a2 += 2 * Math.PI; }
  if (Math.abs(x1 - x2) < 1e-9 && Math.abs(y1 - y2) < 1e-9) a2 = a1 + (cw ? -2 : 2) * Math.PI;
  const n = Math.max(4, Math.ceil(Math.abs(a2 - a1) / (Math.PI / 36)));
  const out = []; for (let k = 1; k <= n; k++) { const a = a1 + (a2 - a1) * k / n; out.push([cx + r * Math.cos(a), cy + r * Math.sin(a)]); }
  return out;
}

// 栅格：像素尺寸 res（mm），范围 [x0,x1]×[y0,y1]
class Raster {
  constructor(x0, y0, x1, y1, res) { this.x0 = x0; this.y0 = y0; this.res = res; this.W = Math.ceil((x1 - x0) / res); this.H = Math.ceil((y1 - y0) / res); this.a = new Uint8Array(this.W * this.H); }
  px(x) { return (x - this.x0) / this.res; } py(y) { return (y - this.y0) / this.res; }
  set(i, j, v) { if (i >= 0 && j >= 0 && i < this.W && j < this.H) this.a[j * this.W + i] = v; }
  disk(x, y, r, v) { const ci = this.px(x), cj = this.py(y), rr = r / this.res; for (let j = Math.floor(cj - rr); j <= Math.ceil(cj + rr); j++) for (let i = Math.floor(ci - rr); i <= Math.ceil(ci + rr); i++) { const dx = i + 0.5 - ci, dy = j + 0.5 - cj; if (dx * dx + dy * dy <= rr * rr) this.set(i, j, v); } }
  capsule(x1, y1, x2, y2, r, v) {
    const a = [this.px(x1), this.py(y1)], b = [this.px(x2), this.py(y2)], rr = r / this.res;
    const minI = Math.floor(Math.min(a[0], b[0]) - rr), maxI = Math.ceil(Math.max(a[0], b[0]) + rr), minJ = Math.floor(Math.min(a[1], b[1]) - rr), maxJ = Math.ceil(Math.max(a[1], b[1]) + rr);
    const dx = b[0] - a[0], dy = b[1] - a[1], L = dx * dx + dy * dy || 1e-12;
    for (let j = minJ; j <= maxJ; j++) for (let i = minI; i <= maxI; i++) { const qx = i + 0.5, qy = j + 0.5; let s = ((qx - a[0]) * dx + (qy - a[1]) * dy) / L; s = Math.max(0, Math.min(1, s)); const ex = qx - a[0] - s * dx, ey = qy - a[1] - s * dy; if (ex * ex + ey * ey <= rr * rr) this.set(i, j, v); }
  }
  poly(pts, v) { // 偶奇填充
    const P = pts.map((p) => [this.px(p[0]), this.py(p[1])]);
    let minJ = Infinity, maxJ = -Infinity; for (const p of P) { minJ = Math.min(minJ, p[1]); maxJ = Math.max(maxJ, p[1]); }
    for (let j = Math.max(0, Math.floor(minJ)); j <= Math.min(this.H - 1, Math.ceil(maxJ)); j++) {
      const yc = j + 0.5, xs = [];
      for (let k = 0; k < P.length; k++) { const p = P[k], q = P[(k + 1) % P.length]; if ((p[1] > yc) !== (q[1] > yc)) xs.push(p[0] + (yc - p[1]) * (q[0] - p[0]) / (q[1] - p[1])); }
      xs.sort((u, w) => u - w);
      for (let k = 0; k + 1 < xs.length; k += 2) for (let i = Math.max(0, Math.ceil(xs[k] - 0.5)); i <= Math.min(this.W - 1, Math.floor(xs[k + 1] - 0.5)); i++) this.a[j * this.W + i] = v;
    }
  }
}

function drawFlash(R, ap, x, y, v, macros) {
  if (!ap) return;
  if (ap.type === 'C') return R.disk(x, y, ap.p[0] / 2, v);
  if (ap.type === 'R') { const w = ap.p[0] / 2, h = ap.p[1] / 2; return R.poly([[x - w, y - h], [x + w, y - h], [x + w, y + h], [x - w, y + h]], v); }
  if (ap.type === 'O') { const w = ap.p[0] / 2, h = ap.p[1] / 2; if (w > h) return R.capsule(x - w + h, y, x + w - h, y, h, v); return R.capsule(x, y - h + w, x, y + h - w, w, v); }
  const mac = macros[ap.type]; if (!mac) throw new Error('未知光圈 ' + ap.type);
  const vars = [0, ...ap.p];
  for (const prim of mac) {
    const code = +prim[0]; const a = prim.slice(1).map((s) => evalExpr(s, vars));
    const pv = a[0] ? v : 1 - v;
    if (code === 1) R.disk(x + a[2], y + a[3], a[1] / 2, pv);
    else if (code === 20) R.capsule(x + a[2], y + a[3], x + a[4], y + a[5], a[1] / 2, pv) /* 宏中 20 号为方头线，这里按圆头近似，宽度很小时差别可忽略 */;
    else if (code === 4) { const n = a[1]; const pts = []; for (let k = 0; k < n; k++) pts.push([x + a[2 + 2 * k], y + a[3 + 2 * k]]); R.poly(pts, pv); }
    else if (code === 21) { const w = a[1] / 2, h = a[2] / 2, cx = x + a[3], cy = y + a[4]; R.poly([[cx - w, cy - h], [cx + w, cy - h], [cx + w, cy + h], [cx - w, cy + h]], pv); }
  }
}

function render(file, R) {
  const g = parseGerber(file);
  for (const o of g.ops) {
    if (o.k === 'region') R.poly(o.pts, o.pol);
    else if (o.k === 'flash') drawFlash(R, o.ap, o.x, o.y, o.pol, g.macros);
    else if (o.k === 'stroke') { const w = o.ap ? (o.ap.type === 'C' ? o.ap.p[0] : Math.min(...o.ap.p)) : 0.1; R.capsule(o.x1, o.y1, o.x2, o.y2, w / 2, o.pol); }
  }
  return g;
}

// 欧氏距离变换（Felzenszwalb），返回到最近“特征像素”的距离（像素）
function edt(mask, W, H) {
  const INF = 1e20, f = new Float64Array(Math.max(W, H)), d = new Float64Array(W * H);
  const v = new Int32Array(Math.max(W, H)), z = new Float64Array(Math.max(W, H) + 1), out = new Float64Array(Math.max(W, H));
  const pass = (len, get, put) => {
    for (let q = 0; q < len; q++) f[q] = get(q);
    let k = 0; v[0] = 0; z[0] = -INF; z[1] = INF;
    for (let q = 1; q < len; q++) { let s; do { const p = v[k]; s = ((f[q] + q * q) - (f[p] + p * p)) / (2 * q - 2 * p); if (s <= z[k]) k--; else break; } while (k >= 0); k++; v[k] = q; z[k] = s; z[k + 1] = INF; }
    k = 0; for (let q = 0; q < len; q++) { while (z[k + 1] < q) k++; const p = v[k]; out[q] = (q - p) * (q - p) + f[p]; }
    for (let q = 0; q < len; q++) put(q, out[q]);
  };
  for (let i = 0; i < W; i++) pass(H, (j) => (mask[j * W + i] ? 0 : INF), (j, val) => { d[j * W + i] = val; });
  for (let j = 0; j < H; j++) pass(W, (i) => d[j * W + i], (i, val) => { d[j * W + i] = Math.sqrt(val); });
  return d;
}

function parseDrill(file) {
  const t = fs.readFileSync(file, 'latin1').split(/\r?\n/);
  const tools = {}; let cur = null; const hits = [];
  for (const l of t) {
    let m;
    if ((m = l.match(/^T(\d+)C([\d.]+)/))) tools[m[1]] = +m[2];
    else if ((m = l.match(/^T(\d+)$/))) cur = m[1];
    else if ((m = l.match(/^X(-?[\d.]+)Y(-?[\d.]+)G85X(-?[\d.]+)Y(-?[\d.]+)/))) hits.push({ d: tools[cur], x: +m[1], y: +m[2], slot: [+m[3], +m[4]] });
    else if ((m = l.match(/^X(-?[\d.]+)Y(-?[\d.]+)/))) hits.push({ d: tools[cur], x: +m[1], y: +m[2] });
  }
  return { tools, hits };
}

module.exports = { parseGerber, Raster, render, edt, parseDrill };
