# Rebuilds the generated fighter model for cel shading, the way hand-made anime-style game models are prepared:
#   - welds the mesh, optionally subdivides it for rounder silhouettes
#   - replaces the lumpy generated normals with normals taken from a smoothed stand-in shape, so shadows fall in
#     broad, clean shapes instead of noise
#   - writes a UV coverage mask so the flat colour texture can be bled past every UV island edge
#   - renders cel-shaded before/after pictures, and exports an FBX for Unreal
# usage: python3 -I rebuild.py -- in.fbx outdir [subdiv 0|1] [voxel metres] [smooth iterations]
import bpy, bmesh, sys, os, math
import numpy as np
from mathutils import Vector
A = sys.argv[sys.argv.index('--') + 1:]
FRONT = -90
SRC, OUT = A[0], A[1]; SUBDIV = int(A[2]) if len(A) > 2 else 0; VOX = float(A[3]) if len(A) > 3 else 0.03; SMOOTH = int(A[4]) if len(A) > 4 else 20
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=SRC, automatic_bone_orientation=False)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE'); body = next(o for o in bpy.data.objects if o.type == 'MESH')
for a in list(bpy.data.actions): bpy.data.actions.remove(a)
if arm.animation_data: arm.animation_data_clear()
for pb in arm.pose.bones: pb.location = (0, 0, 0); pb.rotation_quaternion = (1, 0, 0, 0); pb.rotation_euler = (0, 0, 0); pb.scale = (1, 1, 1)
bpy.context.view_layer.update()
print('scale arm', tuple(round(x, 4) for x in arm.scale), 'body', tuple(round(x, 4) for x in body.scale), 'dims', tuple(round(x, 3) for x in body.dimensions))

def active(o):
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o

def stats(tag):
    m = body.data; m.calc_loop_triangles(); print(tag, 'verts', len(m.vertices), 'tris', len(m.loop_triangles))

# ---------------------------------------------------------------- weld
stats('in  ')
bm = bmesh.new(); bm.from_mesh(body.data); bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5); bm.to_mesh(body.data); bm.free()
stats('weld')

# ---------------------------------------------------------------- before picture needs the original normals: keep a copy
orig = body.copy(); orig.data = body.data.copy(); bpy.context.collection.objects.link(orig); orig.name = 'orig'
orig.modifiers.clear(); orig.parent = None; orig.matrix_world = body.matrix_world.copy()

if SUBDIV:
    active(body)
    md = body.modifiers.new('sub', 'SUBSURF'); md.levels = SUBDIV; md.subdivision_type = 'CATMULL_CLARK'; md.boundary_smooth = 'PRESERVE_CORNERS'
    # the armature modifier must stay last
    while body.modifiers[0].name != 'sub': bpy.ops.object.modifier_move_up(modifier='sub')
    bpy.ops.object.modifier_apply(modifier='sub'); stats('subd')

# ---------------------------------------------------------------- a smoothed stand-in, and its normals carried across
proxy = body.copy(); proxy.data = body.data.copy(); bpy.context.collection.objects.link(proxy); proxy.name = 'proxy'
proxy.modifiers.clear(); proxy.parent = None; proxy.matrix_world = body.matrix_world.copy()
active(proxy)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
rm = proxy.modifiers.new('rm', 'REMESH'); rm.mode = 'VOXEL'; rm.voxel_size = VOX; rm.adaptivity = 0.0; bpy.ops.object.modifier_apply(modifier='rm')
sm = proxy.modifiers.new('sm', 'SMOOTH'); sm.factor = 0.8; sm.iterations = SMOOTH; bpy.ops.object.modifier_apply(modifier='sm')
for p in proxy.data.polygons: p.use_smooth = True
print('proxy verts', len(proxy.data.vertices))
active(body)
for p in body.data.polygons: p.use_smooth = True
dt = body.modifiers.new('nt', 'DATA_TRANSFER'); dt.object = proxy; dt.use_loop_data = True; dt.data_types_loops = {'CUSTOM_NORMAL'}; dt.loop_mapping = 'POLYINTERP_NEAREST'
while body.modifiers[0].name != 'nt': bpy.ops.object.modifier_move_up(modifier='nt')
bpy.ops.object.modifier_apply(modifier='nt')
print('custom normals', body.data.has_custom_normals)

# ---------------------------------------------------------------- UV coverage, for bleeding the texture past island edges
from PIL import Image, ImageDraw
N = 2048; cov = Image.new('L', (N, N), 0); dr = ImageDraw.Draw(cov); uv = body.data.uv_layers.active.data
for p in body.data.polygons:
    pts = [(uv[i].uv.x * N, (1 - uv[i].uv.y) * N) for i in p.loop_indices]; dr.polygon(pts, fill=255, outline=255)
cov.save(os.path.join(OUT, 'uv_cover.png'))

