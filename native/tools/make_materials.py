import unreal
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
MB = '/Game/MeshBites_Stylized_Anime_Shader/Ressources/Master_Materials/'
OUT = '/Game/GkMat'
def LC(r,g,b): return unreal.LinearColor(r,g,b,1.0)
def mi(name, parent, vec={}, sca={}, sw={}):
    path = OUT + '/' + name
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else at.create_asset(name, OUT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(m, unreal.load_asset(MB + parent))
    for k,v in sw.items(): mel.set_material_instance_static_switch_parameter_value(m, k, v)
    for k,v in vec.items(): mel.set_material_instance_vector_parameter_value(m, k, LC(*v))
    for k,v in sca.items(): mel.set_material_instance_scalar_parameter_value(m, k, v)
    mel.update_material_instance(m)
    eal.save_asset(path, False)
    return m
def body(name, col, sh, parent="MM_CellShader_Standard", sca={}, sw={}, vec={}):
    return mi(name, parent, dict({'Base  color': col, 'Shadow_Base': sh}, **vec), sca, dict({'Use Rimlight': True, 'Use Diffuse': False, 'Use Inline': False, 'Use Highlights': False}, **sw))
P = {}
P['P1A'] = body('MI_Gk_P1_A', (0.80,0.05,0.04), (0.33,0.015,0.07), sw={'Use RGB Mask': True})
P['P1B'] = body('MI_Gk_P1_B', (0.86,0.84,0.78), (0.42,0.42,0.56), sw={'Use RGB Mask': True})
P['P2A'] = body('MI_Gk_P2_A', (0.05,0.22,0.85), (0.03,0.05,0.38), sw={'Use RGB Mask': True})
P['P2B'] = body('MI_Gk_P2_B', (0.86,0.84,0.78), (0.42,0.42,0.56), sw={'Use RGB Mask': True})
P['L1A'] = body('MI_Gk_Lit_A', (0.80,0.05,0.04), (0.33,0.015,0.07), 'Material_Lit/MM_CellShader_Standard_Lit')
P['L1B'] = body('MI_Gk_Lit_B', (0.86,0.84,0.78), (0.42,0.42,0.56), 'Material_Lit/MM_CellShader_Standard_Lit')
OL = mi('MI_Gk_Outline', 'MM_OutlinesOverlay', {'Outlines Color': (0.01,0.01,0.015)}, {'Outlines_Thickness': 1.0}, {'Flip culling': False})
OL2 = mi('MI_Gk_Outline_Inv', 'MM_OutlinesOverlay', {'Outlines Color': (0.01,0.01,0.015)}, {'Outlines_Thickness': 1.0}, {'Flip culling': False, 'Invert Opacity': False})
log('made %s' % list(P))
for n in ['Outlines_Thickness','Brightness']: log('OL default %s = %s' % (n, mel.get_material_default_scalar_parameter_value(unreal.load_asset(MB+'MM_OutlinesOverlay'), n)))
for n in ['Light Amount','Light Opacity','Shadow Amount','Shadow Opacity','Shadow: Sharpness','Light: Sharpness','Rimlight Thickness']: log('STD default %s = %s' % (n, mel.get_material_default_scalar_parameter_value(unreal.load_asset(MB+'MM_CellShader_Standard'), n)))
ell = unreal.EditorLevelLibrary
for a in ell.get_all_level_actors():
    if a.get_actor_label().startswith('GkT_'): a.destroy_actor()
log('DONE materials final')
