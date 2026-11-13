"""Build the film-look materials in an isolated Python commandlet."""
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
i={'Normal':node(m,unreal.MaterialExpressionPixelNormalWS),'View':node(m,unreal.MaterialExpressionCameraVectorWS)}
for name,v in [('Paint',(.5,.05,.04)),('Shadow',(.09,.015,.025)),('Key',(.4,-.5,.75)),('KeyTint',(1,.93,.8)),('Rim',(1,.94,.75))]: i[name]=vector(m,name,v)
n=custom(m,i,'''
float3 N=normalize(Normal), V=normalize(View), L=normalize(Key.rgb);
float nl=dot(N,L), nv=saturate(dot(N,V));
float lit=smoothstep(-0.03,0.005,nl), high=smoothstep(.62,.66,nl);
float3 c=lerp(Shadow.rgb,Paint.rgb*KeyTint.rgb,lit);
c=lerp(c,Paint.rgb*KeyTint.rgb*1.2,high*.65);
// The light-facing silhouette is a narrow ink-like cut, not a soft Fresnel glow.
float edge=1-smoothstep(.22,.25,nv);
float lightSide=smoothstep(.05,.12,dot(N,normalize(L-V*.55)));
float rim=edge*lightSide;
c=lerp(c,Rim.rgb*1.4,rim);
float spec=smoothstep(.991,.996,dot(N,normalize(L+V)))*lit;
return lerp(c,KeyTint.rgb*1.15,spec*.18);
'''); E.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR); save(m)
# Environment: preserve actual cast shadows, quantize reflected illumination, cool shadow planes.
m=mat('M_GkFilmForest',unreal.MaterialDomain.MD_POST_PROCESS)
m.set_editor_property('blendable_location',unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)
i={}
for name,tid in [('Scene',unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),('Albedo',unreal.SceneTextureId.PPI_BASE_COLOR),('Depth',unreal.SceneTextureId.PPI_SCENE_DEPTH),('Normal',unreal.SceneTextureId.PPI_WORLD_NORMAL)]:
 n=node(m,unreal.MaterialExpressionSceneTexture); n.set_editor_property('scene_texture_id',tid); i[name]=n
n=custom(m,i,'''
float3 w=float3(.2126,.7152,.0722);
float a=dot(max(Albedo.rgb,0),w), s=dot(max(Scene.rgb,0),w);
float illum=s/max(a,.045);
float light=smoothstep(.26,.29,illum), top=smoothstep(.65,.69,illum);
float bands=.17+.32*light+.33*top;
float3 tint=lerp(float3(.72,.86,1.04),float3(1.05,1.02,.87),light);
float3 c=Scene.rgb*(bands/max(illum,.05))*tint;
float mask=smoothstep(.03,.08,a)*(1-smoothstep(4200,12000,Depth.r));
mask*=1-smoothstep(1.2,2.3,illum);
float2 uv=GetDefaultSceneTextureUV(Parameters,14);
float2 px=View.BufferSizeAndInvSize.zw;
float3 n0=Normal.rgb;
float3 nx=SceneTextureLookup(uv+float2(px.x,0),8,false).rgb;
float3 ny=SceneTextureLookup(uv+float2(0,px.y),8,false).rgb;
float crease=smoothstep(.28,.48,max(length(n0-nx),length(n0-ny)));
float nearMask=1-smoothstep(2800,6500,Depth.r);
return max(lerp(Scene.rgb,c,mask*.88)*(1-crease*.30*nearMask*mask),0);
'''); E.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR); save(m)
# Sharp tapered flecks replace Gaussian discs for energy, rain, and impact strokes.
m=mat('M_GkFilmSpark'); m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT); m.set_editor_property('two_sided',True); m.set_editor_property('used_with_instanced_static_meshes',True)
i={'UV':node(m,unreal.MaterialExpressionTextureCoordinate)}
for idx,name in enumerate(['Red','Green','Blue','Alpha']):
 n=node(m,unreal.MaterialExpressionPerInstanceCustomData); n.set_editor_property('data_index',idx); i[name]=n
