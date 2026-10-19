"""建立 R0.5 绝缘卡扣托架、检查几何干涉并导出 STEP/STL 和说明图。

需要 OCP、Shapely、Matplotlib；输入来自原生 PCB 回读和达妙官方 STEP。
模型坐标使用达妙板坐标，PCB 顶面为 Z=0。没有自动寻路或自动布局。
"""
import argparse,json,math
from pathlib import Path
from shapely.geometry import Polygon,box,Point,LineString
from shapely.affinity import scale,translate
from shapely import wkt
from OCP.gp import gp_Pnt,gp_Vec
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox,BRepPrimAPI_MakeCylinder,BRepPrimAPI_MakePrism
from OCP.BRepBuilderAPI import BRepBuilderAPI_MakePolygon,BRepBuilderAPI_MakeFace
from OCP.BRepAlgoAPI import BRepAlgoAPI_Fuse,BRepAlgoAPI_Cut
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.STEPControl import STEPControl_Writer,STEPControl_AsIs
from OCP.StlAPI import StlAPI_Writer
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.TopExp import TopExp_Explorer
from OCP.TopAbs import TopAbs_SOLID,TopAbs_FACE
from OCP.TopoDS import TopoDS
from OCP.TopLoc import TopLoc_Location
from OCP.BRep import BRep_Tool
from OCP.BRepGProp import BRepGProp
from OCP.GProp import GProp_GProps

A=argparse.ArgumentParser(description=__doc__)
A.add_argument('--geometry',type=Path,required=True);A.add_argument('--bodies',type=Path,required=True)
A.add_argument('--damiao',type=Path,required=True);A.add_argument('--out',type=Path,required=True)
args=A.parse_args();P=Path(__file__).parent
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
p=read(P/'parameters.json');g=read(args.geometry);bd=read(args.bodies)['bodies'];dm=read(args.damiao)['items']
args.out.mkdir(parents=True,exist_ok=True)
z=p['board_underside_z'];zt=z+p['board_thickness'];gap=p['xy_running_clearance'];over=p['clip_overlap'];hook_z=zt+p['retention_vertical_gap']
shift=(17.03159025,-18.542)
local=[v for v in g['outlines'][0]['polygon']['polygon'] if v!='L'];board=translate(scale(Polygon(list(zip(local[::2],local[1::2]))),.0254,.0254,origin=(0,0)),*shift)
xl=shift[0]+270*.0254;xr=shift[0]+950*.0254;rear=shift[0]+1190*.0254;front=shift[1]-430*.0254;back=shift[1]+1410*.0254
features=[]
def extrude(poly,low,high):
 wire=BRepBuilderAPI_MakePolygon()
 for x,y in list(poly.exterior.coords)[:-1]:wire.Add(gp_Pnt(x,y,low))
 wire.Close();return BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(0,0,high-low)).Shape()
def add(name,poly,low,high,kind='frame'):
 assert high>low and poly.is_valid and poly.area>0
 features.append({'name':name,'poly':poly,'z0':low,'z1':high,'kind':kind,'shape':extrude(poly,low,high)})
def rect(name,x0,y0,x1,y1,z0,z1,kind='frame'):add(name,box(x0,y0,x1,y1),z0,z1,kind)
def ramp(name,toe,outer,y0,y1,low,high):
 # 插入倒坡使扣齿向外让位；碰撞记录使用其保守矩形包围盒。
 wire=BRepBuilderAPI_MakePolygon()
 for x,h in [(toe,low),(outer,low),(outer,high)]:wire.Add(gp_Pnt(x,y0,h))
 wire.Close();shape=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(0,y1-y0,0)).Shape()
 features.append({'name':name,'poly':box(min(toe,outer),y0,max(toe,outer),y1),'z0':low,'z1':high,'kind':'hook','shape':shape})
