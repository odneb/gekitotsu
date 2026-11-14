"""Add patchy wetness to an isolated copy of the owned landscape shader.
Run after import_film_ground.py. No terrain geometry or vendor asset is edited.
"""
import unreal
A=unreal.EditorAssetLibrary; E=unreal.MaterialEditingLibrary
root='/Game/GkFx/'
source='/Game/DreamscapeSeries/SharedResources/Materials/MasterMaterials/BaseMaterials/M_Landscape'
m=unreal.load_asset(root+'M_GkWetLandscape') or A.duplicate_asset(source,root+'M_GkWetLandscape')
assert m
if not any(n.get_editor_property('desc')=='GK_SWAMP_WETNESS' for n in E.get_material_expressions(m)):
 def node(cls):return E.create_material_expression(m,cls)
 def scalar(name,value):
  n=node(unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
 pos=node(unreal.MaterialExpressionWorldPosition)
 slope=node(unreal.MaterialExpressionVertexNormalWS)
 noise=node(unreal.MaterialExpressionTextureObjectParameter);noise.set_editor_property('parameter_name','FilmWetNoise');noise.set_editor_property('texture',unreal.load_asset('/Game/DreamscapeSeries/SharedResources/Textures/Noise/T_Noise_Clouds'))
 wet=scalar('FilmWetAmount',1.);cut=scalar('FilmWetThreshold',.29)
 mask=node(unreal.MaterialExpressionCustom);mask.set_editor_property('desc','GK_SWAMP_WETNESS');mask.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
 inputs={'Position':pos,'Slope':slope,'Noise':noise,'Amount':wet,'Threshold':cut};ci=[]
 for name in inputs:
  x=unreal.CustomInput();x.set_editor_property('input_name',name);ci.append(x)
 mask.set_editor_property('inputs',ci)
 mask.set_editor_property('code','''float2 uv=Position.xy/1100.;
float a=Texture2DSample(Noise,NoiseSampler,uv).r;
float b=Texture2DSample(Noise,NoiseSampler,uv*4.7+.37).r;
float n=a*.72+b*.28;
return smoothstep(Threshold-.035,Threshold+.035,n)*smoothstep(.82,.97,Slope.z)*saturate(Amount);''')
 for name,n in inputs.items():assert E.connect_material_expressions(n,'',mask,name)
 for prop,target,factor in [(unreal.MaterialProperty.MP_ROUGHNESS,.055,1.),(unreal.MaterialProperty.MP_BASE_COLOR,None,.30),(unreal.MaterialProperty.MP_NORMAL,None,.85)]:
  prev=E.get_material_property_input_node(m,prop);output=E.get_material_property_input_node_output_name(m,prop);assert prev
  if prop==unreal.MaterialProperty.MP_BASE_COLOR:
   dark=node(unreal.MaterialExpressionMultiply);dark.set_editor_property('const_b',.70);E.connect_material_expressions(prev,output,dark,'A');end=dark
  elif prop==unreal.MaterialProperty.MP_NORMAL:
   end=node(unreal.MaterialExpressionConstant3Vector);end.set_editor_property('constant',unreal.LinearColor(0,0,1,1))
  else:
   end=node(unreal.MaterialExpressionConstant);end.set_editor_property('r',target)
  alpha=mask
  if prop==unreal.MaterialProperty.MP_NORMAL:
   alpha=node(unreal.MaterialExpressionMultiply);alpha.set_editor_property('const_b',factor);E.connect_material_expressions(mask,'',alpha,'A')
  mix=node(unreal.MaterialExpressionLinearInterpolate)
  assert E.connect_material_expressions(prev,output,mix,'A')
  assert E.connect_material_expressions(end,'',mix,'B')
  assert E.connect_material_expressions(alpha,'',mix,'Alpha')
  assert E.connect_material_property(mix,'',prop)
# Keep reruns and fresh reconstructions on the same defaults.
for expression in E.get_material_expressions(m):
 if isinstance(expression,unreal.MaterialExpressionScalarParameter):
  name=str(expression.get_editor_property('parameter_name'))
  if name in {'FilmWetAmount','FilmWetThreshold'}:expression.set_editor_property('default_value',1. if name=='FilmWetAmount' else .29)
E.recompile_material(m);assert A.save_loaded_asset(m)
# Retain the original swamp instance's parameter overrides between the new master and film instance.
base=unreal.load_asset(root+'MI_GkWetLandscapeBase') or A.duplicate_asset('/Game/DreamscapeSeries/DreamscapeDreadmarsh/Materials/MI_Landscape_Swamp',root+'MI_GkWetLandscapeBase')
E.set_material_instance_parent(base,m);assert A.save_loaded_asset(base)
mi=unreal.load_asset(root+'MI_GkFilmLandscape');assert mi
E.set_material_instance_parent(mi,base);assert A.save_loaded_asset(mi,only_if_is_dirty=False)
unreal.SystemLibrary.quit_editor()
