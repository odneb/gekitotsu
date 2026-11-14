"""Reuse Dreamscape's authored foliage shaders, wind and textures with film-oriented colors.
Child instances keep the vendor materials unchanged and are applied only in this arena.
"""
import unreal
A=unreal.EditorAssetLibrary;E=unreal.MaterialEditingLibrary;T=unreal.AssetToolsHelpers.get_asset_tools()
variants=[
 ('OakLeaves_01','MI_GkFilmOak',(.105,.185,.045),(.025,.080,.075),(.28,.34,.07),.28),
 ('Willow_Leaves_02','MI_GkFilmWillow',(.13,.23,.035),(.04,.12,.105),(.32,.42,.09),.20),
 ('Cypress_Leaves_01','MI_GkFilmCypress',(.055,.12,.03),(.012,.05,.045),(.20,.28,.065),.42),
]
for source,name,top,bottom,subsurface,influence in variants:
 parent=unreal.load_asset('/Game/DreamscapeSeries/DreamscapeDreadmarsh/Materials/Foliage/Trees/MI_DreadMarsh_'+source);assert parent
 m=unreal.load_asset('/Game/GkFx/'+name) or T.create_asset(name,'/Game/GkFx',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
 E.set_material_instance_parent(m,parent)
 for param,value in [('Top Tint Color',top),('Bottom Tint Color',bottom),('Subsurface Color',subsurface)]:
  E.set_material_instance_vector_parameter_value(m,param,unreal.LinearColor(*value,1.))
 for param,value in [('Normal Strength',.55),('Subsurface Influence',influence)]:E.set_material_instance_scalar_parameter_value(m,param,value)
 assert A.save_loaded_asset(m)
unreal.SystemLibrary.quit_editor()