def root_round(name,x,sgn,y0,y1,base,r=.8):
 # 圆弧根部过渡；32 段离散圆弧为显式机械轮廓，不作为强度验证。
 profile=[(x-sgn*.05,base-.05),(x+sgn*r,base-.05)]
 profile += [(x+sgn*r+sgn*r*math.cos(-math.pi/2-i*math.pi/64),base+r+r*math.sin(-math.pi/2-i*math.pi/64)) for i in range(33)]
 profile += [(x-sgn*.05,base+r)]
 wire=BRepBuilderAPI_MakePolygon()
 for X,Z in profile:wire.Add(gp_Pnt(X,y0,Z))
 wire.Close();shape=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(0,y1-y0,0)).Shape()
 features.append({'name':name,'poly':box(min(X for X,Z in profile),y0,max(X for X,Z in profile),y1),'z0':base-.05,'z1':base+r,'kind':'spring','shape':shape})
def ramp_y(name,toe,outer,x0,x1,low,high):
 wire=BRepBuilderAPI_MakePolygon()
 for Y,Z in [(toe,low),(outer,low),(outer,high)]:wire.Add(gp_Pnt(x0,Y,Z))
 wire.Close();shape=BRepPrimAPI_MakePrism(BRepBuilderAPI_MakeFace(wire.Wire()).Face(),gp_Vec(x1-x0,0,0)).Shape()
 features.append({'name':name,'poly':box(x0,min(toe,outer),x1,max(toe,outer)),'z0':low,'z1':high,'kind':'hook','shape':shape})
f0,f1=p['frame_bottom_z'],p['frame_top_z'];ox0,ox1=p['outer_rail_x'];y=p['usb_clip_y'];ry=p['rear_clip_y']
# 开放式右侧骨架和 USB 下方承力梁，均在达妙 PCB 的投影之外。
rect('外侧纵梁',ox0,y-1.8,ox1,20.0,f0,f1)
rect('外侧下段加强肋',ox0,y-1.8,ox1,-15.8,f0,z)
rect('外侧上段加强肋',ox0,1.5,ox1,20.0,f0,z)
rect('USB 下横梁',xl-2.0,y-3.0,ox1,y+3.0,f0,-1.1)
add('USB 斜撑',LineString([(xl+.8,y),(ox1-1.4,-21.8)]).buffer(1.3,cap_style='flat',join_style='mitre'),f0,-1.1)
for i,(mx,my) in enumerate(p['mount_centers']):
 add('主控安装座'+str(i+1),Point(mx,my).buffer(p['mount_boss_diameter']/2,quad_segs=48),0,z)
 rect('外侧立柱'+str(i+1),ox0,my-1.4,ox1,my+1.4,f0,z)
 rect('安装座连接梁'+str(i+1),mx-.5,my-1.0,ox1,my+1.0,z-2.5,z)
