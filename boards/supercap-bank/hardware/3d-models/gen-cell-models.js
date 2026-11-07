// 单体 3D 模型（单个零件 STEP，单位 mm，原点在封装原点、z = 0 为板面，单体朝 +z）：
// 34 F 立放 Ø12.9 × 49；10 F 立放 Ø10.5 × 31.5；10 F 卧放：轴线沿 −y，从 y = −3.45 到 −34.95，轴高 6.75（垫高 1.5），
// 两根 Ø0.6 引脚从 (±2.5, 0) 竖直上到轴高再水平接到端面。尺寸取 Eaton TV 系列规格书最大值。
const fs = require('fs');
const f = (n) => { const s = (+n.toFixed(6)).toString(); return s.includes('.') || s.includes('e') ? s.replace('e', 'E') : s + '.'; };
const pt = (p) => `(${p.map(f).join(',')})`;
function model(name, cyls) {
  const L = []; let id = 1; const add = (s) => { L.push(`#${id}=${s};`); return id++; };
  add(`APPLICATION_PROTOCOL_DEFINITION('international standard','automotive_design',2000,#2)`);
  add(`APPLICATION_CONTEXT('core data for automotive mechanical design processes')`);
  const u1 = add('( LENGTH_UNIT() NAMED_UNIT(*) SI_UNIT(.MILLI.,.METRE.) )'), u2 = add('( NAMED_UNIT(*) PLANE_ANGLE_UNIT() SI_UNIT($,.RADIAN.) )'), u3 = add('( NAMED_UNIT(*) SI_UNIT($,.STERADIAN.) SOLID_ANGLE_UNIT() )');
  const u4 = add(`UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-07),#${u1},'distance_accuracy_value','confusion accuracy')`);
  const ctx = add(`( GEOMETRIC_REPRESENTATION_CONTEXT(3) GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#${u4})) GLOBAL_UNIT_ASSIGNED_CONTEXT((#${u1},#${u2},#${u3})) REPRESENTATION_CONTEXT('Context #1','3D Context with UNIT and UNCERTAINTY') )`);
  const P3 = (p) => add(`CARTESIAN_POINT('',${pt(p)})`), D3 = (d) => add(`DIRECTION('',${pt(d)})`);
  const axis = (p, z, x) => add(`AXIS2_PLACEMENT_3D('',#${P3(p)},#${D3(z)},#${D3(x)})`);
  const origin = axis([0, 0, 0], [0, 0, 1], [1, 0, 0]);
  const oe = (e, s) => add(`ORIENTED_EDGE('',*,*,#${e},${s ? '.T.' : '.F.'})`);
  const face = (edges, surf) => { const loop = add(`EDGE_LOOP('',(${edges.map((x) => '#' + x).join(',')}))`); const b = add(`FACE_OUTER_BOUND('',#${loop},.T.)`); return add(`ADVANCED_FACE('',(#${b}),#${surf},.T.)`); };
  const solids = [], styles = [], cc = new Map();
  const style = (rgb) => { const k = rgb.join(','); if (cc.has(k)) return cc.get(k); const col = add(`COLOUR_RGB('',${rgb.map(f).join(',')})`), a = add(`FILL_AREA_STYLE_COLOUR('',#${col})`), b = add(`FILL_AREA_STYLE('',(#${a}))`), c = add(`SURFACE_STYLE_FILL_AREA(#${b})`), d = add(`SURFACE_SIDE_STYLE('',(#${c}))`), e = add(`SURFACE_STYLE_USAGE(.BOTH.,#${d})`), p = add(`PRESENTATION_STYLE_ASSIGNMENT((#${e}))`); cc.set(k, p); return p; };
  for (const [c, A, R, r, h, rgb] of cyls) {
    const P1 = c.map((v, i) => v + h * A[i]), Q0 = c.map((v, i) => v + r * R[i]), Q1 = P1.map((v, i) => v + r * R[i]);
    const ax0 = axis(c, A, R), ax1 = axis(P1, A, R), axb = axis(c, A.map((v) => -v), R);
    const c0 = add(`CIRCLE('',#${ax0},${f(r)})`), c1 = add(`CIRCLE('',#${ax1},${f(r)})`);
    const v0 = add(`VERTEX_POINT('',#${P3(Q0)})`), v1 = add(`VERTEX_POINT('',#${P3(Q1)})`);
    const ln = add(`LINE('',#${P3(Q0)},#${add(`VECTOR('',#${D3(A)},1.)`)})`);
    const e0 = add(`EDGE_CURVE('',#${v0},#${v0},#${c0},.T.)`), e1 = add(`EDGE_CURVE('',#${v1},#${v1},#${c1},.T.)`), es = add(`EDGE_CURVE('',#${v0},#${v1},#${ln},.T.)`);
    const fc = face([oe(e0, true), oe(es, true), oe(e1, false), oe(es, false)], add(`CYLINDRICAL_SURFACE('',#${ax0},${f(r)})`));
    const fb = face([oe(e0, false)], add(`PLANE('',#${axb})`)), ft = face([oe(e1, true)], add(`PLANE('',#${ax1})`));
    const so = add(`MANIFOLD_SOLID_BREP('',#${add(`CLOSED_SHELL('',(#${fc},#${fb},#${ft}))`)})`); solids.push(so); styles.push(add(`STYLED_ITEM('color',(#${style(rgb)}),#${so})`));
  }
  const rep = add(`ADVANCED_BREP_SHAPE_REPRESENTATION('',(#${origin},${solids.map((x) => '#' + x).join(',')}),#${ctx})`);
  add(`MECHANICAL_DESIGN_GEOMETRIC_PRESENTATION_REPRESENTATION('',(${styles.map((x) => '#' + x).join(',')}),#${ctx})`);
  const pc = add(`PRODUCT_CONTEXT('',#2,'mechanical')`), prod = add(`PRODUCT('${name}','${name}','',(#${pc}))`);
  add(`PRODUCT_RELATED_PRODUCT_CATEGORY('part',$,(#${prod}))`);
  const form = add(`PRODUCT_DEFINITION_FORMATION('','',#${prod})`), pdc = add(`PRODUCT_DEFINITION_CONTEXT('part definition',#2,'design')`);
  const def = add(`PRODUCT_DEFINITION('design','',#${form},#${pdc})`), pds = add(`PRODUCT_DEFINITION_SHAPE('','',#${def})`);
  add(`SHAPE_DEFINITION_REPRESENTATION(#${pds},#${rep})`);
  return `ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('${name}'),'2;1');\nFILE_NAME('${name}.step','2026-11-01T00:00:00',(''),(''),'cellmodels.js','cellmodels.js','');\nFILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));\nENDSEC;\nDATA;\n${L.join('\n')}\nENDSEC;\nEND-ISO-10303-21;\n`;
}
const SLEEVE34 = [0.216, 0.314, 0.42], SLEEVE10 = [0.31, 0.541, 0.545], LEAD = [0.78, 0.8, 0.82];
const out = {
  'TV1245-3R0346-R_D12.9xL49': [[[0, 0, 0], [0, 0, 1], [1, 0, 0], 6.45, 49, SLEEVE34]],
  'TV1030-3R0106-R_D10.5xL31.5': [[[0, 0, 0], [0, 0, 1], [1, 0, 0], 5.25, 31.5, SLEEVE10]],
  'TV1030-3R0106-R_D10.5xL31.5_lying': [
    [[0, -3.45, 6.75], [0, -1, 0], [1, 0, 0], 5.25, 31.5, SLEEVE10],
    ...[-2.5, 2.5].flatMap((x) => [[[x, 0, -1.6], [0, 0, 1], [1, 0, 0], 0.3, 1.6 + 6.75, LEAD], [[x, 0, 6.75], [0, -1, 0], [1, 0, 0], 0.3, 3.45, LEAD]]),
  ],
};
for (const [n, c] of Object.entries(out)) { fs.writeFileSync(__dirname + '/' + n + '.step', model(n, c)); console.log(n, fs.statSync(__dirname + '/' + n + '.step').size); }
