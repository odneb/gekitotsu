"""Opaque three-band material for actual extruded Text3D meshes."""
import unreal
E=unreal.MaterialEditingLibrary
m=unreal.load_asset('/Game/GkUI/M_GkInkType')
if not m:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GkInkType','/Game/GkUI',unreal.Material,unreal.MaterialFactoryNew())
E.delete_all_material_expressions(m)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
p=E.create_material_expression(m,unreal.MaterialExpressionVectorParameter);p.set_editor_property('parameter_name','Ink');p.set_editor_property('default_value',unreal.LinearColor(.98,.84,.61,1))
c=E.create_material_expression(m,unreal.MaterialExpressionCustom);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
i=[]
for name in ['N','V','Ink']:
 x=unreal.CustomInput();x.set_editor_property('input_name',name);i.append(x)
c.set_editor_property('inputs',i)
c.set_editor_property('code','float f=abs(dot(normalize(N),normalize(V))); float band=.16 + .34*step(.3,f)+.50*step(.72,f); return Ink*band;')
for name,cls in [('N',unreal.MaterialExpressionPixelNormalWS),('V',unreal.MaterialExpressionCameraVectorWS)]:E.connect_material_expressions(E.create_material_expression(m,cls),'',c,name)
E.connect_material_expressions(p,'',c,'Ink');E.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
E.recompile_material(m);assert unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('INK TYPE SAVED')
