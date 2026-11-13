"""Retune the extracted LIVE-2 actor; run in a full editor with ExecutePythonScript."""
import unreal
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
A=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
E=unreal.MaterialEditingLibrary
assert L.load_level('/Game/GkStage/L_GkFilmFog3')
a,=[a for a in A.get_all_level_actors() if a.get_class().get_name()=='NinjaLive_C']
c,=[c for c in a.get_components_by_class(unreal.ActorComponent) if c.get_name()=='NinjaLiveComponent']
material=unreal.load_asset('/Game/GkFx/MI_GkFilmMist') or unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_GkFilmMist','/Game/GkFx',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
E.set_material_instance_parent(material,unreal.load_asset('/Game/FluidNinjaLive/OutputMaterials/SVOL/MI_SVOL_DirLit_Noisy_Sand_Small'))
for key,value in {'MaxViewSteps':64.0,'Density':8.0,'Extrude':.035,'ExtrudeSoften':.03,'XBackgroundDensity':.008,'XBackgroundDensityNoise':.12,'NoiseDisplace':1.2,'FlowStrength':.35,'FadeDistance':4000.0}.items():E.set_material_instance_scalar_parameter_value(material,key,value)
for key,value in {'Color1':unreal.LinearColor(.24,.29,.31,1),'Color2':unreal.LinearColor(.64,.68,.62,1),'AmbientColor':unreal.LinearColor(.12,.15,.18,1)}.items():E.set_material_instance_vector_parameter_value(material,key,value)
assert unreal.EditorAssetLibrary.save_loaded_asset(material)
for key,value in {'SimActivatedByPawnProximity':False,'InteractionVolumeSize':unreal.Vector(12,32,3)}.items():a.set_editor_property(key,value)
settings={'ExtentsXYZ':unreal.Vector(8,28,1.5),'ResolutionX':128,'ResolutionY':256,'SamplingFPS':30,'OverlapBasedInteraction':True,'HVolumeRender':False,'FVolumeRender':False,'SVolumeRender':True,'SVolumePositionOffset':unreal.Vector(0,0,5),'SVolumeSizeMultiplier':1.0,'EnableHeightField':False,'DensityNoiseAmount':.25,'DirectVelocityY':.12,'SecondaryOutputMaterials':[material],'SecondaryOutputMaterialSelected':0,'EraserModeREADTOOLTIP':False,'InvertFieldAndPointDensity':0.0,'LiveV1LegacyMode':False,'DensityAccumulation':.96,'BrushStrength':.55,'GlobalBrushScale':.7,'PauseSimWhenNotVisible':False,'ComponentIsActive':True,'ExecuteCommandsAtStart':[]}
for key,value in settings.items():c.set_editor_property(key,value)
assert L.save_current_level()
L.load_level('/Engine/Maps/Entry')

unreal.SystemLibrary.quit_editor()
