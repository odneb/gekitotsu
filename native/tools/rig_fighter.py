# Rig the (unrigged, A-pose) Meshy fighter onto a skeleton with the bone names the game drives, and export it for Unreal.
import bpy, sys, math
from mathutils import Vector
src, outdir = sys.argv[-2], sys.argv[-1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
ob = [o for o in bpy.data.objects if o.type == 'MESH'][0]
bpy.context.view_layer.objects.active = ob; ob.select_set(True)
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
me = ob.data
zs = [v.co.z for v in me.vertices]; xs = [v.co.x for v in me.vertices]
H = 1.80; k = H / (max(zs) - min(zs)); z0 = min(zs); xc = (max(xs) + min(xs)) / 2
for v in me.vertices: v.co = Vector(((v.co.x - xc) * k, v.co.y * k, (v.co.z - z0) * k))
me.update()
V = [v.co.copy() for v in me.vertices]
def cen(pts): 
    c = Vector((0, 0, 0))
    for p in pts: c += p
    return c / max(1, len(pts))
xmax = max(p.x for p in V)
handL = cen([p for p in V if p.x > xmax - 0.07])
footL = [p for p in V if p.x > 0.02 and p.z < 0.10]
fc = cen(footL); toe_y = min(p.y for p in footL); heel_y = max(p.y for p in footL)
print('hand', handL, 'foot', fc, 'toe', toe_y, 'heel', heel_y, 'xmax', xmax)
for f in (0.95, 0.9, 0.86, 0.82, 0.78, 0.7, 0.6, 0.5, 0.4, 0.3):
    sl = [p for p in V if abs(p.z - f * H) < 0.012]
    if sl: print('slice %.2f x %.3f..%.3f y %.3f..%.3f' % (f, min(p.x for p in sl), max(p.x for p in sl), min(p.y for p in sl), max(p.y for p in sl)))
# joints (his left is +X, he faces -Y)
sh = Vector((0.175, 0.01, 0.805 * H))
wr = handL + (sh - handL).normalized() * 0.09
el = sh.lerp(wr, 0.5) + Vector((0, 0.012, 0))
hipj = Vector((0.09, 0.0, 0.515 * H)); an = Vector((fc.x, heel_y - 0.075, 0.105)); kn = hipj.lerp(an, 0.5) + Vector((0, -0.015, 0))
ball = Vector((fc.x, toe_y + 0.09, 0.03)); toe = Vector((fc.x, toe_y, 0.03))
J = {'root': (Vector((0, 0, 0)), Vector((0, 0.1, 0)), None),
     'pelvis': (Vector((0, 0, 0.53 * H)), Vector((0, 0, 0.575 * H)), 'root'),
     'spine_01': (Vector((0, 0, 0.575 * H)), Vector((0, 0, 0.62 * H)), 'pelvis'),
     'spine_02': (Vector((0, 0, 0.62 * H)), Vector((0, 0, 0.67 * H)), 'spine_01'),
     'spine_03': (Vector((0, 0, 0.67 * H)), Vector((0, 0, 0.72 * H)), 'spine_02'),
     'spine_04': (Vector((0, 0, 0.72 * H)), Vector((0, 0, 0.77 * H)), 'spine_03'),
     'spine_05': (Vector((0, 0, 0.77 * H)), Vector((0, 0, 0.835 * H)), 'spine_04'),
     'neck_01': (Vector((0, 0, 0.835 * H)), Vector((0, 0, 0.86 * H)), 'spine_05'),
     'neck_02': (Vector((0, 0, 0.86 * H)), Vector((0, 0, 0.885 * H)), 'neck_01'),
     'head': (Vector((0, 0, 0.885 * H)), Vector((0, 0, 0.99 * H)), 'neck_02')}
for sfx, m in (('_l', 1), ('_r', -1)):
    X = lambda p: Vector((p.x * m, p.y, p.z))
    J['clavicle' + sfx] = (X(Vector((0.03, 0.0, 0.815 * H))), X(sh), 'spine_05')
    J['upperarm' + sfx] = (X(sh), X(el), 'clavicle' + sfx)
    J['lowerarm' + sfx] = (X(el), X(wr), 'upperarm' + sfx)
    J['hand' + sfx] = (X(wr), X(wr + (wr - el).normalized() * 0.16), 'lowerarm' + sfx)
    J['thigh' + sfx] = (X(hipj), X(kn), 'pelvis')
    J['calf' + sfx] = (X(kn), X(an), 'thigh' + sfx)
    J['foot' + sfx] = (X(an), X(ball), 'calf' + sfx)
    J['ball' + sfx] = (X(ball), X(toe), 'foot' + sfx)
arm = bpy.data.armatures.new('Armature'); ao = bpy.data.objects.new('Armature', arm); bpy.context.scene.collection.objects.link(ao)
bpy.ops.object.select_all(action='DESELECT'); bpy.context.view_layer.objects.active = ao; ao.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for n, (h, t, p) in J.items():
    b = arm.edit_bones.new(n); b.head = h; b.tail = t
for n, (h, t, p) in J.items():
    if p: arm.edit_bones[n].parent = arm.edit_bones[p]; arm.edit_bones[n].use_connect = False
arm.edit_bones['root'].use_deform = False
bpy.ops.object.mode_set(mode='OBJECT')
# weights: Blender's bone heat; if it fails on this mesh, fall back to nearest-bone weights
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); ao.select_set(True); bpy.context.view_layer.objects.active = ao
bpy.ops.object.parent_set(type='ARMATURE_NAME')
# Weights by distance to each bone, with the arms kept to the sleeves (the coat's side hangs next to them), then smoothed over the surface.
def segd(p, a, b):
    ab = b - a; t = max(0.0, min(1.0, (p - a).dot(ab) / max(1e-9, ab.dot(ab)))); return (p - (a + ab * t)).length