# 三个刚性托面；USB 处托住板边，避开金属外壳及焊脚。
for side,x,sgn in [('左',xl,-1),('右',xr,1)]:
 a,b=(x+.04,x+2.30) if sgn<0 else (39.60,40.60)
 rect('USB '+side+'刚性座',a,y-1.3,b,y+1.3,f0,z,'seat')
 if sgn>0:
  # 右侧避开 SWD 直出通道，扣齿改抓前边缘；释放方向为 -Y。
  ix0,ix1=39.68,40.53;inside=front-gap;outside=inside-1.0
  rect('USB 右前扣底座',a,outside-.8,b,y+1.2,f0,f1)
  rect('USB 右前弹臂',ix0,outside,ix1,inside,f0,hook_z+1.0,'spring')
  rect('USB 右前扣齿',ix0,outside,ix1,front+over,hook_z,hook_z+.55,'hook')
  ramp_y('USB 右前导入斜面',front+over,outside,ix0,ix1,hook_z+.5,hook_z+1.5)
  rect('USB 右前释放耳',ix0,outside-.6,ix1,inside,hook_z+.55,hook_z+1.0,'spring')
  continue
 # 梁沿 Z 弯曲，让位方向为板外，初始扣口不向 PCB 施加预紧。
 inner=x+sgn*gap;outer=inner+sgn*p['clip_beam_thickness']
 rect('USB '+side+'弹臂',min(inner,outer),y-1.5,max(inner,outer),y+1.5,f0,hook_z+1.0,'spring')
 root_round('USB '+side+'根部圆弧',outer,sgn,y-1.5,y+1.5,-1.1)
 toe=x-sgn*over
 rect('USB '+side+'扣齿',min(toe,outer),y-1.5,max(toe,outer),y+1.5,hook_z,hook_z+.55,'hook')
 ramp('USB '+side+'导入斜面',toe,outer,y-1.5,y+1.5,hook_z+.50,hook_z+1.5)
 rect('USB '+side+'释放耳',min(inner,outer)-(.6 if sgn<0 else 0),y-1.0,max(inner,outer)+(.6 if sgn>0 else 0),y+1.0,hook_z+.55,hook_z+1.0,'spring')
 # 前端挡边承受 USB 向外拔出的纵向载荷；侧面仍留有尺寸间隙。
 rect('USB '+side+'挡边脚',a,front-.9,b,y+1.2,f0,z-2.0)
 rect('USB '+side+'前挡边',a,front-.9,b,front-gap,z-2.1,zt-.15,'stop')
# 后部座只伸入板边空白区。R2 已在 PCB 上向内移 0.381 mm。
rect('后部承力梁',rear+.2,ry-.65,p['mount_centers'][1][0]+.7,ry+.65,z-2.5,z)
rect('后部托唇',rear-.6,ry-.65,rear+.7,ry+.65,z-2.5,z,'seat')
rect('后部纵向挡边',rear-1.3,back+gap,rear+1.0,back+1.0,z-2.5,zt+.1,'stop')
rect('后部挡边连接',rear+.2,ry-.5,rear+1.0,back+.8,z-2.5,z)
# 后扣采用长悬臂，主臂位于主控右侧；端部横梁跨过后部插座上方。
sx=ox1-1.05;root_y=2.5;free_y=ry
rect('后扣根座',sx-.3,root_y-1,sx+1.2,root_y+1,f0,z+.7)
rect('后扣弹臂',sx,root_y,sx+1.0,free_y+.6,z-.5,z+.7,'spring')
rect('后扣升高段',sx,free_y-.6,sx+1,free_y+.6,z-.5,z+4.1,'spring')
rect('后扣横臂',rear+gap,free_y-.6,sx+1,free_y+.6,z+2.9,z+4.1,'spring')
rect('后扣齿背',rear+gap,free_y-.6,rear+gap+1,free_y+.6,hook_z,z+4.1,'spring')
rect('后扣齿',rear-over,free_y-.6,rear+gap+1,free_y+.6,hook_z,hook_z+.55,'hook')
ramp('后扣导入斜面',rear-over,rear+gap+1,free_y-.6,free_y+.6,hook_z+.50,hook_z+1.5)

# 去除 M2 孔；第二个孔沿 Y 留 0.4 mm 调节量，避免强拉主控板孔位。
cuts=[]
for i,(mx,my) in enumerate(p['mount_centers']):
 r=p['mount_screw_clearance']/2;travel=p['mount_slot_travel'] if i else 0
 poly=Point(mx,my).buffer(r,quad_segs=48) if not travel else box(mx-r,my-travel/2,mx+r,my+travel/2).union(Point(mx,my-travel/2).buffer(r,quad_segs=48)).union(Point(mx,my+travel/2).buffer(r,quad_segs=48))
 cuts.append(extrude(poly,-.1,z+.1))
