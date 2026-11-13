"""Native opaque print surfaces and a restrained screen-print finish for the menu set."""
import unreal
E=unreal.MaterialEditingLibrary
A=unreal.EditorAssetLibrary
def mat(name,domain=unreal.MaterialDomain.MD_SURFACE):
 m=unreal.load_asset('/Game/GkUI/'+name)
 if not m:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,'/Game/GkUI',unreal.Material,unreal.MaterialFactoryNew())
 E.delete_all_material_expressions(m);m.set_editor_property('material_domain',domain)
 if domain==unreal.MaterialDomain.MD_SURFACE:m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 return m
def node(m,cls):return E.create_material_expression(m,cls)
def param(m,key,value):
 n=node(m,unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',unreal.LinearColor(*value,1));return n
def code(m,args,src):
 c=node(m,unreal.MaterialExpressionCustom);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3);ins=[]
 for key in args:
  q=unreal.CustomInput();q.set_editor_property('input_name',key);ins.append(q)
 c.set_editor_property('inputs',ins);c.set_editor_property('code',src)
 for key,n in args.items():E.connect_material_expressions(n,'',c,key)
 E.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 E.recompile_material(m);assert A.save_loaded_asset(m)
m=mat('M_GkPrintSurface')
code(m,{'N':node(m,unreal.MaterialExpressionPixelNormalWS),'V':node(m,unreal.MaterialExpressionCameraVectorWS),'UV':node(m,unreal.MaterialExpressionTextureCoordinate),'Tint':param(m,'Tint',(.86,.68,.42))},'''
float h=frac(sin(dot(floor(UV*1700),float2(127.1,311.7)))*43758.5453);
float face=step(.65,abs(dot(normalize(N),normalize(V))));
return lerp(float3(.008,.006,.004),Tint.rgb*(.955+.045*h),face);
''')
m=mat('M_GkPrintFinish',unreal.MaterialDomain.MD_POST_PROCESS);m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
n=node(m,unreal.MaterialExpressionSceneTexture);n.set_editor_property('scene_texture_id',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
code(m,{'Scene':n},'''
float2 p=Parameters.SvPosition.xy;
float grain=frac(sin(dot(floor(p),float2(12.9898,78.233)))*43758.5453);
return Scene.rgb*(.965+.045*grain);
''')
unreal.log('PRINT SET MATERIALS SAVED')