# ---------------------------------------------------------------- pictures
def toon(name, tex):
    m = bpy.data.materials.new(name); m.use_nodes = True; nt = m.node_tree; nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial'); em = nt.nodes.new('ShaderNodeEmission'); geo = nt.nodes.new('ShaderNodeNewGeometry')
    dot = nt.nodes.new('ShaderNodeVectorMath'); dot.operation = 'DOT_PRODUCT'; dot.name = 'LIGHT'; dot.inputs[1].default_value = Vector((0.45, -0.62, 0.6)).normalized()
    st = nt.nodes.new('ShaderNodeMath'); st.operation = 'GREATER_THAN'; st.inputs[1].default_value = 0.02
    mix = nt.nodes.new('ShaderNodeMix'); mix.data_type = 'RGBA'
    nt.links.new(geo.outputs['Normal'], dot.inputs[0]); nt.links.new(dot.outputs['Value'], st.inputs[0]); nt.links.new(st.outputs[0], mix.inputs[0])
    if tex:
        t1 = nt.nodes.new('ShaderNodeTexImage'); t1.image = bpy.data.images.load(tex[0]); t2 = nt.nodes.new('ShaderNodeTexImage'); t2.image = bpy.data.images.load(tex[1])
        inkm = nt.nodes.new('ShaderNodeMix'); inkm.data_type = 'RGBA'; inkm.inputs[6].default_value = (0.02, 0.01, 0.02, 1)
        nt.links.new(t1.outputs['Alpha'], inkm.inputs[0]); nt.links.new(t1.outputs['Color'], inkm.inputs[7])
        nt.links.new(t2.outputs['Color'], mix.inputs[6]); nt.links.new(inkm.outputs[2], mix.inputs[7])
    else:
        mix.inputs[6].default_value = (0.25, 0.25, 0.4, 1); mix.inputs[7].default_value = (0.9, 0.9, 0.9, 1)
    nt.links.new(mix.outputs[2], em.inputs['Color']); nt.links.new(em.outputs[0], out.inputs[0]); return m

def render(objs, path, mat):
    for o in bpy.data.objects:
        if o.type == 'MESH': o.hide_render = o not in objs
    for o in objs: o.data.materials.clear(); o.data.materials.append(mat)
    sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.samples = 8; sc.cycles.device = 'CPU'; sc.render.resolution_x = 700; sc.render.resolution_y = 900
    sc.view_settings.view_transform = 'Standard'; sc.render.film_transparent = False
    if not sc.world: sc.world = bpy.data.worlds.new('w')
    sc.world.use_nodes = True; sc.world.node_tree.nodes['Background'].inputs[0].default_value = (0.16, 0.05, 0.08, 1)
    cam = bpy.data.objects.get('cam')
    if not cam:
        cd = bpy.data.cameras.new('cam'); cd.type = 'ORTHO'; cam = bpy.data.objects.new('cam', cd); bpy.context.collection.objects.link(cam); sc.camera = cam
    for i, (az, sc_, cz) in enumerate(VIEWS):
        r = math.radians(az); cam.location = (math.cos(r) * 6, math.sin(r) * 6, cz); cam.data.ortho_scale = sc_
        lr = math.radians(az + 65); mat.node_tree.nodes['LIGHT'].inputs[1].default_value = Vector((math.cos(lr), math.sin(lr), 0.75)).normalized()
        d = Vector((0, 0, cz)) - cam.location; cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()
        sc.render.filepath = '%s_%d.png' % (path, i); bpy.ops.render.render(write_still=True)

VIEWS = [(-90, 2.0, 0.9), (0, 2.0, 0.9), (90, 2.0, 0.9), (180, 2.0, 0.9)] if '--find' in sys.argv else [(FRONT - 40, 2.0, 0.9), (FRONT + 60, 2.0, 0.9), (FRONT - 30, 0.8, 1.42)]
T = (os.path.join(OUT, 'Kagu_Flat.png'), os.path.join(OUT, 'Kagu_Shade.png')) if os.path.exists(os.path.join(OUT, 'Kagu_Flat.png')) else None
if '--render' in sys.argv:
    if '--find' not in sys.argv: render([orig], os.path.join(OUT, 'before'), toon('grey', None))
    if '--find' not in sys.argv: render([body], os.path.join(OUT, 'after'), toon('grey2', None))
    if T: render([body], os.path.join(OUT, 'colour'), toon('col', T))

# ---------------------------------------------------------------- out to Unreal
for o in (orig, proxy): bpy.data.objects.remove(o)
body.data.materials.clear(); body.data.materials.append(bpy.data.materials.new('Kagu'))
bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True); body.select_set(True); bpy.context.view_layer.objects.active = arm
# leave no scale or rotation on the objects: the skeleton goes out at true size, in centimetres, with clean bones
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
bpy.context.view_layer.update()
print('applied: arm scale', tuple(round(x, 3) for x in arm.scale), 'body dims', tuple(round(x, 3) for x in body.dimensions), 'hips head', tuple(round(x, 3) for x in arm.data.bones['Hips'].head_local))
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, 'Kagu_V2.fbx'), use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=False, apply_scale_options='FBX_SCALE_ALL',
                         mesh_smooth_type='OFF', use_tspace=False, use_armature_deform_only=False, primary_bone_axis='Y', secondary_bone_axis='X', axis_forward='-Z', axis_up='Y')
print('exported')
