"""Author the opaque, three-tone projectile surface; no billboard or opacity gradient."""
import unreal
A=unreal.EditorAssetLibrary
E=unreal.MaterialEditingLibrary
path='/Game/GkFx/M_GkFilmProjectile'
m=unreal.load_asset(path)
if not m:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GkFilmProjectile','/Game/GkFx',unreal.Material,unreal.MaterialFactoryNew())
E.delete_all_material_expressions(m)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_OPAQUE)
def custom(code,inputs):
 c=E.create_material_expression(m,unreal.MaterialExpressionCustom)
 c.set_editor_property('code',code);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
 names=[]
 for name,cls in inputs.items():
  i=unreal.CustomInput();i.set_editor_property('input_name',name);names.append(i)
 c.set_editor_property('inputs',names)
 for name,cls in inputs.items():E.connect_material_expressions(E.create_material_expression(m,cls),'',c,name)
 return c
c=custom('''
float f=saturate(dot(normalize(N),normalize(V)));
float3 c=float3(.008,.21,.63);
c=lerp(c,float3(.035,.75,1.4),step(.22,f));
c=lerp(c,float3(1.45,1.7,1.9),step(.48,f));
float t=floor(Clock*24)/24;
float scratch=step(.96,sin(UV.x*25+sin(UV.y*9-t*12)))*step(.4,f);
return lerp(c,float3(.01,.35,.85),scratch*.8);
''',{'N':unreal.MaterialExpressionPixelNormalWS,'V':unreal.MaterialExpressionCameraVectorWS,'UV':unreal.MaterialExpressionTextureCoordinate,'Clock':unreal.MaterialExpressionTime})
E.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
c=custom('return N*(sin(UV.x*43+floor(Clock*24)*.6)*.75);',{'N':unreal.MaterialExpressionVertexNormalWS,'UV':unreal.MaterialExpressionTextureCoordinate,'Clock':unreal.MaterialExpressionTime})
E.connect_material_property(c,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
E.recompile_material(m)
assert A.save_loaded_asset(m)
unreal.log('FILM PROJECTILE SAVED')
