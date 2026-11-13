# Probe: what the Dreadmarsh hut floor / scaffold / lantern meshes are and how big, so a dock can be laid from them.
import unreal, os
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
SRC = '/Game/DreamscapeSeries/DreamscapeDreadmarsh/Maps/Dreadmarsh_DemoMap_Resize'
les.load_level(SRC)
out = []
want = ('SM_Dreadmarsh_Hut_Floor_04', 'SM_Dreadmarsh_Hut_Floor_5', 'SM_Dreadmarsh_Hut_Scaffolding_01', 'SM_DreadMarsh_Lantern_01_B', 'SM_DreadMarsh_Lantern_03_B', 'SM_Dreadmarsh_Hut_Log_4', 'SM_Dreadmarsh_Hut_FloorSides_01')
for a in eas.get_all_level_actors():
    lab = a.get_actor_label()
    if lab in want:
        comp = a.get_component_by_class(unreal.StaticMeshComponent)
        mesh = comp.static_mesh if comp else None
        o, e = a.get_actor_bounds(False); l = a.get_actor_location(); r = a.get_actor_rotation(); s = a.get_actor_scale3d()
        out.append('%s | mesh %s | loc %.0f,%.0f,%.0f rot y%.0f scale %.2f,%.2f,%.2f | bounds ctr %.0f,%.0f,%.0f ext %.0f,%.0f,%.0f' % (lab, mesh.get_path_name() if mesh else None, l.x, l.y, l.z, r.yaw, s.x, s.y, s.z, o.x, o.y, o.z, e.x, e.y, e.z))
open(os.path.join(PROJECT, 'Saved', 'dock_probe.txt'), 'w').write('\n'.join(out)); log('probed %d' % len(out))
les.load_level('/Engine/Maps/Entry')
log('DONE dockprobe')