n=custom(m,i,'return float3(Red,Green,Blue);'); E.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
n=custom(m,i,'float2 p=abs(UV-.5)*2; return step(p.x*.62+p.y,1)*Alpha;',unreal.CustomMaterialOutputType.CMOT_FLOAT1); E.connect_material_property(n,'',unreal.MaterialProperty.MP_OPACITY); save(m)
unreal.log('FILM MATERIALS SAVED')
# Three dimensional energy: view-dependent value groups on an actual mesh, with racing ink streaks.
m=mat('M_GkFilmEnergy'); m.set_editor_property('used_with_nanite',True)
i={'N':node(m,unreal.MaterialExpressionPixelNormalWS),'V':node(m,unreal.MaterialExpressionCameraVectorWS),'UV':node(m,unreal.MaterialExpressionTextureCoordinate),'Clock':node(m,unreal.MaterialExpressionTime)}
n=custom(m,i,'''
float t=floor(Clock*30)/30, facing=abs(dot(normalize(N),normalize(V)));
float swirl=.05*sin(UV.x*53+UV.y*71-t*32)+.025*sin(UV.x*117-UV.y*21+t*19);
float core=smoothstep(.84+swirl,.855+swirl,facing);
float halo=smoothstep(.10,.125,facing);
float3 c=lerp(float3(.025,.24,1.2),float3(.08,1.15,2.3),halo);
c=lerp(c,float3(1.8,2.1,2.3),core);
float streak=step(.965,sin(UV.x*81+sin(UV.y*41-t*38)*.35))*step(.25,facing);
float dash=step(.12,frac(UV.y*5-t*3));
return lerp(c,float3(.08,.58,1.65),streak*dash*.85);
'''); E.connect_material_property(n,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
amp=node(m,unreal.MaterialExpressionScalarParameter);amp.set_editor_property('parameter_name','FlowAmplitude');amp.set_editor_property('default_value',1.0)
wpo=custom(m,{'N':node(m,unreal.MaterialExpressionVertexNormalWS),'UV':node(m,unreal.MaterialExpressionTextureCoordinate),'Clock':node(m,unreal.MaterialExpressionTime),'Amplitude':amp},'float t=floor(Clock*30)/30; return N*Amplitude*(sin(UV.y*83-t*45)*.65+sin(UV.y*177-t*67)*.35);')
E.connect_material_property(wpo,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET);save(m)
unreal.log('FILM ENERGY SAVED')
# Reuse the stage's cloud mask on its existing sky dome with six animated palettes.
m=mat('M_GkFilmSky'); m.set_editor_property('is_sky',True); m.set_editor_property('two_sided',True)
i={'Position':node(m,unreal.MaterialExpressionWorldPosition),'Camera':node(m,unreal.MaterialExpressionCameraPositionWS),'Clock':node(m,unreal.MaterialExpressionTime)}
for name,v in [('Zenith',(.04,.28,.68)),('Horizon',(.55,.76,.82)),('CloudLight',(1.1,1.05,.9)),('CloudShadow',(.40,.53,.68)),('SunDir',(.5,-.4,.7)),('MoonDir',(.93,.35,.45))]: i[name]=vector(m,name,v)
n=node(m,unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name','Night');n.set_editor_property('default_value',0.);i['Night']=n
n=node(m,unreal.MaterialExpressionTextureObjectParameter);n.set_editor_property('parameter_name','CloudMask');n.set_editor_property('texture',unreal.load_asset('/Game/DreamscapeSeries/SharedResources/Textures/Sky/T_CloudsOffset_M'));i['CloudMask']=n
n=node(m,unreal.MaterialExpressionTextureObjectParameter);n.set_editor_property('parameter_name','MoonTexture');n.set_editor_property('texture',unreal.load_asset('/Game/DreamscapeSeries/SharedResources/Textures/Sky/T_Moon'));i['MoonTexture']=n
n=custom(m,i,'''
float3 ray=normalize(Position-Camera);
float2 uv=float2(atan2(ray.y,ray.x)/6.283185+.5,acos(clamp(ray.z,-1,1))/3.141593);
float height=saturate(ray.z*1.8+.12);
float3 c=lerp(Horizon.rgb,Zenith.rgb,smoothstep(0,.85,height));
float noise=Texture2DSample(CloudMask,CloudMaskSampler,float2(uv.x*1.5+.13+Clock*.0007,clamp(.95-ray.z*1.65,.03,.97))).r;
float mask=smoothstep(.48,.51,noise)*smoothstep(-.1,.2,ray.z);
float light=.35*smoothstep(.52,.55,noise)+.65*smoothstep(.65,.69,noise);
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

# This small deforming mesh uses the regular static-mesh path and an authored material slot.
beam=unreal.load_asset('/Game/GkFx/SM_GkFilmBeam')
if beam:
 settings=beam.get_editor_property('nanite_settings');settings.enabled=False;beam.set_editor_property('nanite_settings',settings)
 beam.set_material(0,unreal.load_asset('/Game/GkFx/M_GkFilmEnergy'))
 assert A.save_loaded_asset(beam)
