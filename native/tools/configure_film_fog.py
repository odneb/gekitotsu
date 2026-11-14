"""Retune the extracted LIVE-2 actor; run in a full editor with ExecutePythonScript."""
import unreal
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.MaterialEditingLibrary
assert L.load_level('/Game/GkStage/L_GkFilmFog3')
a,=[a for a in A.get_all_level_actors() if a.get_class().get_name()=='NinjaLive_C']
c,=[c for c in a.get_components_by_class(unreal.ActorComponent) if c.get_name()=='NinjaLiveComponent']
# Build the local cel volume with refine_swamp_vapor.py before configuring this actor.
material=unreal.load_asset('/Game/GkFx/MI_GkFilmMist')
assert material, 'Run prepare_render_assets.sh first'
for key,value in {'SimActivatedByPawnProximity':False,'InteractionVolumeSize':unreal.Vector(26,32,3)}.items():a.set_editor_property(key,value)
settings={'ExtentsXYZ':unreal.Vector(24,28,1.5),'ResolutionX':256,'ResolutionY':512,'SamplingFPS':30,'OverlapBasedInteraction':True,'HVolumeRender':False,'FVolumeRender':False,'SVolumeRender':True,'SVolumePositionOffset':unreal.Vector(0,0,5),'SVolumeSizeMultiplier':1.0,'EnableHeightField':False,'DensityNoiseAmount':3.0,'DensityNoiseTile':1.2,'DensityNoiseSpeed':.025,'DirectVelocityY':-.08,'SecondaryOutputMaterials':[material],'SecondaryOutputMaterialSelected':0,'EraserModeREADTOOLTIP':False,'InvertFieldAndPointDensity':0.0,'LiveV1LegacyMode':False,'DensityAccumulation':.96,'BrushStrength':.55,'GlobalBrushScale':.7,'PauseSimWhenNotVisible':False,'ComponentIsActive':True,'ExecuteCommandsAtStart':[]}
for key,value in settings.items():c.set_editor_property(key,value)
assert L.save_current_level()
L.load_level('/Engine/Maps/Entry')

unreal.SystemLibrary.quit_editor()
