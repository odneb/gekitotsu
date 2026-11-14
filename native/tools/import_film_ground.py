"""Import a painted albedo into a child of the existing eight-layer landscape material."""
import unreal
from pathlib import Path
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
filename=Path(__file__).resolve().parents[1]/'SourceArt/FilmGround.png'
t=unreal.AssetImportTask();t.filename=str(filename);t.destination_path='/Game/GkFx';t.destination_name='T_GkFilmGround';t.automated=True;t.replace_existing=True;t.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);assert t.imported_object_paths
tex=unreal.load_asset('/Game/GkFx/T_GkFilmGround')
tex.set_editor_property('srgb',True)
tex.set_editor_property('power_of_two_mode',unreal.TexturePowerOfTwoSetting.STRETCH_TO_POWER_OF_TWO)
assert A.save_loaded_asset(tex)
parent=unreal.load_asset('/Game/DreamscapeSeries/DreamscapeDreadmarsh/Materials/MI_Landscape_Swamp')
m=unreal.load_asset('/Game/GkFx/MI_GkFilmLandscape') or unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_GkFilmLandscape','/Game/GkFx',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
E.set_material_instance_parent(m,parent)
for k in (3,5,6,7,8):
 E.set_material_instance_texture_parameter_value(m,'T_Layer_%02d_BC'%k,tex)
 E.set_material_instance_scalar_parameter_value(m,'Layer_%02d_Tiling'%k,10.0)
 E.set_material_instance_scalar_parameter_value(m,'Layer_%02d_Normal_Intensity'%k,.35)
 E.set_material_instance_scalar_parameter_value(m,'Layer_%02d_ParallaxHeightRatio'%k,0.)
for n in E.get_scalar_parameter_names(m):
 if 'Tiling' in str(n):unreal.log('FILM LAND TILING '+str(n)+' '+str(E.get_material_instance_scalar_parameter_value(m,n)))
assert A.save_loaded_asset(m)
unreal.log('FILM GROUND IMPORTED')
