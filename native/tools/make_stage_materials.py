# The canyon rocks drawn the way the fighters are: flat colour in two hard tones from the MeshBites shader's lit variant, so
# they still answer to the sky's light and weather (run from the editor's Python watcher).
import unreal
mel = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools(); ell = unreal.EditorLevelLibrary
MB = '/Game/MeshBites_Stylized_Anime_Shader/Ressources/Master_Materials/'; DST = '/Game/GkStage'
for x in ell.get_all_level_actors():
    if x.get_actor_label().startswith('GkT_'): x.destroy_actor()
ar = unreal.AssetRegistryHelpers.get_asset_registry(); n = 0
PAL = [((0.62, 0.27, 0.14), (0.2, 0.08, 0.12)), ((0.66, 0.33, 0.17), (0.23, 0.1, 0.13)), ((0.55, 0.23, 0.13), (0.18, 0.07, 0.12))]
for a in ar.get_assets_by_path('/Game/GF_Desert_Canyon/Assets', recursive=True):
    if str(a.asset_class_path.asset_name) != 'StaticMesh': continue
    name = 'MI_Cel_' + str(a.asset_name); path = DST + '/' + name
    if eal.does_asset_exist(path): eal.delete_asset(path)
    m = at.create_asset(name, DST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(m, unreal.load_asset(MB + 'Material_Lit/MM_CellShader_Standard_Lit'))
    mel.set_material_instance_static_switch_parameter_value(m, 'Use RGB Mask', True)
    base, shade = PAL[n % 3]
    mel.set_material_instance_vector_parameter_value(m, 'Base  color', unreal.LinearColor(base[0], base[1], base[2], 1)); mel.set_material_instance_vector_parameter_value(m, 'Shadow_Base', unreal.LinearColor(shade[0], shade[1], shade[2], 1))
    for k, v in {'Shadow: Sharpness': 0.001, 'Shadow Opacity': 1.0, 'Shadow Amount': 0.5}.items(): mel.set_material_instance_scalar_parameter_value(m, k, v)
    mel.update_material_instance(m); eal.save_asset(path, False); n += 1
for t in ('MI_T_FlatLit', 'MI_T_FlatUnlit', 'MI_T_TexUnlit'):
    if eal.does_asset_exist(DST + '/' + t): eal.delete_asset(DST + '/' + t)
log('made %d flat cel rock materials' % n)
