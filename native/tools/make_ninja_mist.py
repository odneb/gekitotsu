# A small level holding one of FluidNinja LIVE's upright smoke simulations (the "Smoke Chamber" container of its VolumeDemo_FAKE level),
# stood on the plane the fight is fought on and stretched to the stage, to be streamed in at the fight's centre.
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
DST = '/Game/GkStage/L_GkNinjaMist'
if not unreal.EditorAssetLibrary.does_asset_exist(DST):
    les.load_level('/Game/FluidNinjaLive/Levels/Misc/VolumeDemo_FAKE')
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log('save as %s' % unreal.EditorLoadingAndSavingUtils.save_map(w, DST))
les.load_level(DST)
keep = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == 'Container1' and a.get_class().get_name() == 'NinjaLive_C': keep = a
n = 0
for a in eas.get_all_level_actors():
    if a != keep:
        try:
            if eas.destroy_actor(a): n += 1
        except Exception as e: pass
if keep:
    keep.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD)
    for k in ('TraceMeshSize', 'InteractionVolumeSize', 'ActivationVolumeSize', 'SimActivatedByPawnProximity'):
        try: log('was %s = %s' % (k, str(keep.get_editor_property(k))[:200]))
        except Exception as e: log('no %s' % k)
    for k, v in (('SimActivatedByPawnProximity', False), ('TraceMeshSize', unreal.Vector(5, 18, 1)), ('InteractionVolumeSize', unreal.Vector(5, 18, 3))):
        try: keep.set_editor_property(k, v); log('set %s' % k)
        except Exception as e: log('no %s: %s' % (k, str(e)[:120]))
    keep.set_actor_location_and_rotation(unreal.Vector(0, 0, 0), unreal.Rotator(roll=0, pitch=90, yaw=0), False, True)
    try: keep.rerun_construction_scripts()
    except Exception as e: log('no rerun: %s' % str(e)[:100])
    o, ext = keep.get_actor_bounds(False); log('bounds %s ext %s rot %s' % (o, ext, keep.get_actor_rotation()))
log('removed %d, left %s' % (n, [(a.get_actor_label(), a.get_class().get_name()) for a in eas.get_all_level_actors()]))
log('saved %s' % les.save_current_level())
les.load_level('/Engine/Maps/Entry')
log('DONE ninjamist')