solid=features[0]['shape']
for q in features[1:]:solid=BRepAlgoAPI_Fuse(solid,q['shape']).Shape()
for q in cuts:solid=BRepAlgoAPI_Cut(solid,q).Shape()
assert BRepCheck_Analyzer(solid).IsValid(),'托架实体无效'
e=TopExp_Explorer(solid,TopAbs_SOLID);count=0
while e.More():count+=1;e.Next()
assert count==1,('托架存在不相连实体',count)
mass=GProp_GProps();BRepGProp.VolumeProperties_s(solid,mass)

# 保守包围盒检查：连接器使用官方 STEP，日志板使用封装本体与明确高度假设。
objects=[]
for c in dm:
 if c['ref']=='Board':continue
 x0,y0,z0,x1,y1,z1=c['bbox'];objects.append({'name':'达妙 '+c['ref'],'poly':box(x0,y0,x1,y1),'z0':z0,'z1':z1,'source':'官方 STEP 包围盒'})
for name,c in bd.items():
 if name=='R12':continue
 poly=translate(scale(wkt.loads(c['wkt']),.0254,.0254,origin=(0,0)),*shift)
 height=4.5 if name=='L1' else 2.0
 if c['layer']==2:lo,hi=z-height,z
 else:lo,hi=zt,zt+(10 if name in ['J2','J4'] else 3)
 objects.append({'name':'日志板 '+name,'poly':poly,'z0':lo,'z1':hi,'source':'原生封装本体；高度为模型假设'})
# 通孔焊脚保守按板底突出 3 mm、钻孔半径外加 0.15 mm 核对。
for q in g['freepads']:
 if q.get('metallization') and q.get('hole'):
  x=shift[0]+q['x']*.0254;yq=shift[1]+q['y']*.0254;r=q['hole'][1]*.0254/2+.15
  objects.append({'name':'日志板通孔焊脚 '+q['primitiveId'],'poly':Point(x,yq).buffer(r,quad_segs=32),'z0':z-3,'z1':z,'source':'原生钻孔；焊脚突出 3 mm 为保守假设'})
# 卡片从右侧水平移出，使用卡座全宽与 30 mm 行程的保守检查通道。
sd=next(q for q in objects if q['name']=='日志板 J5');x0,y0,x1,y1=sd['poly'].bounds
objects.append({'name':'microSD 出卡通道','poly':box(x1,y0,x1+30,y1),'z0':zt,'z1':zt+3,'source':'设计检查假设：卡座全宽、30 mm 向右行程'})
ports=['P12','P13','P15','P6','P11']
for c in dm:
 if c['ref'] not in ports:continue
 x0,y0,z0,x1,y1,z1=c['bbox'];objects.append({'name':c['ref']+' 竖插通道','poly':box(x0-.5,y0-.5,x1+.5,y1+.5),'z0':z0,'z1':40,'source':'裸插座外扩 0.5 mm；不代表真实线束全部尺寸'})
for c in dm:
 if c['ref'] not in ['J1','P4','P5','P10','P1','U13','FPC1','SW1','SW2']:continue
 x0,y0,z0,x1,y1,z1=c['bbox'];ref=c['ref']
 if ref in ['J1','P4','P5','P10']:path=box(x0-.5,y1-.25,x1+.5,y1+20)
 elif ref in ['P1','SW1','SW2']:path=box(x0-.5,y0-(6 if ref.startswith('SW') else 20),x1+.5,y0+.25)
 else:path=box(x1-.25,y0-.5,x1+20,y1+.5)
 objects.append({'name':ref+' 侧向操作通道','poly':path,'z0':z0-.5,'z1':z1+.5,'source':'插座本体加 0.5 mm、20 mm 直出；按键为 6 mm 操作通道。实际线缆须试配'})
for i,(mx,my) in enumerate(p['mount_centers']):
 objects.append({'name':'安装螺钉'+str(i+1)+' 工具通道','poly':Point(mx,my).buffer(1.25,quad_segs=48),'z0':z+.05,'z1':30,'source':'主控安装孔上方预留直径 2.5 mm 直入工具通道；按所选螺钉和工具确认'})
