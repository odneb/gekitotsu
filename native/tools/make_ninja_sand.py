"""Extract and tune the interactive sand actor from FluidNinja LIVE-2's Sand showcase."""
import unreal

source = '/Game/FluidNinjaLive/Levels/Sand'
destination = '/Game/GkStage/L_GkNinjaSand'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if unreal.EditorAssetLibrary.does_asset_exist(destination):
    raise RuntimeError('Sand extraction already exists')
if not levels.load_level(source):
    raise RuntimeError('Cannot open FluidNinja Sand showcase')

actor, = [a for a in actors.get_all_level_actors()
          if a.get_actor_label().startswith('Ninja SIM 30m - HVOL or SVOL')]
component, = [c for c in actor.get_components_by_class(unreal.ActorComponent)
              if c.get_name() == 'NinjaLiveComponent']

for target, settings in (
    (actor, {
        'SimActivatedByPawnProximity': False,
        'TraceMeshSize': unreal.Vector(6, 18, 1),
        'InteractionVolumeSize': unreal.Vector(6, 18, 2),
    }),
    (component, {
        'ResolutionX': 128,
        'ResolutionY': 384,
        'HVolumeRender': False,
        'FVolumeRender': False,
        'SVolumeRender': True,
        'EnableHeightField': False,
        'SamplingFPS': 30,
        'OverlapBasedInteraction': True,
        'SVolumePositionOffset': unreal.Vector(0, 0, 20),
    }),
):
    for name, value in settings.items():
        target.set_editor_property(name, value)
        log('CANYON SAND', name, target.get_editor_property(name))

actor.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD, unreal.DetachmentRule.KEEP_WORLD)
actor.set_actor_location_and_rotation(unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), False, True)
actors.set_selected_level_actors([actor])
log('CANYON SAND stream API', unreal.EditorLevelUtils.create_new_streaming_level.__doc__)
stream = unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded, destination, True)
if not stream:
    raise RuntimeError('Could not extract the sand actor to a new level')
package = unreal.find_object(None, destination)
if not package or not unreal.EditorLoadingAndSavingUtils.save_packages([package], False):
    raise RuntimeError('Could not save the extracted sand actor')
if not levels.load_level(destination):
    raise RuntimeError('Could not reopen the extracted sand level')
for placed in actors.get_all_level_actors():
    log('CANYON SAND extracted', placed.get_actor_label(), placed.get_actor_location())
levels.load_level('/Engine/Maps/Entry')
