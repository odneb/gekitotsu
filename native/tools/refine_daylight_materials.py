"""Regenerate only fighter, forest and sky look materials; leave combat-energy assets untouched."""
import unreal
E=unreal.MaterialEditingLibrary
A=unreal.EditorAssetLibrary
T=unreal.AssetToolsHelpers.get_asset_tools()
def mat(name,domain=None):
 p='/Game/GkFx/'+name
 m=unreal.load_asset(p)
 if m: E.delete_all_material_expressions(m)
 else: m=T.create_asset(name,'/Game/GkFx',unreal.Material,unreal.MaterialFactoryNew())
 m.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
 if domain: m.set_editor_property('material_domain',domain)
 return m
def node(m,cls): return E.create_material_expression(m,cls)
def vector(m,name,value):
 n=node(m,unreal.MaterialExpressionVectorParameter); n.set_editor_property('parameter_name',name); n.set_editor_property('default_value',unreal.LinearColor(*value,1)); return n
def custom(m,inputs,code,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
 n=node(m,unreal.MaterialExpressionCustom); n.set_editor_property('output_type',kind)
 ns=[]
 for name in inputs:
  p=unreal.CustomInput(); p.set_editor_property('input_name',name); ns.append(p)
 n.set_editor_property('inputs',ns); n.set_editor_property('code',code)
 for name,other in inputs.items():
  assert E.connect_material_expressions(other,'',n,name)
 return n
def save(m):
 E.recompile_material(m); assert A.save_loaded_asset(m)
# Three designed value groups, a narrow directional edge, and a small hard highlight.
m=mat('M_GkFilmFighter'); m.set_editor_property('used_with_skeletal_mesh',True)
# Reuse the mannequin's authored normal/AO detail instead of flattening it away.
normal=node(m,unreal.MaterialExpressionTextureSampleParameter2D);normal.set_editor_property('parameter_name','BodyNormal');normal.set_editor_property('texture',unreal.load_asset('/Game/Characters/Mannequins/Textures/Manny/T_Manny_01_BN'));normal.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
normal_scale=node(m,unreal.MaterialExpressionMultiply)
normal_scale.set_editor_property('const_b',.5)
normal_weights=node(m,unreal.MaterialExpressionConstant3Vector);normal_weights.set_editor_property('constant',unreal.LinearColor(.45,.45,1.,1.))
E.connect_material_expressions(normal,'RGB',normal_scale,'A');E.connect_material_expressions(normal_weights,'',normal_scale,'B')
E.connect_material_property(normal_scale,'',unreal.MaterialProperty.MP_NORMAL)
occlusion=node(m,unreal.MaterialExpressionTextureSampleParameter2D);occlusion.set_editor_property('parameter_name','BodyOcclusion');occlusion.set_editor_property('texture',unreal.load_asset('/Game/Characters/Mannequins/Textures/Manny/T_Manny_01_MRA'));occlusion.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)

i={'Normal':node(m,unreal.MaterialExpressionPixelNormalWS),'View':node(m,unreal.MaterialExpressionCameraVectorWS),'Occlusion':occlusion}
for name,v in [('Paint',(.5,.05,.04)),('Shadow',(.09,.015,.025)),('Key',(.4,-.5,.75)),('KeyTint',(1,.93,.8)),('Rim',(1,.94,.75)),('BackKey',(.65,-.65,.4))]: i[name]=vector(m,name,v)
for name,value in [('RimPower',3.8),('RimWidth',1.1),('ShoulderStrength',.75),('KeyShadowCut',-.05),('KeyHighlightCut',.60),('SpecularStrength',.12)]:
 n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);i[name]=n
