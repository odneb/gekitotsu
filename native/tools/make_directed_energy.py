"""Hard-edged white-core energy on the existing beam mesh and charge sphere."""
import unreal
E=unreal.MaterialEditingLibrary
A=unreal.EditorAssetLibrary
path='/Game/GkFx/M_GkDirectedEnergy'
m=unreal.load_asset(path) or unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GkDirectedEnergy','/Game/GkFx',unreal.Material,unreal.MaterialFactoryNew())
E.delete_all_material_expressions(m);m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
def node(cls):return E.create_material_expression(m,cls)
args={'N':node(unreal.MaterialExpressionPixelNormalWS),'V':node(unreal.MaterialExpressionCameraVectorWS),'UV':node(unreal.MaterialExpressionTextureCoordinate),'Clock':node(unreal.MaterialExpressionTime)}
c=node(unreal.MaterialExpressionCustom);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
ins=[]
for key in args:
 i=unreal.CustomInput();i.set_editor_property('input_name',key);ins.append(i)
c.set_editor_property('inputs',ins)
c.set_editor_property('code',r'''
float t=floor(Clock*24)/24;
float facing=abs(dot(normalize(N),normalize(V)));
float serration=.045*sin(UV.y*97-t*40)+.025*sin(UV.y*173+t*50);
float core=step(.45+serration,facing);
float edge=step(.14,facing);
float3 shell=lerp(float3(.008,.035,.22),float3(.015,.62,1.8),edge);
float scratch=step(.978,sin(UV.x*97+UV.y*17-t*36))*step(.65,frac(UV.y*7+t*2));
return lerp(lerp(shell,float3(2.8,3.1,3.3),core),float3(.01,.28,.85),scratch*.85);
''')
for key,n in args.items():E.connect_material_expressions(n,'',c,key)
E.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
E.recompile_material(m);A.save_loaded_asset(m)
unreal.log('GK DIRECTED ENERGY SAVED')
