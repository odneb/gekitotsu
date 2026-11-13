# The stage floor: the canyon pack's rippled sand, tiled across the plane with a slow variation over it so the repeat does not show.
import unreal
mel = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
path = '/Game/GkStage/M_GkSand'
if eal.does_asset_exist(path): eal.delete_asset(path)
m = at.create_asset('M_GkSand', '/Game/GkStage', unreal.Material, unreal.MaterialFactoryNew())
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
T = '/Game/GF_Desert_Canyon/Textures/Textures/Rock_sand_1/'
def uv(u, v, y):
    c = E(unreal.MaterialExpressionTextureCoordinate, -900, y); c.set_editor_property('u_tiling', u); c.set_editor_property('v_tiling', v); return c
def tex(asset, coord, y, kind):
    t = E(unreal.MaterialExpressionTextureSample, -600, y); t.set_editor_property('texture', unreal.load_asset(asset)); t.set_editor_property('sampler_type', kind)
    mel.connect_material_expressions(coord, '', t, 'UVs'); return t
near = uv(160.0, 180.0, 0); far = uv(13.0, 14.6, 300)
col = tex(T + 'T_sand_1', near, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
big = tex(T + 'T_sand_1', far, 300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
lo = E(unreal.MaterialExpressionConstant, -350, 420); lo.set_editor_property('r', 0.72)
hi = E(unreal.MaterialExpressionConstant, -350, 480); hi.set_editor_property('r', 1.25)
var = E(unreal.MaterialExpressionLinearInterpolate, -200, 380); mel.connect_material_expressions(lo, '', var, 'A'); mel.connect_material_expressions(hi, '', var, 'B'); mel.connect_material_expressions(big, 'R', var, 'Alpha')
out = E(unreal.MaterialExpressionMultiply, -50, 100); mel.connect_material_expressions(col, 'RGB', out, 'A'); mel.connect_material_expressions(var, '', out, 'B')
mel.connect_material_property(out, '', unreal.MaterialProperty.MP_BASE_COLOR)
nrm = tex(T + 'T_SAND_RIPPLES_Normal', near, 600, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
mel.connect_material_property(nrm, 'RGB', unreal.MaterialProperty.MP_NORMAL)
r = E(unreal.MaterialExpressionConstant, -50, 300); r.set_editor_property('r', 0.92); mel.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(m); eal.save_asset(path, False)
log('made %s' % path)
