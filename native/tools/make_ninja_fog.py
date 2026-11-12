# A small level holding only FluidNinja's low-lying fog simulation (taken from its Fog_Mist demo level), to be streamed into a stage at the fight's centre.
import unreal, os
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/GkStage/L_GkNinjaFog'):
    les.load_level('/Game/FluidNinjaLive/Levels/Fog_Mist')
    w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log('save as %s' % unreal.EditorLoadingAndSavingUtils.save_map(w, '/Game/GkStage/L_GkNinjaFog'))
les.load_level('/Game/GkStage/L_GkNinjaFog')
keep = None
for a in eas.get_all_level_actors():
    if a.get_actor_label() == 'Ninja SIM 40m - FVOL 200m': keep = a
n = 0
for a in eas.get_all_level_actors():
    if a != keep:
        try:
            if eas.destroy_actor(a): n += 1
        except Exception as e: pass
if keep:
    keep.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD)
    keep.set_actor_location(unreal.Vector(0, 0, 0), False, True)
    for k, v in (('SimActivatedByPawnProximity', False),):
        try: keep.set_editor_property(k, v); log('set %s' % k)
        except Exception as e: log('no %s: %s' % (k, str(e)[:120]))
    for k in ('SimActivatedByPawnProximity', 'InteractionVolumeSize', 'SourceActor', 'OverlapFilterInclusiveObjType', 'TraceMeshInactiveBehaviour', 'OverlapFilterInclusiveBoneNames'):
        try: log('%s = %s' % (k, str(keep.get_editor_property(k))[:300]))
        except Exception as e: log('no %s' % k)
log('removed %d, left %s' % (n, [(a.get_actor_label(), a.get_class().get_name()) for a in eas.get_all_level_actors()]))
log('saved %s' % les.save_current_level())
les.load_level('/Engine/Maps/Entry')
log('DONE ninjafog')
