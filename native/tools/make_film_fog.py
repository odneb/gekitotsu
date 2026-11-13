"""Extract only the LIVE-2 simulation actor, without the demonstration level script."""
import unreal, json
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
dest='/Game/GkStage/L_GkFilmFog3'
assert not unreal.EditorAssetLibrary.does_asset_exist(dest), 'Already extracted: use configure_film_fog.py to retune'
assert L.load_level('/Game/FluidNinjaLive/Levels/Fog_Mist')
a,=[a for a in A.get_all_level_actors() if a.get_actor_label()=='Ninja SIM 40m - FVOL 200m']
c,=[c for c in a.get_components_by_class(unreal.ActorComponent) if c.get_name()=='NinjaLiveComponent']
for target,settings in [(a,{'SimActivatedByPawnProximity':False,'InteractionVolumeSize':unreal.Vector(12,32,3)}),(c,{'ResolutionX':128,'ResolutionY':256,'SamplingFPS':30,'OverlapBasedInteraction':True,'HVolumeRender':False,'FVolumeRender':True,'FVolumeExtentsZ':80.0})]:
 for key,value in settings.items():
  target.set_editor_property(key,value)
  unreal.log('FILM FOG '+key+' '+str(target.get_editor_property(key)))
a.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD)
a.set_actor_location_and_rotation(unreal.Vector(0,0,0),unreal.Rotator(0,0,0),False,True)
A.set_selected_level_actors([a])
stream=unreal.EditorLevelUtils.create_new_streaming_level(unreal.LevelStreamingAlwaysLoaded,dest,False)
assert stream
moved=unreal.EditorLevelUtils.move_actors_to_level([a],stream,False,False)
unreal.log("FOG MOVED "+str(moved))
assert moved==1
package=unreal.find_object(None,dest)
assert unreal.EditorLoadingAndSavingUtils.save_packages([package],False)
unreal.log('FILM FOG SAVED')
L.load_level('/Engine/Maps/Entry')

exec(open(str(__file__).replace('make_film_fog.py','configure_film_fog.py')).read())