names = [n for n in J if n != 'root']
REACH = {'upperarm': 0.085, 'lowerarm': 0.075, 'hand_': 0.14, 'clavicle': 0.10}
W = []
for v in me.vertices:
    ds = []
    for n in names:
        leg = n[:5] in ('thigh', 'calf_', 'foot_', 'ball_')
        if leg and ((n.endswith('_l') and v.co.x < 0.0) or (n.endswith('_r') and v.co.x > 0.0)): continue
        d = segd(v.co, J[n][0], J[n][1])
        lim = [r for k2, r in REACH.items() if n.startswith(k2)]
        if lim and d > lim[0]: continue
        ds.append((d, n))
    ds.sort(); d0 = ds[0][0]
    ws = {n: math.exp(-((d - d0) / 0.03) ** 2) for d, n in ds[:4]}; tot = sum(ws.values())
    W.append({n: w / tot for n, w in ws.items()})
nb = [[] for _ in me.vertices]
for e in me.edges: a_, b_ = e.vertices; nb[a_].append(b_); nb[b_].append(a_)
for it in range(4):
    W2 = []
    for i, w in enumerate(W):
        acc = dict(w)
        for j in nb[i]:
            for n, x in W[j].items(): acc[n] = acc.get(n, 0.0) + x
        tot = sum(acc.values()); W2.append({n: x / tot for n, x in acc.items()})
    W = W2
for n in names:
    if n not in ob.vertex_groups: ob.vertex_groups.new(name=n)
for i, w in enumerate(W):
    top = sorted(w.items(), key=lambda t: -t[1])[:4]; tot = sum(x for n, x in top)
    for n, x in top:
        if x / tot > 0.01: ob.vertex_groups[n].add([i], x / tot, 'REPLACE')
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
bpy.ops.object.vertex_group_limit_total(limit=4); bpy.ops.object.vertex_group_normalize_all(lock_active=False)
ob.name = 'Fighter'; me.name = 'Fighter'
if me.has_custom_normals:
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.mesh.customdata_custom_splitnormals_clear()
for p in me.polygons: p.use_smooth = True   # smooth normals: the cel shader's shadow edge follows them
# a test pose rendered to check the skinning
sc = bpy.context.scene; sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.light = 'FLAT'; sc.display.shading.color_type = 'TEXTURE'
sc.render.resolution_x = 800; sc.render.resolution_y = 900
cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); sc.collection.objects.link(cam); sc.camera = cam; cam.data.type = 'ORTHO'; cam.data.ortho_scale = 2.6
def shot(name, d, rot):
    cam.location = Vector((0, 0, 0.95)) + d * 6; cam.rotation_euler = rot; sc.render.filepath = outdir + '/' + name + '.png'; bpy.ops.render.render(write_still=True)
pb = ao.pose.bones
def R(n, ax, deg):
    from mathutils import Quaternion
    b = pb[n]; b.rotation_mode = 'QUATERNION'
    # rotate about a world axis
    M = (ao.matrix_world @ b.bone.matrix_local).to_3x3(); la = M.inverted() @ Vector(ax)
    b.rotation_quaternion = Quaternion(la, math.radians(deg)) @ b.rotation_quaternion
R('upperarm_l', (0, 1, 0), -60); R('lowerarm_l', (1, 0, 0), 70); R('upperarm_r', (1, 0, 0), 80); R('lowerarm_r', (1, 0, 0), 60)
R('thigh_l', (1, 0, 0), 70); R('calf_l', (1, 0, 0), -80); R('spine_03', (1, 0, 0), -15); R('head', (0, 0, 1), 30); R('thigh_r', (1, 0, 0), -25)
bpy.context.view_layer.update()
shot('pose_front', Vector((0, -1, 0)), (math.pi / 2, 0, 0)); shot('pose_side', Vector((1, 0, 0)), (math.pi / 2, 0, math.pi / 2)); shot('pose_q', Vector((-0.7, -0.7, 0)).normalized(), (math.pi / 2, 0, -math.pi / 4))
for b in pb: b.rotation_quaternion = (1, 0, 0, 0)
bpy.context.view_layer.update()
# Unreal works in centimetres: make the numbers centimetres and say so, so nothing is rescaled on the way in
bpy.context.scene.unit_settings.scale_length = 0.01
ob.parent = None; ao.scale = (100, 100, 100); ob.scale = (100, 100, 100)
for o in (ao, ob):
    bpy.ops.object.select_all(action='DESELECT'); o.select_set(True); bpy.context.view_layer.objects.active = o
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
ob.parent = ao
bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); ao.select_set(True); bpy.context.view_layer.objects.active = ao
bpy.ops.export_scene.fbx(filepath=outdir + '/SK_Fighter.fbx', use_selection=True, object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=False,
                         apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='FACE', use_armature_deform_only=False, axis_forward='-Z', axis_up='Y', primary_bone_axis='Y', secondary_bone_axis='X')
print('exported')
