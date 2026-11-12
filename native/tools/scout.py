# Lists what stands where in a pack's demo map: cameras, player starts and the landmark meshes, so a fight can be set down in front of them.
import unreal, os, re
MAPS = MAPS if 'MAPS' in dir() else []
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
KEY = re.compile(r'skelet|skull|whale|torii|gate|bridge|shrine|temple|pagoda|tower|tree_?large|bigtree|giant|glow|statue|hut|house|castle|lantern|boat|altar|arch|ruin|waterfall|windmill|well|camp|cathedral|light_?house|sakura|cherry|mushroom', re.I)
for m in MAPS:
    out = []
    try:
        ok = les.load_level(m); w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        out.append('map %s loaded %s' % (m, ok))
        descs = []
        try: descs = unreal.WorldPartitionBlueprintLibrary.get_actor_descs() or []
        except Exception as e: out.append('no descs: %s' % str(e)[:100])
        out.append('descs %d' % len(descs))
        seen = {}
        for d in descs:
            cls = str(d.native_class.get_name()) if d.native_class else ''; lab = str(d.label); b = d.bounds
            c = (b.min + b.max) * 0.5; e = (b.max - b.min)
            line = '%s | %s | %.0f,%.0f,%.0f | size %.0f,%.0f,%.0f' % (cls, lab, c.x, c.y, c.z, e.x, e.y, e.z)
            if 'Camera' in cls or 'PlayerStart' in cls or 'Sequence' in cls or 'Light' in cls or 'Fog' in cls or 'Sky' in cls or 'PostProcess' in cls or 'Landscape' in cls: out.append(line)
            elif KEY.search(lab):
                k = re.sub(r'\d+$', '', lab); seen[k] = seen.get(k, 0) + 1
                if seen[k] <= 6: out.append(line)
        acts = eas.get_all_level_actors(); out.append('loaded actors %d' % len(acts))
        seen = {}
        for a in acts:
            cls = a.get_class().get_name(); lab = a.get_actor_label(); l = a.get_actor_location(); r = a.get_actor_rotation()
            if 'Camera' in cls or 'PlayerStart' in cls or 'Sequence' in cls or 'DirectionalLight' in cls or 'Fog' in cls or 'Sky' in cls or 'PostProcess' in cls:
                out.append('A %s | %s | %.0f,%.0f,%.0f | rot p%.0f y%.0f' % (cls, lab, l.x, l.y, l.z, r.pitch, r.yaw))
            elif not descs and KEY.search(lab):
                k = re.sub(r'\d+$', '', lab); seen[k] = seen.get(k, 0) + 1
                if seen[k] <= 6:
                    o, ext = a.get_actor_bounds(False); out.append('A %s | %s | %.0f,%.0f,%.0f | size %.0f,%.0f,%.0f | yaw %.0f' % (cls, lab, o.x, o.y, o.z, ext.x * 2, ext.y * 2, ext.z * 2, r.yaw))
    except Exception as e:
        import traceback; out.append(traceback.format_exc())
    open(os.path.join(PROJECT, 'Saved', 'scout_%s.txt' % m.split('/')[-1]), 'w').write('\n'.join(out)); log('scouted %s: %d lines' % (m, len(out)))
les.load_level('/Engine/Maps/Entry')
log('DONE scout')
