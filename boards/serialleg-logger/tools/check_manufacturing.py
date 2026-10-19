"""检查本板原生 Gerber 的指定制造几何；不替代完整 DFM 或板厂 CAM。"""
import sys,re,math,json,zipfile
from pathlib import Path

from shapely.geometry import Polygon,Point,LineString,box
from shapely.ops import unary_union
from shapely import STRtree
if len(sys.argv)!=2:raise SystemExit('用法：python check_manufacturing.py <原生导出目录>')
ROOT=Path(sys.argv[1]).resolve()
G=ROOT/'gerber'
G.mkdir(exist_ok=True)
with zipfile.ZipFile(ROOT/'gerber.zip') as z:z.extractall(G)
def parse(name):
    # 仅接受本次原生导出的毫米 Gerber；未知指令必须报错。
    aps={};ap=None;pos=(0,0);mode=1;region=False;cont=[];polys=[];polarity=True;draws=[]
    def add(g):
        if not g.is_valid:g=g.buffer(0)
        if not g.is_empty:draws.append((polarity,g))
    def flush():
        if len(cont)>2:add(Polygon(cont))
    for raw in (G/name).read_text().splitlines():
        l=raw.strip()
        if not l:continue
        if l.startswith('%ADD'):
            m=re.fullmatch(r'%ADD(\d+)(C|R|RoundRect),([-\d.X]+)\*%',l)
            if not m:raise ValueError(l)
            aps[int(m[1])]=(m[2],list(map(float,m[3].split('X'))));continue
        if l.startswith('%AMRoundRect*'):continue
        if l=='%LPD*%':polarity=True;continue
        if l=='%LPC*%':polarity=False;continue
        if l.startswith('G04') or l in ['%FSLAX45Y45*%','%MOMM*%','G75*','M02*']:continue
        m=re.fullmatch(r'G54D(\d+)\*',l)
        if m:ap=aps[int(m[1])];continue
        if l=='G36*':region=True;cont=[];continue
        if l=='G37*':flush();region=False;cont=[];continue
        m=re.match(r'G0([123])',l)
        if m:mode=int(m[1]);l=l[m.end():]
        data=dict(re.findall(r'([XYIJD])(-?\d+)',l))
        if not data:raise ValueError(l)
        q=(float(data.get('X',pos[0]*100000))/100000,float(data.get('Y',pos[1]*100000))/100000);op=int(data.get('D',1))
        if op==2:
            if region:flush();cont=[q]
            pos=q;continue
        if op==3:
            typ,v=ap
            if typ=='C':g=Point(q).buffer(v[0]/2,quad_segs=32)
            elif typ=='R':g=box(q[0]-v[0]/2,q[1]-v[1]/2,q[0]+v[0]/2,q[1]+v[1]/2)
            else:
                d,x1,y1,x2,y2=v
                g=Polygon([(q[0]+x1,q[1]+y1),(q[0]+x2,q[1]+y2),(q[0]-x1,q[1]-y1),(q[0]-x2,q[1]-y2)]).buffer(d/2,quad_segs=32)
            add(g);pos=q;continue
        path=[pos,q]
        if mode in [2,3]:
            cen=(pos[0]+float(data.get('I',0))/100000,pos[1]+float(data.get('J',0))/100000);a=math.atan2(pos[1]-cen[1],pos[0]-cen[0]);b=math.atan2(q[1]-cen[1],q[0]-cen[0]);rad=math.dist(pos,cen)
            da=(b-a)%(2*math.pi) if mode==3 else -((a-b)%(2*math.pi))
            if abs(da)<1e-8:da=2*math.pi*(1 if mode==3 else -1)
            n=max(8,math.ceil(abs(da)/0.02));path=[(cen[0]+rad*math.cos(a+da*i/n),cen[1]+rad*math.sin(a+da*i/n)) for i in range(n+1)]
        if region:cont.extend(path[1:])
        else:add(LineString(path).buffer(ap[1][0]/2,quad_segs=32))
        pos=q
    combined=Polygon()
    # 保留极性顺序，不能将明暗层一次性相减。
    batch=[];last=True
    for positive,g in draws:
        if positive!=last:
            u=unary_union(batch);combined=combined.union(u) if last else combined.difference(u);batch=[];last=positive
        batch.append(g)
    u=unary_union(batch);combined=combined.union(u) if last else combined.difference(u)
    return combined,draws,aps
report={'unit':'mm','arc_step_rad':0.02,'circle_segments':128,'scope':'Gerber 几何检查；不代替板厂 CAM 或实物装配','limitations':['阻焊桥仅检查合并后独立开窗之间的距离，不把同网焊盘与过孔的共同开窗判断为错误。','弧线及圆按折线近似；边界结果需结合原生 DRC 与板厂 CAM。','未自动核对阻抗、钢网厚度与开口比例、器件高度及装配方向。'],'layers':{}}
for side,maskname,silkname in [('top','Gerber_TopSolderMaskLayer.GTS','Gerber_TopSilkscreenLayer.GTO'),('bottom','Gerber_BottomSolderMaskLayer.GBS','Gerber_BottomSilkscreenLayer.GBO')]:
    mask,md,ma=parse(maskname);silk,sd,sa=parse(silkname)
    opens=list(mask.geoms) if hasattr(mask,'geoms') else [mask];tree=STRtree(opens);bridges=[]
    for i,g in enumerate(opens):
        for j in tree.query(g.buffer(.101)):
            if int(j)<=i:continue
            d=g.distance(opens[j])
            if d<.0999:bridges.append({'gap':d,'bbox1':g.bounds,'bbox2':opens[j].bounds})
    silkbad=[]
    for i,(positive,g) in enumerate(sd):
        if positive and g.distance(mask)<.1499:silkbad.append({'index':i,'clearance':g.distance(mask),'bbox':g.bounds})
    widths=sorted(set(v[1][0] for v in sa.values() if v[0]=='C'))
    report['layers'][side]={'mask_openings':len(opens),'min_silk_to_mask':silk.distance(mask),'silk_aperture_widths':widths,'silk_clearance_violations':silkbad,'mask_bridge_violations':bridges}
