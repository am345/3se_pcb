// 把若干 Gerber 层画成 SVG（区域、闪光、线段），node svgview.js out.svg x0 y0 x1 y1(mil) 层文件:颜色 ...
const fs = require('fs'); const { parseGerber } = require('./gerber.js');
const [outF, x0, y0, x1, y1, ...specs] = process.argv.slice(2);
const X0 = +x0 * 0.0254, Y0 = +y0 * 0.0254, X1 = +x1 * 0.0254, Y1 = +y1 * 0.0254; const S = 900 / (X1 - X0);
const px = (x) => ((x - X0) * S).toFixed(2), py = (y) => ((Y1 - y) * S).toFixed(2);
const o = [`<svg xmlns="http://www.w3.org/2000/svg" width="900" height="${((Y1 - Y0) * S).toFixed(0)}" style="background:#111">`];
for (const spec of specs) {
  const [file, color, op] = spec.split('|'); const g = parseGerber(file);
  o.push(`<g fill="${color}" stroke="${color}" opacity="${op || 0.8}">`);
  for (const e of g.ops) {
    if (e.k === 'region') o.push(`<path d="M${e.pts.map((p) => px(p[0]) + ',' + py(p[1])).join(' L')} Z" stroke="none" fill="${e.pol ? color : '#111'}"/>`);
    else if (e.k === 'stroke') { const w = e.ap ? (e.ap.type === 'C' ? e.ap.p[0] : Math.min(...e.ap.p)) : 0.1; o.push(`<line x1="${px(e.x1)}" y1="${py(e.y1)}" x2="${px(e.x2)}" y2="${py(e.y2)}" stroke-width="${(w * S).toFixed(2)}" stroke-linecap="round" fill="none"/>`); }
    else if (e.k === 'flash' && e.ap) { if (e.ap.type === 'C') o.push(`<circle cx="${px(e.x)}" cy="${py(e.y)}" r="${(e.ap.p[0] / 2 * S).toFixed(2)}" stroke="none"/>`); else if (e.ap.type === 'R') o.push(`<rect x="${(+px(e.x) - e.ap.p[0] / 2 * S).toFixed(2)}" y="${(+py(e.y) - e.ap.p[1] / 2 * S).toFixed(2)}" width="${(e.ap.p[0] * S).toFixed(2)}" height="${(e.ap.p[1] * S).toFixed(2)}" stroke="none"/>`); else o.push(`<circle cx="${px(e.x)}" cy="${py(e.y)}" r="${(0.4 * S).toFixed(2)}" stroke="none"/>`); }
  }
  o.push('</g>');
}
o.push('</svg>'); fs.writeFileSync(outF, o.join('\n'));
fs.writeFileSync(outF.replace(/\.svg$/, '.html'), `<!doctype html><meta charset="utf-8"><title>g</title><body style="margin:0;background:#000"><img src="${require('path').basename(outF)}" style="max-width:100%;max-height:100vh">`);