n=custom(m,i,'''
float3 N=normalize(Normal), V=normalize(View), L=normalize(Key.rgb);
float nl=dot(N,L), nv=saturate(dot(N,V));
// Pixel-sized transitions keep the drawn value groups crisp without crawling edges.
float aa=max(fwidth(nl)*.8,.012);
float lit=smoothstep(KeyShadowCut-aa,KeyShadowCut+aa,nl), high=smoothstep(KeyHighlightCut-aa,KeyHighlightCut+aa,nl);
float3 middle=lerp(Shadow.rgb,Paint.rgb*KeyTint.rgb,.56);
float3 c=lerp(Shadow.rgb,middle,smoothstep(-.46-aa,-.46+aa,nl));
c=lerp(c,Paint.rgb*KeyTint.rgb,lit);
c=lerp(c,Paint.rgb*KeyTint.rgb*1.18,high*.65);
// Independent backlight direction: broad enough to read at the gameplay camera.
float side=dot(N,normalize(BackKey.rgb));
float edgeAA=max(fwidth(nv),.014);
float width=lerp(.18,.36,smoothstep(-.2,.7,side))*RimWidth;
float rim=(1-smoothstep(width-edgeAA,width+edgeAA,nv))*smoothstep(.04,.25,side);
// A colored shoulder leading into a narrow bright edge retains the costume hue.
float shoulder=(1-smoothstep(width+.05,width+.15,nv))*smoothstep(.04,.25,side);
c+=lerp(Paint.rgb*Rim.rgb,Rim.rgb,.25)*shoulder*ShoulderStrength;
c=lerp(c,Rim.rgb*RimPower,rim);
float spec=smoothstep(.982,.988,dot(N,normalize(L+V)))*lit;
return lerp(c,KeyTint.rgb*1.65,spec*SpecularStrength)*(.78+.22*saturate(Occlusion.b));
'''); E.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR); save(m)
# Environment: preserve actual cast shadows, quantize reflected illumination, cool shadow planes.
m=mat('M_GkFilmForest',unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_DOF)
i={}
for name,tid in [('Scene',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),('Albedo',unreal.SceneTextureId.PPI_BASE_COLOR),('Depth',unreal.SceneTextureId.PPI_SCENE_DEPTH),('Normal',unreal.SceneTextureId.PPI_WORLD_NORMAL),('Model',unreal.SceneTextureId.PPI_SHADING_MODEL_ID)]:
 n=node(m,unreal.MaterialExpressionSceneTexture); n.set_editor_property('scene_texture_id',tid); i[name]=n
for name,value in [('CelStrength',.35),('ShadowThreshold',.32),('HighlightThreshold',.72),('DebugIllumination',0.),('CelEdgeSoftness',.08),('CreaseStrength',.12)]:
 n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);i[name]=n
n=custom(m,i,'''
// Unlit fighters, energy and sky already own their value design.
if(Model.r<.5)return Scene.rgb;
float3 w=float3(.2126,.7152,.0722);
float a=dot(max(Albedo.rgb,0),w), s=dot(max(Scene.rgb,0),w);
float illum=s/max(a,.045);
float aa=clamp(fwidth(illum)*.8,.015,CelEdgeSoftness);
if(DebugIllumination>.5) {
 if(a<.03)return float3(0,0,0);
 return illum<.32?float3(0,0,1):illum<.72?float3(0,1,0):illum<1.2?float3(1,1,0):illum<2.3?float3(1,.2,0):float3(1,0,1);
}
float light=smoothstep(ShadowThreshold-aa,ShadowThreshold+aa,illum), top=smoothstep(HighlightThreshold-aa,HighlightThreshold+aa,illum);
float high2=smoothstep(1.7-aa,1.7+aa,illum), high3=smoothstep(3.5-aa,3.5+aa,illum);
float bands=.18+.40*light+.55*top+1.17*high2+2.0*high3;
float3 tint=lerp(float3(.70,.73,1.12),float3(1.06,1.00,.94),light);
float3 c=Scene.rgb*(bands/max(illum,.05))*tint;
float mask=smoothstep(.03,.08,a)*(1-smoothstep(4200,12000,Depth.r));
mask*=1-smoothstep(6.,10.,illum);
float2 uv=GetDefaultSceneTextureUV(Parameters,14);
float2 px=View.BufferSizeAndInvSize.zw;
float3 n0=Normal.rgb;
float3 nx=SceneTextureLookup(uv+float2(px.x,0),8,false).rgb;
float3 ny=SceneTextureLookup(uv+float2(0,px.y),8,false).rgb;
float crease=smoothstep(.28,.48,max(length(n0-nx),length(n0-ny)));
float nearMask=1-smoothstep(2800,6500,Depth.r);
return max(lerp(Scene.rgb,c,mask*CelStrength)*(1-crease*CreaseStrength*nearMask*mask),0);
'''); E.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR); save(m)
# Reuse the stage's cloud mask on its existing sky dome with six animated palettes.
m=mat('M_GkFilmSky'); m.set_editor_property('is_sky',True); m.set_editor_property('two_sided',True)
i={'Position':node(m,unreal.MaterialExpressionWorldPosition),'Camera':node(m,unreal.MaterialExpressionCameraPositionWS),'Clock':node(m,unreal.MaterialExpressionTime)}
for name,v in [('Zenith',(.04,.28,.68)),('Horizon',(.55,.76,.82)),('CloudLight',(1.1,1.05,.9)),('CloudShadow',(.40,.53,.68)),('SunDir',(.5,-.4,.7)),('MoonDir',(.93,.35,.45))]: i[name]=vector(m,name,v)
n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name','Night');n.set_editor_property('default_value',0.);i['Night']=n
for name,value in [('CloudVerticalScale',1.35),('CloudVerticalOffset',.88),('CloudShadowLift',.22)]:
 n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);i[name]=n
