# Dreadmarsh: a copy of the pack's demo map with a wooden dock laid on the bank in front of the whale skeleton (the pack's own floor planks, logs and lanterns),
# so the fight stands on dry, level boards. A, YAW, TOP are where the fight is set down and the height of the boards.
import unreal, math
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SRC = '/Game/DreamscapeSeries/DreamscapeDreadmarsh/Maps/Dreadmarsh_DemoMap_Resize'; DST = '/Game/GkStage/L_Dreadmarsh'
A = (-19636.0, -19008.0); YAW = 95.0; TOP = -4740.0
if not unreal.EditorAssetLibrary.does_asset_exist(DST):
    les.load_level(SRC)
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log('save as %s' % unreal.EditorLoadingAndSavingUtils.save_map(w, DST))
les.load_level(DST)
for a in list(eas.get_all_level_actors()):
    if a.get_actor_label().startswith('GkDock'): eas.destroy_actor(a)
M = '/Game/DreamscapeSeries/DreamscapeDreadmarsh/Meshes/'
floor = unreal.load_asset(M + 'Modules/HutWalls/SM_Dreadmarsh_Hut_Floor_04'); log_m = unreal.load_asset(M + 'Modules/HutWalls/SM_Dreadmarsh_Hut_Log_01')
lan = unreal.load_asset(M + 'Props/Lanterns/SM_DreadMarsh_Lantern_01_B'); lan3 = unreal.load_asset(M + 'Props/Lanterns/SM_DreadMarsh_Lantern_03_B')
for m in (floor, log_m):   # boards and piles are solid to the game's ground trace
    try:
        bs = m.get_editor_property('body_setup'); bs.set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE); unreal.EditorAssetLibrary.save_loaded_asset(m)
    except Exception as e: log('collision: %s' % str(e)[:120])
f = (math.cos(math.radians(YAW)), math.sin(math.radians(YAW))); rt = (-f[1], f[0])
def put(mesh, fo, lat, z, rot, scale, name):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(A[0] + f[0] * fo + rt[0] * lat, A[1] + f[1] * fo + rt[1] * lat, z), rot)
    a.set_actor_label(name); a.static_mesh_component.set_static_mesh(mesh); a.set_actor_scale3d(unreal.Vector(*scale)); return a
n = 0
for r, fo in enumerate((-140.0, 342.0)):
    for c, lat in enumerate((-620.0, 620.0)):
        put(floor, fo, lat, TOP - 13.0, unreal.Rotator(roll=0, pitch=0, yaw=YAW + 90.0), (1.5, 1.12, 1.0), 'GkDock_Plank_%d_%d' % (r, c)); n += 1
for fo in (-330.0, 520.0):
    for k, lat in enumerate((-1150.0, -575.0, 0.0, 575.0, 1150.0)):
        put(log_m, fo, lat, TOP - 54.0, unreal.Rotator(roll=180, pitch=0, yaw=17.0 * k), (1.5, 1.5, 1.5), 'GkDock_Pile_%d_%d' % (int(fo), k)); n += 1
for s, lat in enumerate((-1180.0, 1180.0)):
    put(log_m, 560.0, lat, TOP, unreal.Rotator(roll=0, pitch=0, yaw=0), (1.2, 1.2, 1.2), 'GkDock_Post_%d' % s)
    put(lan if s == 0 else lan3, 560.0, lat, TOP + 692.0, unreal.Rotator(roll=0, pitch=0, yaw=0), (0.75, 0.75, 0.75) if s == 0 else (1.2, 1.2, 1.2), 'GkDock_Lantern_%d' % s); n += 2
for a in eas.get_all_level_actors():
    if a.get_actor_label().startswith('GkDock_Plank') or a.get_actor_label().startswith('GkDock_Post'):
        o, e = a.get_actor_bounds(False); log('%s ctr %.0f,%.0f,%.0f ext %.0f,%.0f,%.0f' % (a.get_actor_label(), o.x, o.y, o.z, e.x, e.y, e.z))
log('placed %d, saved %s' % (n, les.save_current_level()))
les.load_level('/Engine/Maps/Entry')
log('DONE dock')
