import unreal
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert L.load_level('/Game/DreamscapeSeries/DreamscapeDreadmarsh/Maps/Dreadmarsh_DemoMap_Resize')
W=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
changed=unreal.GkStageAuthoring.prepare_forest(W,unreal.Vector(-23164,-21088,-5100),73.0)
unreal.log('FILM STAGE CHANGED '+str(changed))
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in A.get_all_level_actors():
 if any(name in actor.get_class().get_name() for name in ('BP_SpiderAI','BP_FoliageSpawner','GroundFogTool')): A.destroy_actor(actor)
W.get_world_settings().set_editor_property('force_no_precomputed_lighting',True)
assert changed>0
assert L.save_current_level()
L.load_level('/Engine/Maps/Entry')
unreal.SystemLibrary.quit_editor()
