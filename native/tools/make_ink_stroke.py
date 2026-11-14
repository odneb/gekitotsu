"""Hard masked, instanced cel brush strokes. No translucent radial gradients."""
import unreal
E=unreal.MaterialEditingLibrary
path='/Game/GkFx/M_GkInkStroke'
m=unreal.load_asset(path) or unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GkInkStroke','/Game/GkFx',unreal.Material,unreal.MaterialFactoryNew())
E.delete_all_material_expressions(m)
m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
m.set_editor_property('two_sided',True)
m.set_editor_property('used_with_instanced_static_meshes',True)
def node(cls):return E.create_material_expression(m,cls)
uv=node(unreal.MaterialExpressionTextureCoordinate)
c=node(unreal.MaterialExpressionCustom);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
i=unreal.CustomInput();i.set_editor_property('input_name','UV');c.set_editor_property('inputs',[i]);E.connect_material_expressions(uv,'',c,'UV')
c.set_editor_property('code',r'''
float along=UV.y;
float across=abs(UV.x-.5)*2;
float taper=pow(saturate(sin(along*3.14159265)),.5);
float tooth=.025*sin(along*43)+.015*sin(along*91);
float edge=step(across,taper*.92+tooth);
float scratch=step(.965,sin(UV.x*113+along*11))*step(.42,along);
return edge*(1-scratch);
''')
E.connect_material_property(c,'',unreal.MaterialProperty.MP_OPACITY_MASK)
cols=[]
for k in range(3):
 n=node(unreal.MaterialExpressionPerInstanceCustomData);n.set_editor_property('data_index',k);cols.append(n)
a=node(unreal.MaterialExpressionAppendVector);E.connect_material_expressions(cols[0],'',a,'A');E.connect_material_expressions(cols[1],'',a,'B')
b=node(unreal.MaterialExpressionAppendVector);E.connect_material_expressions(a,'',b,'A');E.connect_material_expressions(cols[2],'',b,'B')
assert E.connect_material_property(b,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
E.recompile_material(m);unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('GK INK STROKE SAVED')
