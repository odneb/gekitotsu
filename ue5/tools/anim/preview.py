# Renders poses on the rebuilt model, cel shaded, as the game camera sees a fighter facing left.
# usage: python3 -I preview.py -- kagu.blend poses.json out.jpg [tile width] [columns] [azimuth]
import bpy, sys, os, json, math
import numpy as np
from mathutils import Matrix, Vector
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import anim
A = sys.argv[sys.argv.index('--') + 1:]
BLEND, SPEC, OUT = A[0], json.load(open(A[1])), A[2]; TW = int(A[3]) if len(A) > 3 else 320; COLS = int(A[4]) if len(A) > 4 else 8; AZ = float(A[5]) if len(A) > 5 else 0.0
bpy.ops.wm.open_mainfile(filepath=BLEND)
arm = next(o for o in bpy.data.objects if o.type == 'ARMATURE'); body = next(o for o in bpy.data.objects if o.type == 'MESH')
S = np.diag([1.0, -1.0, 1.0])
def apply(P):
    D = anim.solve(P); pose = {}
    for b in anim.ORDER:
        R, t = D[b]; M = np.eye(4); M[:3, :3] = S @ R @ S; M[:3, 3] = (S @ t) / 100.0
        pose[b] = Matrix(M.tolist()) @ arm.data.bones[b].matrix_local
    for b in anim.ORDER:
        bone = arm.data.bones[b]; pb = arm.pose.bones[b]
        if bone.parent: pb.matrix_basis = (bone.parent.matrix_local.inverted() @ bone.matrix_local).inverted() @ pose[bone.parent.name].inverted() @ pose[b]
        else: pb.matrix_basis = bone.matrix_local.inverted() @ pose[b]
sc = bpy.context.scene; sc.render.engine = 'CYCLES'; sc.cycles.samples = 6; sc.cycles.device = 'CPU'; sc.render.resolution_x = TW; sc.render.resolution_y = int(TW * 1.15)
sc.view_settings.view_transform = 'Standard'
if not sc.world: sc.world = bpy.data.worlds.new('w')
sc.world.use_nodes = True; sc.world.node_tree.nodes['Background'].inputs[0].default_value = (0.2, 0.07, 0.1, 1)
cd = bpy.data.cameras.new('pcam'); cd.type = 'ORTHO'; cd.ortho_scale = 2.35; cam = bpy.data.objects.new('pcam', cd); bpy.context.collection.objects.link(cam); sc.camera = cam
r = math.radians(AZ); cam.location = (math.cos(r) * 8, math.sin(r) * 8, 1.0); cam.rotation_euler = (Vector((0, 0, 1.0)) - cam.location).to_track_quat('-Z', 'Y').to_euler()
lr = math.radians(AZ - 30)
for m in body.data.materials:
    if m and m.node_tree and 'LIGHT' in m.node_tree.nodes: m.node_tree.nodes['LIGHT'].inputs[1].default_value = Vector((math.cos(lr), math.sin(lr), 0.75)).normalized()
# the floor line, so the feet can be judged
bpy.ops.mesh.primitive_plane_add(size=30, location=(0, 0, -0.002)); fl = bpy.context.active_object; fm = bpy.data.materials.new('floor'); fm.use_nodes = True
fm.node_tree.nodes.clear(); o = fm.node_tree.nodes.new('ShaderNodeOutputMaterial'); e = fm.node_tree.nodes.new('ShaderNodeEmission'); e.inputs[0].default_value = (0.06, 0.03, 0.07, 1); fm.node_tree.links.new(e.outputs[0], o.inputs[0]); fl.data.materials.append(fm)
tiles = []
for i, (label, P) in enumerate(SPEC):
    P = {k: (tuple(v) if isinstance(v, list) else v) for k, v in P.items()}
    apply(P); bpy.context.view_layer.update(); sc.render.filepath = '/tmp/gk_tile_%03d.png' % i; bpy.ops.render.render(write_still=True); tiles.append((label, sc.render.filepath))
from PIL import Image, ImageDraw
w, h = sc.render.resolution_x, sc.render.resolution_y; rows = (len(tiles) + COLS - 1) // COLS
sheet = Image.new('RGB', (w * COLS, h * rows), (30, 12, 18)); dr = ImageDraw.Draw(sheet)
for i, (label, p) in enumerate(tiles):
    x, y = (i % COLS) * w, (i // COLS) * h; sheet.paste(Image.open(p).convert('RGB'), (x, y)); dr.text((x + 6, y + 4), label, fill=(255, 235, 200))
sheet.save(OUT, quality=88); print('sheet', OUT, sheet.size)