geom=json.loads((ROOT/'native-geometry.json').read_text(encoding='utf-8'))
ring=[]
for p in geom['pads']:
    if p.get('metallization') and p.get('hole') and p['hole'][0]=='ROUND':
        w,h=p['pad'][1:3];r=(min(w,h)-p['hole'][1])*.0254/2
        ring.append({'id':p['primitiveId'],'pad':p['padNumber'],'ring':r,'xy':[p['x']*.0254,p['y']*.0254]})
report['pth_annular_ring']={'minimum':min(x['ring'] for x in ring),'failures':[x for x in ring if x['ring']<.1499],'count':len(ring)}
report['via']={'count':len(geom['vias'])}
def drills(name):
    sizes={};active=None;out=[]
    for l in (G/name).read_text().splitlines():
        m=re.fullmatch(r'T(\d+)C([\d.]+)',l)
        if m:sizes[m[1]]=float(m[2]);continue
        m=re.fullmatch(r'T(\d+)',l)
        if m:active=sizes[m[1]];continue
        m=re.fullmatch(r'X(-?[\d.]+)Y(-?[\d.]+)',l)
        if m:out.append((float(m[1]),float(m[2]),active))
        elif l and not l.startswith(';') and l not in ['M48','METRIC,LZ,0000.00000','%','G05','G90','M30']:raise ValueError(l)
    return out
holes=drills('Drill_NPTH_Through.DRL')
clearances=[]
outline=geom['outlines'][0]['polygon']['polygon']
assert outline[2]=='L' and len(geom['outlines'])==1
numbers=outline[:2]+outline[3:]
board=Polygon([(x*.0254,y*.0254) for x,y in zip(numbers[::2],numbers[1::2])])
edge_clearances=[]
for layer in ['Gerber_TopLayer.GTL','Gerber_InnerLayer1.G1','Gerber_InnerLayer2.G2','Gerber_BottomLayer.GBL']:
    cu,_,_=parse(layer)
    # 成品铜必须扣除实际钻掉的材料；不删孔外的环形残铜。
    cu=cu.difference(unary_union([Point(x,y).buffer(d/2,quad_segs=64) for x,y,d in holes]))
    edge_clearances.append({'layer':layer,'clearance':cu.distance(board.boundary),'outside_board_mm2':cu.difference(board).area})
    for x,y,d in holes:
        clearances.append({'layer':layer,'hole':[x,y,d],'clearance':Point(x,y).buffer(d/2,quad_segs=64).distance(cu)})
report['npth_to_copper']={'minimum':min(x['clearance'] for x in clearances),'measurements':clearances,'failures':[x for x in clearances if x['clearance']<.1999]}
# 11.8 mil 为保存的板边规则；0.0001 mm 只用于输出坐标舍入容差。
report['edge_to_copper']={'minimum':min(x['clearance'] for x in edge_clearances),'measurements':edge_clearances,'failures':[x for x in edge_clearances if x['clearance']<11.8*.0254-.0001 or x['outside_board_mm2']>1e-7]}
all_pth=drills('Drill_PTH_Through.DRL');via=drills('Drill_PTH_Through_Via.DRL')
pth=[p for p in all_pth if not any(math.dist(p[:2],v[:2])<.002 and abs(p[2]-v[2])<.002 for v in via)]
report['via_annular_ring_min']=min((v['diameter']-v['holeDiameter'])*.0254/2 for v in geom['vias'])
report['silk_source']={'minimum_font_mil':min(s['fontSize'] for s in geom['strings']),'minimum_line_mil':min(s['lineWidth'] for s in geom['strings'])}
report['drills']={'pth_count':len(pth),'via_count':len(via),'npth_count':len(holes),'minimum_via_diameter':min(v[2] for v in via),'minimum_pth_pair_gap':min(math.dist(a[:2],b[:2])-(a[2]+b[2])/2 for i,a in enumerate(pth) for b in pth[i+1:])}
paste=[]
for layer in ['Gerber_TopPasteMaskLayer.GTP','Gerber_BottomPasteMaskLayer.GBP']:
    openings,_,_=parse(layer)
    for x,y,d in holes+pth:
        area=openings.intersection(Point(x,y).buffer(d/2,quad_segs=64)).area
        if area>1e-7:paste.append({'layer':layer,'hole':[x,y,d],'overlap_mm2':area})
report['paste_on_component_drills']={'scope':'NPTH 与连接器 PTH 钻孔区域不应有锡膏开口；不涵盖全部钢网规则。','failures':paste}
(ROOT/'manufacturing-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:{'mask_bridge_failures':len(v['mask_bridge_violations']),'silk_failures':len(v['silk_clearance_violations']),'widths':v['silk_aperture_widths']} for k,v in report['layers'].items()}))
print(report['pth_annular_ring'])
violations=sum(len(v['silk_clearance_violations'])+len(v['mask_bridge_violations']) for v in report['layers'].values())
violations+=sum(len(report[k]['failures']) for k in ['pth_annular_ring','npth_to_copper','edge_to_copper','paste_on_component_drills'])
print(json.dumps({'checked_geometry_violations':violations}))
if violations:raise SystemExit(1)