collisions=[];near=[]
for f in features:
 if f['kind']=='seat' and f['z1']>z+1e-6:raise ValueError('托面高度错误')
 for obj in objects:
  area=f['poly'].intersection(obj['poly']).area;dz=min(f['z1'],obj['z1'])-max(f['z0'],obj['z0'])
  if area>1e-6 and dz>1e-6:collisions.append({'feature':f['name'],'object':obj['name'],'overlap_mm3':area*dz})
  xy=f['poly'].distance(obj['poly']);zz=max(0,f['z0']-obj['z1'],obj['z0']-f['z1']);dist=math.hypot(xy,zz)
  if dist<.5:near.append({'feature':f['name'],'object':obj['name'],'gap_mm':dist})
 # 仅允许托面在下表面接触；锁齿位于 PCB 顶面以上。
 overlap=f['poly'].intersection(board).area;dz=min(f['z1'],zt)-max(f['z0'],z)
 if overlap>1e-6 and dz>1e-6:collisions.append({'feature':f['name'],'object':'日志板 PCB','overlap_mm3':overlap*dz})
board_host=next(q for q in dm if q['ref']=='Board')['bbox'];hostpoly=box(board_host[0],board_host[1],board_host[3],board_host[4])
for f in features:
 if f['z0']<0 and f['z1']>board_host[2] and f['poly'].intersection(hostpoly).area>1e-6:collisions.append({'feature':f['name'],'object':'达妙 PCB 矩形外包络','overlap_mm3':'待核对'})
