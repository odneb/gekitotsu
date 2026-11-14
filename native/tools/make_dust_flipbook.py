"""Original anime dust atlas, animated on the bounded contact-effect pool."""
import unreal
from pathlib import Path
E=unreal.MaterialEditingLibrary;A=unreal.EditorAssetLibrary
file=Path(__file__).resolve().parents[1]/'SourceArt/FilmDustAtlas.png'
t=unreal.AssetImportTask();t.filename=str(file);t.destination_path='/Game/GkFx';t.destination_name='T_GkFilmDust';t.automated=True;t.replace_existing=True;t.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);assert t.imported_object_paths
tex=unreal.load_asset('/Game/GkFx/T_GkFilmDust');tex.set_editor_property('srgb',True);A.save_loaded_asset(tex)
name='M_GkDustFlipbook';m=unreal.load_asset('/Game/GkFx/'+name) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/GkFx',unreal.Material,unreal.MaterialFactoryNew())
E.delete_all_material_expressions(m);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);m.set_editor_property('used_with_instanced_static_meshes',True);m.set_editor_property('two_sided',True);m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
def node(cls):return E.create_material_expression(m,cls)
uv=node(unreal.MaterialExpressionTextureCoordinate);texture=node(unreal.MaterialExpressionTextureObjectParameter);texture.set_editor_property('parameter_name','Atlas');texture.set_editor_property('texture',tex)
inputs={'UV':uv,'Atlas':texture}
for k,key in enumerate(['R','G','B','Age']):
 n=node(unreal.MaterialExpressionPerInstanceCustomData);n.set_editor_property('data_index',k);inputs[key]=n
for output,kind,code in [('MP_EMISSIVE_COLOR',unreal.CustomMaterialOutputType.CMOT_FLOAT3,'return pixel.rgb*float3(R,G,B)*1.25;'),('MP_OPACITY_MASK',unreal.CustomMaterialOutputType.CMOT_FLOAT1,'return pixel.a;')]:
 c=node(unreal.MaterialExpressionCustom);c.set_editor_property('output_type',kind);ins=[]
 for key in inputs:
  i=unreal.CustomInput();i.set_editor_property('input_name',key);ins.append(i)
 c.set_editor_property('inputs',ins)
 for key,n in inputs.items():E.connect_material_expressions(n,'',c,key)
 c.set_editor_property('code','float f=min(floor(saturate(Age)*16),15);float2 p=(float2(UV.y,UV.x)+float2(fmod(f,4),floor(f/4)))/4;float4 pixel=Texture2DSample(Atlas,AtlasSampler,p);'+code)
 E.connect_material_property(c,'',getattr(unreal.MaterialProperty,output))
E.recompile_material(m);assert A.save_loaded_asset(m)
unreal.log('FILM DUST ATLAS SAVED');unreal.SystemLibrary.quit_editor()
