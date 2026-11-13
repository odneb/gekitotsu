"""Generate the combat director's Mac-compatible post-process material. No engine modifications."""
import unreal
E=unreal.MaterialEditingLibrary
A=unreal.EditorAssetLibrary
path='/Game/GkFx/M_GkCombatPrint'
m=unreal.load_asset(path)
if not m:m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GkCombatPrint','/Game/GkFx',unreal.Material,unreal.MaterialFactoryNew())
E.delete_all_material_expressions(m)
m.set_editor_property('material_domain',unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
m.set_editor_property('blendable_priority',50)
def node(cls):return E.create_material_expression(m,cls)
def scalar(name,value):
 n=node(unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);return n
def vector(name,value):
 n=node(unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',unreal.LinearColor(*value));return n
def scene(kind):
 n=node(unreal.MaterialExpressionSceneTexture);n.set_editor_property('scene_texture_id',kind);return n
args={'Scene':scene(unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),'Stencil':scene(unreal.SceneTextureId.PPI_CUSTOM_STENCIL),'Depth':scene(unreal.SceneTextureId.PPI_SCENE_DEPTH),'CustomDepth':scene(unreal.SceneTextureId.PPI_CUSTOM_DEPTH)}
for key,default in [('Mode',0),('Amount',0),('Dim',0),('Lines',0),('Beat',0)]:args[key]=scalar(key,default)
args['Accent']=vector('Accent',(.8,.06,.025,1));args['Focus']=vector('Focus',(.5,.5,0,0))
c=node(unreal.MaterialExpressionCustom);c.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for key in args:
 i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
c.set_editor_property('inputs',inputs)
c.set_editor_property('code',r'''
float3 src=Scene.rgb;
float person=step(.5,Stencil.r)*step(CustomDepth.r-3,Depth.r);
float lum=dot(src,float3(.2126,.7152,.0722));
float2 uv=GetViewportUV(Parameters);
float2 pixel=Parameters.SvPosition.xy;
float2 d=uv-Focus.xy;d.x*=1.77778;
float radius=length(d);float angle=atan2(d.y,d.x);
float spoke=pow(saturate(sin(angle*29+floor(Beat)*.37)),22);
spoke=max(spoke*.85,pow(saturate(sin(angle*67+.6)),50)*.35);
float strokes=spoke*smoothstep(.23,.65,radius)*Lines*(1-person);
float3 paper=float3(.93,.89,.78),black=float3(.008,.012,.018);
float threshold=smoothstep(.28,.32,lum);
float3 ink=lerp(black,paper,threshold);
// Figures remain illustrated volumes, with a black silhouette and a few cut-white highlights.
ink=lerp(ink,lerp(black,paper,step(.64,lum)),person);
float3 result=src*(1-Dim*(1-person));
if(Mode>.5){
 float3 graphic=ink;
 if(Mode>1.5 && Mode<2.5){
  graphic=lerp(black,paper,threshold*.8);
  graphic=lerp(graphic,lerp(black,Accent.rgb*1.15+.04,step(.26,lum)),person);
 }
 if(Mode>2.5 && Mode<3.5)graphic=lerp(paper,black,threshold);
 if(Mode>3.5){
  float dots=step(.45,length(frac(pixel/5)-.5));
  float tone=lerp(.55,.9,step(.36,lum));
  graphic=lerp(paper*lerp(tone,1,dots),ink,person);
  float edge=saturate(length(float2(ddx(lum),ddy(lum)))*5);
  graphic=lerp(graphic,black,edge*.65);
 }
 result=lerp(result,graphic,Amount);
}
result=lerp(result,Mode>2.5&&Mode<3.5?paper:black,strokes*.75);
return max(result,0);
''')
for key,n in args.items():E.connect_material_expressions(n,'',c,key)
E.connect_material_property(c,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
E.recompile_material(m);A.save_loaded_asset(m)
unreal.log('GK COMBAT PRINT SAVED')
