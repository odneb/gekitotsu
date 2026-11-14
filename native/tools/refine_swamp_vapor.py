"""Build a cel-lit child of FluidNinja's volume renderer without editing the vendor assets.
The LIVE-2 density and velocity fields remain the source of the visible vapor.
"""
import unreal
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary
root='/Game/GkFx/'
source='/Game/FluidNinjaLive/OutputMaterials/Base/M_NinjaOutput_BaseMaterial_SMOKEVOLUME'
m=unreal.load_asset(root+'M_GkCelMistVolume')
if not m:m=A.duplicate_asset(source,root+'M_GkCelMistVolume')
assert m
# Keep vapor out of opaque cel quantization; TSR composes this translucency separately.
m.set_editor_property("translucency_pass",unreal.MaterialTranslucencyPass.MTP_AFTER_DOF)
for n in E.get_material_expressions(m):
 if not isinstance(n,unreal.MaterialExpressionCustom):continue
 code=n.get_editor_property('code')
 if 'color+=absorptionfrommarch' not in code or 'GK_CEL_VAPOR' in code:continue
 # Retain FluidNinja's raymarch, self-shadowing and interaction; simplify only lighting values.
 if '*lightvisibility*LightColor' in code:
  code=code.replace('*lightvisibility*LightColor','*(.22+.43*smoothstep(.32,.37,lightvisibility)+.35*smoothstep(.74,.79,lightvisibility))*LightColor')
  code+='\n// GK_CEL_VAPOR: three illumination tiers, original simulation and density preserved.\n'
  n.set_editor_property('code',code)
E.recompile_material(m);assert A.save_loaded_asset(m)
base=unreal.load_asset(root+'MI_GkCelMistBase') or A.duplicate_asset('/Game/FluidNinjaLive/OutputMaterials/SVOL/MI_SVOL_DirLit_Noisy_Sand_Small',root+'MI_GkCelMistBase')
E.set_material_instance_parent(base,m);assert A.save_loaded_asset(base)
mi=unreal.load_asset(root+'MI_GkFilmMist');assert mi
E.set_material_instance_parent(mi,base)
for name,value in {'MaxViewSteps':128.,'Density':4.,'DensityPow':.5,'Extrude':.045,'ExtrudeSoften':.01,'XBackgroundDensity':.012,'XBackgroundDensityNoise':.18,'NoiseDisplace':4.0,'NoiseTiling':4.0,'FlowStrength':.35,'FadeDistance':5000.,'AtmoLightDesaturate':.65,'DensityOffset':-.018,'Shadow':.75,'LightIntensity':.18}.items():
 E.set_material_instance_scalar_parameter_value(mi,name,value)
for name,value in {'Color1':(.10,.16,.30),'Color2':(.35,.48,.70),'AmbientColor':(.07,.10,.18)}.items():
 E.set_material_instance_vector_parameter_value(mi,name,unreal.LinearColor(*value,1.))
assert A.save_loaded_asset(mi)
unreal.SystemLibrary.quit_editor()