n=node(m,unreal.MaterialExpressionTextureObjectParameter);n.set_editor_property('parameter_name','CloudMask');n.set_editor_property('texture',unreal.load_asset('/Game/DreamscapeSeries/SharedResources/Textures/Sky/T_CloudsOffset_M'));i['CloudMask']=n
n=node(m,unreal.MaterialExpressionTextureObjectParameter);n.set_editor_property('parameter_name','MoonTexture');n.set_editor_property('texture',unreal.load_asset('/Game/DreamscapeSeries/SharedResources/Textures/Sky/T_Moon'));i['MoonTexture']=n
n=custom(m,i,'''
float3 ray=normalize(Position-Camera);
float2 uv=float2(atan2(ray.y,ray.x)/6.283185+.5,acos(clamp(ray.z,-1,1))/3.141593);
float height=saturate(ray.z*1.8+.12);
float3 c=lerp(Horizon.rgb,Zenith.rgb,smoothstep(0,.85,height));
float noise=Texture2DSample(CloudMask,CloudMaskSampler,float2(uv.x*1.5+.13+Clock*.0007,clamp(CloudVerticalOffset-ray.z*CloudVerticalScale,.03,.97))).r;
float mask=smoothstep(.09,.13,noise)*smoothstep(-.1,.2,ray.z);
float cloudValue=pow(saturate(noise),3.0);
float light=CloudShadowLift*smoothstep(.12,.18,cloudValue)+.38*smoothstep(.38,.43,cloudValue)+(1-CloudShadowLift-.38)*smoothstep(.70,.76,cloudValue);
c=lerp(c,lerp(CloudShadow.rgb,CloudLight.rgb,light),mask);
float disc=smoothstep(.9988,.9992,dot(ray,normalize(SunDir.rgb)));
c=lerp(c,CloudLight.rgb*2.5,disc*(1-Night));
float moon=smoothstep(.99968,.99972,dot(ray,normalize(MoonDir.rgb)));
float3 md=normalize(MoonDir.rgb), mr=normalize(cross(float3(0,0,1),md)), mu=cross(md,mr);
float2 moonUV=float2(dot(ray,mr),-dot(ray,mu))/.049+.5;
float crater=Texture2DSample(MoonTexture,MoonTextureSampler,float2(.100,.084)+saturate(moonUV)*.814).r;
float3 moonColor=lerp(float3(.20,.40,.75),float3(.78,1.05,1.45),crater);
c=lerp(c,moonColor,moon*Night*(1-mask*.8));
float2 cells=floor(uv*float2(1400,700));
float h=frac(sin(dot(cells,float2(127.1,311.7)))*43758.5453);
float stars=step(.9994,h)*smoothstep(.05,.25,ray.z)*(1-mask);
return c+float3(.5,.65,1)*stars*Night;
''');E.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);save(m)
unreal.log('FILM SKY SAVED')


unreal.SystemLibrary.quit_editor()
