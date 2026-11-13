# M_GkRim v4: crisp anime edge light = the original view-angle rim (worked), but never on surfaces facing up (ground, water) and faded with distance.
import unreal
mel = unreal.MaterialEditingLibrary; eal = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
DST = '/Game/GkFx'; NAME = 'M_GkRim'; path = DST + '/' + NAME
m = unreal.load_asset(path) if eal.does_asset_exist(path) else None
if m is None: m = at.create_asset(NAME, DST, unreal.Material, unreal.MaterialFactoryNew())
else: mel.delete_all_material_expressions(m)
log('material %s' % m)
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_POST_PROCESS)
BL = [getattr(unreal.BlendableLocation, k) for k in dir(unreal.BlendableLocation) if 'AFTER_TONEMAPPING' in k.upper() and 'REPLACING' not in k.upper()][0]; m.set_editor_property('blendable_location', BL)
E = lambda cls, x, y: mel.create_material_expression(m, cls, x, y)
C = mel.connect_material_expressions
def sp(name, v, x, y):
    e = E(unreal.MaterialExpressionScalarParameter, x, y); e.set_editor_property('parameter_name', name); e.set_editor_property('default_value', v); return e
def mask(src, ch, x, y):
    e = E(unreal.MaterialExpressionComponentMask, x, y)
    for k in 'rgb':
        e.set_editor_property(k, k in ch)
    C(src, '', e, ''); return e
def tex(i, x, y):
    e = E(unreal.MaterialExpressionSceneTexture, x, y); e.set_editor_property('scene_texture_id', i); return e
def cst(v, x, y):
    e = E(unreal.MaterialExpressionConstant, x, y); e.set_editor_property('r', v); return e
thr = sp('Thr', 0.72, -1800, -300); soft = sp('Soft', 0.04, -1800, -200); strength = sp('Strength', 1.0, -1800, -100); expo = sp('Exp', 2.0, -1800, 0)
near = sp('Near', 2500.0, -1800, 100); far = sp('Far', 9000.0, -1800, 200)
col = E(unreal.MaterialExpressionVectorParameter, -1800, 320); col.set_editor_property('parameter_name', 'Color'); col.set_editor_property('default_value', unreal.LinearColor(0.55, 1.0, 0.9, 1.0))
n = tex(unreal.SceneTextureId.PPI_WORLD_NORMAL, -1500, -500); n3 = mask(n, 'rgb', -1300, -500)
cam = E(getattr(unreal, "MaterialExpressionCameraVector", None) or getattr(unreal, "MaterialExpressionCameraVectorWS"), -1300, -380)
dot = E(unreal.MaterialExpressionDotProduct, -1100, -450); C(n3, '', dot, 'A'); C(cam, '', dot, 'B')
ab = E(unreal.MaterialExpressionAbs, -950, -450); C(dot, '', ab, '')
sat = E(unreal.MaterialExpressionSaturate, -820, -450); C(ab, '', sat, '')
inv = E(unreal.MaterialExpressionOneMinus, -700, -450); C(sat, '', inv, '')
pw = E(unreal.MaterialExpressionPower, -570, -450); C(inv, '', pw, 'Base'); C(expo, '', pw, 'Exp')
hi = E(unreal.MaterialExpressionAdd, -1500, -250); C(thr, '', hi, 'A'); C(soft, '', hi, 'B')
rim = E(unreal.MaterialExpressionSmoothStep, -420, -400); C(thr, '', rim, 'Min'); C(hi, '', rim, 'Max'); C(pw, '', rim, 'Value')
dp = tex(unreal.SceneTextureId.PPI_SCENE_DEPTH, -1500, 100); dp1 = mask(dp, 'r', -1300, 100)
fd = E(unreal.MaterialExpressionSmoothStep, -1100, 100); C(near, '', fd, 'Min'); C(far, '', fd, 'Max'); C(dp1, '', fd, 'Value')
w = E(unreal.MaterialExpressionOneMinus, -950, 100); C(fd, '', w, '')
ln = E(unreal.MaterialExpressionLength, -1100, -600); C(n3, '', ln, '')
vl = E(unreal.MaterialExpressionSmoothStep, -950, -650); C(cst(0.5, -1100, -700), '', vl, 'Min'); C(cst(0.9, -1100, -780), '', vl, 'Max'); C(ln, '', vl, 'Value')
nz = mask(n3, 'b', -1300, -900)
up = E(unreal.MaterialExpressionSmoothStep, -950, -900); C(cst(0.3, -1100, -950), '', up, 'Min'); C(cst(0.6, -1100, -1000), '', up, 'Max'); C(nz, '', up, 'Value')
noup = E(unreal.MaterialExpressionOneMinus, -820, -900); C(up, '', noup, '')
m1 = E(unreal.MaterialExpressionMultiply, -250, -300); C(rim, '', m1, 'A'); C(w, '', m1, 'B')
m2 = E(unreal.MaterialExpressionMultiply, -120, -300); C(m1, '', m2, 'A'); C(vl, '', m2, 'B')
m2b = E(unreal.MaterialExpressionMultiply, -20, -300); C(m2, '', m2b, 'A'); C(noup, '', m2b, 'B')
m3 = E(unreal.MaterialExpressionMultiply, 70, -300); C(m2b, '', m3, 'A'); C(strength, '', m3, 'B')
tint = E(unreal.MaterialExpressionMultiply, 170, -200); C(m3, '', tint, 'A'); C(col, '', tint, 'B')
sc = tex(unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0, -300, 150); sc3 = mask(sc, 'rgb', -100, 150)
out = E(unreal.MaterialExpressionAdd, 320, 0); C(sc3, '', out, 'A'); C(tint, '', out, 'B')
mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(m); eal.save_asset(path, False)
log('made %s' % path); log('DONE rim material')
