// 列出丝印到阻焊开窗 < 0.15 mm 的所有位置（按连通块）
const { parseGerber, Raster, render, edt } = require('./gerber.js');
const G = process.argv[2]; const res = 0.01;
const X0 = 70.0, Y0 = -62.0, X1 = 110.0, Y1 = -18.5;
const mk = () => new Raster(X0, Y0, X1, Y1, res);
for (const [side, mf, sf] of [['top', 'Gerber_TopSolderMaskLayer.GTS', 'Gerber_TopSilkscreenLayer.GTO'], ['bottom', 'Gerber_BottomSolderMaskLayer.GBS', 'Gerber_BottomSilkscreenLayer.GBO']]) {
  const M = mk(); render(G + '/' + mf, M); const S = mk(); render(G + '/' + sf, S);
  const W = M.W, N = W * M.H; const d = edt(M.a, W, M.H);
  const lim = 0.15 / res; const seen = new Uint8Array(N); const blobs = [];
  for (let k = 0; k < N; k++) { if (!S.a[k] || d[k] >= lim || seen[k]) continue; const q = [k]; seen[k] = 1; let n = 0, mn = Infinity, bx = [1e9, 1e9, -1e9, -1e9];
    while (q.length) { const c = q.pop(); n++; mn = Math.min(mn, d[c]); const i = c % W, j = (c - i) / W; bx = [Math.min(bx[0], i), Math.min(bx[1], j), Math.max(bx[2], i), Math.max(bx[3], j)];
      for (const kk of [c + 1, c - 1, c + W, c - W]) if (kk >= 0 && kk < N && S.a[kk] && d[kk] < lim && !seen[kk]) { seen[kk] = 1; q.push(kk); } }
    const mil = (i, j) => [+((X0 + (i + 0.5) * res) / 0.0254).toFixed(0), +((Y0 + (j + 0.5) * res) / 0.0254).toFixed(0)];
    blobs.push({ min_mm: +(mn * res).toFixed(3), from: mil(bx[0], bx[1]), to: mil(bx[2], bx[3]) }); }
  blobs.sort((a, b) => a.min_mm - b.min_mm);
  console.log(side, blobs.length); for (const b of blobs) console.log('  ', JSON.stringify(b));
}