report={'revision':p['revision'],'pcb_bbox_mm':[board.bounds[2]-board.bounds[0],board.bounds[3]-board.bounds[1]],'pcb_area_mm2':board.area,'bracket_bbox_mm':[min(f['poly'].bounds[0] for f in features),min(f['poly'].bounds[1] for f in features),min(f['z0'] for f in features),max(f['poly'].bounds[2] for f in features),max(f['poly'].bounds[3] for f in features),max(f['z1'] for f in features)],'one_valid_solid':count==1,'volume_mm3':mass.Mass(),'collisions':collisions,'near_pairs':near,'checked_operation_channels':[{'name':o['name'],'bbox':list(o['poly'].bounds),'z0':o['z0'],'z1':o['z1'],'basis':o['source']} for o in objects if o['name'].endswith('通道')],'spring_strain_estimate':{'method':'等截面悬臂小挠度估计 epsilon=1.5*t*delta/L^2；固定端取实际横梁顶面。未计根部应力集中、后扣横臂柔度及打印各向异性','USB_left':1.5*p['clip_beam_thickness']*(over+.15)/(hook_z+1.1-.8)**2,'USB_right':1.5*(over+.15)/(hook_z+1.1)**2,'rear':1.5*(over+.15)/(free_y-root_y-1)**2,'delta_mm':over+.15,'material_allowable_verified':False},'limitations':['托架为试配设计，不是承载、振动或寿命认证。','H40/H35 叠高须实测校准，卡扣不得使 PCB 预弯。','主控原有固定螺钉的长度、机架空位需按装配实物确定。','部件包围盒、底面高度和插拔通道均按记录假设检查，未覆盖任意插头包胶或弯折。','BOOT0 仍需拆下日志板维护。']}
(args.out/'bracket-check.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
(args.out/'bracket-features.json').write_text(json.dumps([{k:(v.wkt if k=='poly' else v) for k,v in f.items() if k!='shape'} for f in features],ensure_ascii=False,indent=2),encoding='utf-8')
writer=STEPControl_Writer();writer.Transfer(solid,STEPControl_AsIs);writer.Write(str(args.out/'logger-clip-bracket.step'))
BRepMesh_IncrementalMesh(solid,.06,False,.25,True).Perform();stl=StlAPI_Writer();stl.Write(solid,str(args.out/'logger-clip-bracket.stl'))
print(json.dumps({'solid_count':count,'volume_mm3':mass.Mass(),'collisions':collisions,'near_count':len(near),'bbox':report['bracket_bbox_mm']},ensure_ascii=False,indent=2))

# 导出可视图：实体托架和简化叠装，明确区别于实物照片。
import matplotlib;matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.font_manager import FontProperties
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
font_candidates=[Path('C:/Windows/Fonts/msyh.ttc'),Path('/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc')]
for fp in font_candidates:
 if fp.exists():plt.rcParams['font.family']=FontProperties(fname=str(fp)).get_name();break
def triangles(shape):
 out=[];e=TopExp_Explorer(shape,TopAbs_FACE)
 while e.More():
  face=TopoDS.Face(e.Current());loc=TopLoc_Location();tri=BRep_Tool.Triangulation_s(face,loc)
  if tri:
   tr=loc.Transformation()
   for i in range(1,tri.NbTriangles()+1):
    ids=tri.Triangle(i).Get();out.append([[tri.Node(j).Transformed(tr).X(),tri.Node(j).Transformed(tr).Y(),tri.Node(j).Transformed(tr).Z()] for j in ids])
  e.Next()
 return out
mesh=triangles(solid)
def prism(ax,poly,lo,hi,color,alpha=1):
 for part in getattr(poly,'geoms',[poly]):
  pts=list(part.exterior.coords)[:-1];faces=[[(x,y,lo) for x,y in pts],[(x,y,hi) for x,y in pts]]
  faces += [[(x,y,lo),(X,Y,lo),(X,Y,hi),(x,y,hi)] for (x,y),(X,Y) in zip(pts,pts[1:]+pts[:1])]
  ax.add_collection3d(Poly3DCollection(faces,facecolors=color,edgecolors='#42565b',linewidths=.2,alpha=alpha))
for assembly in [False,True]:
 fig=plt.figure(figsize=(12,9));ax=fig.add_subplot(111,projection='3d')
 ax.add_collection3d(Poly3DCollection(mesh,facecolors='#eead55',edgecolors='#a2753a',linewidths=.1,alpha=1))
 if assembly:
  prism(ax,hostpoly,board_host[2],0,'#28504b',.7);prism(ax,board,z,zt,'#3bab89',.8)
  for obj in objects:
   if obj['name'].endswith('通道'):continue
   if obj['name'].startswith('日志板') and obj['name'] not in ['日志板 U1','日志板 J2','日志板 J4','日志板 J5']:continue
   if obj['name'].startswith('达妙 ') and obj['name'][3:] not in ['P1','P4','P5','P6','P10','P11','P12','P13','P15','P16','J1','U13']:continue
   prism(ax,obj['poly'],obj['z0'],obj['z1'],'#b6c8cb' if obj['name'].startswith('达妙') else '#556e87',.75)
 ax.set_xlim(8 if assembly else 20,64);ax.set_ylim(-34,24);ax.set_zlim(-6,20);ax.set_box_aspect((56 if assembly else 44,58,26));ax.view_init(elev=33,azim=-57)
 ax.set_xlabel('X / mm');ax.set_ylabel('Y / mm');ax.set_zlabel('Z / mm');ax.set_title('R0.5｜'+('P16 直插与绝缘卡扣托架（简化高度模型）' if assembly else '绝缘卡扣托架实体'),fontsize=16)
 fig.text(.5,.035,'黄色：托架；绿色：PCB。名义叠高 7.5 mm，打印前须实测校准；未做强度及振动实测。',ha='center',fontsize=10)
 fig.savefig(args.out/('bracket-assembly.png' if assembly else 'bracket.png'),dpi=150,bbox_inches='tight');plt.close(fig)
if collisions:raise SystemExit('存在几何碰撞，导出仅用于定位问题，不可作为通过版本使用。')
