import unreal
unreal.log('NINJA REFRESH COUNT '+str(unreal.GkStageAuthoring.refresh_ninja()))
unreal.EditorAssetLibrary.save_directory('/Game/FluidNinjaLive',only_if_is_dirty=True,recursive=True)
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert L.load_level('/Game/GkStage/L_GkFilmFog3')
a,=[a for a in A.get_all_level_actors() if a.get_class().get_name()=='NinjaLive_C']
c,=[c for c in a.get_components_by_class(unreal.ActorComponent) if c.get_name()=='NinjaLiveComponent']
for key,value in {'PauseSimWhenNotVisible':False,'ComponentIsActive':True,'ComponentIsActivePrevious':True,'bAutoActivate':True,'bVisible':True,'ExecuteCommandsAtStart':[]}.items(): c.set_editor_property(key,value)
assert L.save_current_level()
unreal.log('NINJA ACTIVATION SETTINGS SAVED')

L.load_level('/Engine/Maps/Entry')
unreal.SystemLibrary.quit_editor()
