# Builds the look-development scene: imports the fighter, makes the cel-shading and ink-outline materials,
# lays out the red-sun shrine arena with real lights, fog and reflections, and takes screenshots.
import unreal, os, math, traceback

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
ELL = unreal.EditorLevelLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
SRC = os.path.join(PROJECT, 'SourceArt')
V = unreal.Vector
R = lambda p, y, r=0: unreal.Rotator(roll=r, pitch=p, yaw=y)
C = lambda r, g, b, a=1.0: unreal.LinearColor(r, g, b, a)


def sp(o, **kw):
    for k, v in kw.items():
        try: o.set_editor_property(k, v)
        except Exception as e: log('  prop', type(o).__name__, k, 'failed:', str(e)[:120])


def imp(fn, dest, opts=None):
    t = unreal.AssetImportTask()
    sp(t, filename=os.path.join(SRC, fn), destination_path=dest, automated=True, replace_existing=True, save=True)
    if opts: sp(t, options=opts)
    AT.import_asset_tasks([t])
    paths = list(t.get_editor_property('imported_object_paths'))
    log('import', fn, '->', paths)
    return paths


# ------------------------------------------------------------------ character
def import_character():
    if not EAL.does_asset_exist('/Game/Kagu/Kagu_Run'):
        o = unreal.FbxImportUI()
        sp(o, import_mesh=True, import_as_skeletal=True, import_animations=True, import_materials=False, import_textures=False,
           mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH, automated_import_should_detect_type=False, create_physics_asset=False)
        try:
            sp(o.skeletal_mesh_import_data, import_morph_targets=False, use_t0_as_ref_pose=False, convert_scene=True, normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            sp(o.anim_sequence_import_data, import_bone_tracks=True, remove_redundant_keys=False)
        except Exception as e: log('fbx opts', e)
        imp('Kagu_Run.fbx', '/Game/Kagu', o)
    for fn in ['Kagu_Base.png', 'Kagu_Normal.png', 'Kagu_Rough.png']:
        if not EAL.does_asset_exist('/Game/Kagu/' + fn[:-4]): imp(fn, '/Game/Kagu')
    found = {}
    for p in EAL.list_assets('/Game/Kagu', recursive=True):
        a = EAL.load_asset(p)
        found.setdefault(type(a).__name__, []).append(a)
    log('kagu assets', {k: [x.get_name() for x in v] for k, v in found.items()})
    return found


# ------------------------------------------------------------------ materials
def mat(name):
    p = '/Game/Mat/' + name
    if EAL.does_asset_exist(p):
        m = EAL.load_asset(p); MEL.delete_all_material_expressions(m)
    else:
        m = AT.create_asset(name, '/Game/Mat', unreal.Material, unreal.MaterialFactoryNew())
    return m


def node(m, cls, x=0, y=0, **kw):
    n = MEL.create_material_expression(m, cls, x, y); sp(n, **kw); return n


def vparam(m, name, col, y=0): return node(m, unreal.MaterialExpressionVectorParameter, -700, y, parameter_name=name, default_value=col)
def sparam(m, name, v, y=0): return node(m, unreal.MaterialExpressionScalarParameter, -700, y, parameter_name=name, default_value=v)


def custom(m, code, inputs, out=unreal.CustomMaterialOutputType.CMOT_FLOAT3, x=-250, y=0):
    c = node(m, unreal.MaterialExpressionCustom, x, y, output_type=out, code=code)
    ins = []
    for nm, _, _ in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', nm); ins.append(i)
    c.set_editor_property('inputs', ins)
    for nm, src, outn in inputs: MEL.connect_material_expressions(src, outn, c, nm)
    return c


def done(m):
    MEL.recompile_material(m); EAL.save_loaded_asset(m); return m


TOON = '''
float3 n = normalize(N); float3 l = normalize(L.xyz); float ndl = dot(n, l);
float t = smoothstep(-0.03, 0.03, ndl) * 0.62 + smoothstep(0.5, 0.56, ndl) * 0.38;
float3 b = Base;
float rd = saturate((b.r - max(b.g, b.b)) / max(b.r, 0.05) * 1.7); float lum = dot(b, float3(0.3, 0.5, 0.2));
float3 pale = lerp(lum * lerp(float3(0.55, 1.0, 2.6), float3(1, 1, 1), smoothstep(0.3, 0.62, lum)), lerp(float3(0.5, 0.6, 0.82), float3(1, 1, 1), smoothstep(0.1, 0.65, b.r)), rd);
b = lerp(b, pale, Theme);
b = lerp(b, b * b * 1.25, 0.2) * 1.5 + 0.03;
float3 lit = b * lerp(Shadow.rgb, Light.rgb, t);
float fres = 1.0 - saturate(dot(n, normalize(Vw)));
float rim = smoothstep(0.58, 0.66, fres) * smoothstep(-0.5, 0.2, dot(n, normalize(RimDir.xyz)));
return lit + Rim.rgb * rim;
'''


def mk_toon(base_tex):
    m = mat('M_Toon'); sp(m, shading_model=unreal.MaterialShadingModel.MSM_UNLIT)
    tex = node(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -300, parameter_name='BaseTex', texture=base_tex)
    n = node(m, unreal.MaterialExpressionVertexNormalWS, -700, -100)
    v = node(m, unreal.MaterialExpressionCameraVectorWS, -700, -40)
    L = vparam(m, 'LightDir', C(0.62, -0.25, 0.6), 40); li = vparam(m, 'Light', C(1.5, 1.3, 1.2), 240); sh = vparam(m, 'Shadow', C(0.42, 0.22, 0.4), 440)
    rd = vparam(m, 'RimDir', C(-1, 0, 0.25), 640); rc = vparam(m, 'Rim', C(2.4, 0.5, 0.25), 840); th = sparam(m, 'Theme', 0.0, 1040)
    c = custom(m, TOON, [('Base', tex, 'RGB'), ('N', n, ''), ('Vw', v, ''), ('L', L, ''), ('Light', li, ''), ('Shadow', sh, ''), ('RimDir', rd, ''), ('Rim', rc, ''), ('Theme', th, '')])
    MEL.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return done(m)


def mk_outline():
    m = mat('M_Ink'); sp(m, shading_model=unreal.MaterialShadingModel.MSM_UNLIT, blend_mode=unreal.BlendMode.BLEND_MASKED, two_sided=True)
    ink = vparam(m, 'Ink', C(0.012, 0.004, 0.01), 0); MEL.connect_material_property(ink, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    ts = node(m, unreal.MaterialExpressionTwoSidedSign, -700, 300)
    neg = node(m, unreal.MaterialExpressionMultiply, -450, 300, const_b=-1.0); MEL.connect_material_expressions(ts, '', neg, 'A')
    MEL.connect_material_property(neg, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    vn = node(m, unreal.MaterialExpressionVertexNormalWS, -700, 500); w = sparam(m, 'Width', 0.55, 600)
    mul = node(m, unreal.MaterialExpressionMultiply, -450, 500); MEL.connect_material_expressions(vn, '', mul, 'A'); MEL.connect_material_expressions(w, '', mul, 'B')
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return done(m)


def mk_lit(name, col, rough=0.5, metal=0.0, emis=None, unlit=False, spec=0.5):
    m = mat(name)
    if unlit: sp(m, shading_model=unreal.MaterialShadingModel.MSM_UNLIT)
    if not unlit:
        MEL.connect_material_property(vparam(m, 'Color', col, 0), '', unreal.MaterialProperty.MP_BASE_COLOR)
        MEL.connect_material_property(sparam(m, 'Rough', rough, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
        MEL.connect_material_property(sparam(m, 'Metal', metal, 300), '', unreal.MaterialProperty.MP_METALLIC)
        MEL.connect_material_property(sparam(m, 'Spec', spec, 400), '', unreal.MaterialProperty.MP_SPECULAR)
    if emis is not None:
        MEL.connect_material_property(vparam(m, 'Glow', emis, 600), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return done(m)


FLOOR_COL = '''
float2 g = frac(WP.xy / 240.0 + 0.5); float grout = saturate(step(g.x, 0.012) + step(g.y, 0.012));
float2 id = floor(WP.xy / 240.0 + 0.5); float h = frac(sin(dot(id, float2(12.9898, 78.233))) * 43758.5453);
return lerp(float3(0.035, 0.028, 0.04) * (0.7 + 0.6 * h), float3(0.008, 0.006, 0.01), grout);
'''
FLOOR_R = '''
float2 g = frac(WP.xy / 240.0 + 0.5); float grout = saturate(step(g.x, 0.012) + step(g.y, 0.012));
float w = sin(WP.x * 0.0061 + sin(WP.y * 0.004) * 2.0) * sin(WP.y * 0.0053 + 1.3) * 0.5 + 0.5;
return lerp(lerp(0.035, 0.22, smoothstep(0.35, 0.75, w)), 0.6, grout);
'''


def mk_floor():
    m = mat('M_WetStone')
    wp = node(m, unreal.MaterialExpressionWorldPosition, -700, 0)
    MEL.connect_material_property(custom(m, FLOOR_COL, [('WP', wp, '')], y=-100), '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(custom(m, FLOOR_R, [('WP', wp, '')], out=unreal.CustomMaterialOutputType.CMOT_FLOAT1, y=200), '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(node(m, unreal.MaterialExpressionConstant, -250, 400, r=0.6), '', unreal.MaterialProperty.MP_SPECULAR)
    return done(m)


SKY = '''
float h = saturate(normalize(WP - Cam).z); float g = pow(1.0 - h, 3.0);
float3 top = float3(0.035, 0.012, 0.07), mid = float3(0.5, 0.04, 0.09), low = float3(1.9, 0.32, 0.1);
float3 c = lerp(top, mid, smoothstep(0.0, 0.75, g)); c = lerp(c, low, smoothstep(0.72, 1.0, g));
float band = sin(h * 46.0 + sin(WP.y * 0.00003) * 3.0) * 0.5 + 0.5; c *= 0.88 + 0.24 * smoothstep(0.55, 0.9, band) * smoothstep(0.02, 0.2, h) * (1.0 - smoothstep(0.2, 0.5, h));
return c;
'''


def mk_sky():
    m = mat('M_Sky'); sp(m, shading_model=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    wp = node(m, unreal.MaterialExpressionWorldPosition, -700, 0); cp = node(m, unreal.MaterialExpressionCameraPositionWS, -700, 150)
    MEL.connect_material_property(custom(m, SKY, [('WP', wp, ''), ('Cam', cp, '')]), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return done(m)


def inst(name, parent, scalars=None, vectors=None):
    p = '/Game/Mat/' + name
    mi = EAL.load_asset(p) if EAL.does_asset_exist(p) else AT.create_asset(name, '/Game/Mat', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for k, v in (scalars or {}).items(): MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items(): MEL.set_material_instance_vector_parameter_value(mi, k, v)
    MEL.update_material_instance(mi); EAL.save_loaded_asset(mi); return mi


# ------------------------------------------------------------------ level
def shape(name, mesh, loc, scale, m, rot=None):
    a = ELL.spawn_actor_from_object(unreal.load_asset('/Engine/BasicShapes/' + mesh), V(*loc), rot or R(0, 0))
    a.set_actor_scale3d(V(*scale)); a.set_actor_label(name)
    c = a.static_mesh_component; c.set_material(0, m); return a


def torii(tag, x, y, s, red, dark):
    # two leaning pillars, a tie beam, and the long curved-looking double lintel
    h = 520 * s; w = 330 * s
    for sd in (-1, 1):
        shape(tag + '_pillar', 'Cylinder', (x, y + sd * w, h / 2), (0.62 * s, 0.62 * s, h / 100), red, R(0, 0, sd * -2.5))
        shape(tag + '_foot', 'Cylinder', (x, y + sd * (w + 4 * s), 22 * s), (0.86 * s, 0.86 * s, 0.44 * s), dark)
    shape(tag + '_nuki', 'Cube', (x, y, h * 0.78), (0.42 * s, (2 * w + 190 * s) / 100, 0.5 * s), red)
    shape(tag + '_shimaki', 'Cube', (x, y, h + 12 * s), (0.6 * s, (2 * w + 300 * s) / 100, 0.52 * s), red)
    shape(tag + '_kasagi', 'Cube', (x, y, h + 52 * s), (0.86 * s, (2 * w + 420 * s) / 100, 0.34 * s), dark)
    for sd in (-1, 1): shape(tag + '_tip', 'Cube', (x, y + sd * (w + 205 * s), h + 66 * s), (0.86 * s, 0.5 * s, 0.34 * s), dark, R(0, 0, sd * 14))
    shape(tag + '_gaku', 'Cube', (x + 4, y, h * 0.9), (0.2 * s, 0.6 * s, 0.9 * s), dark)


def lantern(tag, x, y, s, stone, glow):
    shape(tag + '_base', 'Cube', (x, y, 14 * s), (0.9 * s, 0.9 * s, 0.28 * s), stone)
    shape(tag + '_post', 'Cylinder', (x, y, 75 * s), (0.3 * s, 0.3 * s, 1.1 * s), stone)
    shape(tag + '_box', 'Cube', (x, y, 150 * s), (0.62 * s, 0.62 * s, 0.56 * s), glow)
    shape(tag + '_cap', 'Cone', (x, y, 198 * s), (1.25 * s, 1.25 * s, 0.5 * s), stone)
    l = ELL.spawn_actor_from_class(unreal.PointLight, V(x + 20, y, 150 * s), R(0, 0)); l.set_actor_label(tag + '_light')
    sp(l.point_light_component, intensity_units=unreal.LightUnits.CANDELAS, intensity=55.0, light_color=unreal.Color(255, 150, 70), attenuation_radius=900.0, source_radius=18.0, cast_shadows=True, volumetric_scattering_intensity=0.6)


def fighter(tag, sk, anim, mi, ink, y, yaw, t, scale):
    cy = CEN[1] * scale * (1 if yaw == 0 else -1); cx = CEN[0] * scale * (1 if yaw == 0 else -1)
    a = ELL.spawn_actor_from_class(unreal.SkeletalMeshActor, V(-cx, y - cy, 0), R(0, yaw)); a.set_actor_label(tag); a.set_actor_scale3d(V(scale, scale, scale))
    c = a.skeletal_mesh_component
    try: c.set_skinned_asset_and_update(sk)
    except Exception:
        try: c.set_skeletal_mesh(sk)
        except Exception as e: log('set mesh', e)
    for i in range(max(1, c.get_num_materials())): c.set_material(i, mi)
    sp(c, overlay_material=ink, cast_shadow=True)
    if anim:
        try:
            c.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
            d = unreal.SingleAnimationPlayData(); sp(d, anim_to_play=anim, saved_position=t, saved_play_rate=0.0, saved_looping=False, saved_playing=False)
            sp(c, animation_data=d)
        except Exception as e: log('anim', e)
    return a


def build():
    kg = import_character()
    sk = (kg.get('SkeletalMesh') or [None])[0]; anim = (kg.get('AnimSequence') or [None])[0]
    tex = {t.get_name(): t for t in kg.get('Texture2D', [])}
    base = tex.get('Kagu_Base')
    if sk is None or base is None: log('MISSING character assets'); return
    ext = sk.get_bounds().box_extent; log('mesh extent', ext.x, ext.y, ext.z, 'anim', anim and anim.get_name(), anim and anim.get_play_length())
    scale = 92.0 / max(1e-3, ext.z)

    toon = mk_toon(base); ink = mk_outline()
    kagu = inst('MI_Kagu', toon, {'Theme': 0.0}); shiro = inst('MI_Shiro', toon, {'Theme': 1.0}, {'Rim': C(0.6, 1.1, 2.6), 'Shadow': C(0.34, 0.36, 0.66), 'LightDir': C(0.62, 0.25, 0.6)})
    ink_k = inst('MI_Ink', ink, {'Width': 0.5 / scale})
    stone = mk_lit('M_Stone', C(0.05, 0.045, 0.055), 0.75); red = mk_lit('M_Lacquer', C(0.55, 0.035, 0.03), 0.32); dark = mk_lit('M_DarkWood', C(0.02, 0.014, 0.018), 0.5)
    glow = mk_lit('M_Paper', C(0.8, 0.5, 0.25), 0.9, emis=C(22.0, 8.5, 2.2)); sun = mk_lit('M_Sun', C(1, 0, 0), unlit=True, emis=C(3.2, 0.16, 0.07)); mount = mk_lit('M_Mountain', C(0.03, 0.012, 0.03), 0.95)
    floor = mk_floor(); sky = mk_sky()

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if EAL.does_asset_exist('/Game/Maps/Arena'):
        les.load_level('/Game/Maps/Arena')
        for a in ELL.get_all_level_actors():
            try: ELL.destroy_actor(a)
            except Exception: pass
    else: les.new_level('/Game/Maps/Arena')

    # --- light: a low red sun behind the stage throwing long light toward the camera, sky, fog you can see shafts in
    s = ELL.spawn_actor_from_class(unreal.DirectionalLight, V(0, 0, 800), R(-7.5, 8)); s.set_actor_label('Sun')
    sp(s.light_component, intensity=3.2, light_color=unreal.Color(255, 92, 50), atmosphere_sun_light=True, volumetric_scattering_intensity=3.0, enable_light_shaft_bloom=True, bloom_scale=0.25, bloom_tint=unreal.Color(255, 60, 40), light_source_angle=2.0, cast_shadows=True)
    k = ELL.spawn_actor_from_class(unreal.DirectionalLight, V(0, 0, 900), R(-38, 150)); k.set_actor_label('Fill')
    sp(k.light_component, intensity=0.45, light_color=unreal.Color(150, 110, 255), atmosphere_sun_light=False, cast_shadows=False, volumetric_scattering_intensity=0.0)
    shape('SkyDome', 'Sphere', (0, 0, 0), (-2400, 2400, 2400), sky)
    sl = ELL.spawn_actor_from_class(unreal.SkyLight, V(0, 0, 500), R(0, 0)); sp(sl.light_component, real_time_capture=True, intensity=0.7)
    fg = ELL.spawn_actor_from_class(unreal.ExponentialHeightFog, V(0, 0, 0), R(0, 0))
    sp(fg.component, fog_density=0.012, fog_height_falloff=0.1, fog_inscattering_luminance=C(0.22, 0.03, 0.05), enable_volumetric_fog=True, volumetric_fog_scattering_distribution=0.75, volumetric_fog_extinction_scale=0.8, volumetric_fog_albedo=unreal.Color(255, 190, 190), fog_max_opacity=0.92, start_distance=300.0)
    pp = ELL.spawn_actor_from_class(unreal.PostProcessVolume, V(0, 0, 0), R(0, 0)); sp(pp, unbound=True)
    st = pp.get_editor_property('settings')
    for kk, vv in dict(bloom_intensity=1.35, bloom_threshold=0.2, auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL, auto_exposure_bias=-0.2, auto_exposure_apply_physical_camera_exposure=False, vignette_intensity=0.55,
                       scene_fringe_intensity=0.25, film_grain_intensity=0.12, color_saturation=unreal.Vector4(1.12, 1.0, 1.08, 1.0), color_contrast=unreal.Vector4(1.1, 1.08, 1.1, 1.0),
                       color_gamma_shadows=unreal.Vector4(0.96, 0.9, 1.06, 1.0), lens_flare_intensity=0.35, ambient_occlusion_intensity=0.7).items():
        try: st.set_editor_property('override_' + kk, True); st.set_editor_property(kk, vv)
        except Exception as e: log('  pp', kk, str(e)[:90])
    sp(pp, settings=st)

    # --- set: wet stone court, a great gate framing the sun, smaller gates and lanterns running back, a shrine hall, the mountain
    shape('Court', 'Plane', (-3000, 0, 0), (260, 260, 1), floor)
    shape('SunDisc', 'Sphere', (-62000, 1500, 7600), (250, 250, 250), sun)
    shape('Mountain', 'Cone', (-36000, 1200, 5200), (300, 300, 104), mount)
    shape('MountainL', 'Cone', (-33000, -26000, 2600), (260, 260, 52), mount); shape('MountainR', 'Cone', (-34000, 27000, 3100), (280, 280, 62), mount)
    torii('GateMain', -1750, 0, 2.3, red, dark)
    torii('GateL', -3400, -2100, 1.5, red, dark); torii('GateR', -3400, 2100, 1.5, red, dark); torii('GateFar', -6200, 0, 3.0, red, dark)
    for i, x in enumerate((-260, -900, -1500, -2300)):
        for sd in (-1, 1): lantern('Lantern%d%s' % (i, 'L' if sd < 0 else 'R'), x, sd * (760 + i * 150), 1.25, stone, glow)
    for sd in (-1, 1):
        y = sd * 3300
        shape('Hall_body', 'Cube', (-2600, y, 260), (9, 14, 5.2), dark); shape('Hall_roof', 'Cube', (-2600, y, 560), (11.5, 17, 0.5), dark); shape('Hall_roof2', 'Cube', (-2600, y, 640), (8, 13, 0.6), dark)
        for j in range(4): shape('Hall_win', 'Cube', (-2145, y - 480 + j * 320, 250), (0.1, 1.9, 2.2), glow)
    shape('Steps1', 'Cube', (-1250, 0, 12), (5, 30, 0.24), stone); shape('Steps2', 'Cube', (-1500, 0, 36), (5, 26, 0.24), stone)

    # --- fighters, mid-rush at each other
    W, Lc = ref_pose(); skel = (kg.get('Skeleton') or [None])[0]
    reed = bake('P_Reed', POSES['P_Reed'], W, Lc, skel); coil = bake('P_Coil', POSES['P_Coil'], W, Lc, skel)
    a = fighter('KAGU', sk, reed, kagu, ink_k, 150, 180, 0.0, scale); b = fighter('SHIRO', sk, coil, shiro, ink_k, -150, 0, 0.0, scale)

    cam = ELL.spawn_actor_from_class(unreal.CineCameraActor, V(600, 0, 92), R(2.5, 180)); cam.set_actor_label('MatchCam')
    cc = cam.get_cine_camera_component()
    try:
        fb = cc.get_editor_property('filmback'); sp(fb, sensor_width=36.0, sensor_height=20.25); sp(cc, filmback=fb)
        fs = cc.get_editor_property('focus_settings'); sp(fs, focus_method=unreal.CameraFocusMethod.MANUAL, manual_focus_distance=600.0); sp(cc, focus_settings=fs)
        sp(cc, current_focal_length=30.0, current_aperture=8.0)
    except Exception as e: log('cam', e)
    try: les.pilot_level_actor(cam)
    except Exception as e: log('pilot', e)
    les.save_current_level()
    log('scene built: scale', scale)

    def shot(n):
        def f():
            try: unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, 'look_%s.png' % n, cam); log('shot', n)
            except Exception: log(traceback.format_exc())
        return f
    later(6, shot('a'))


# ------------------------------------------------------------------ poses authored for this fighter
# A pose is written as targets in the fighter's own space (x = his left, y = the way he faces, z = up, centimetres,
# measured from the point between his feet): where the hips are, how the spine leans and turns, where each hand and
# foot is. Limbs are solved with two-bone IK, and the result is baked into an animation asset.
ML = unreal.MathLibrary
PARENT = {'Spine02': 'Hips', 'Spine01': 'Spine02', 'Spine': 'Spine01', 'neck': 'Spine', 'Head': 'neck', 'head_end': 'Head', 'headfront': 'Head',
          'LeftShoulder': 'Spine', 'LeftArm': 'LeftShoulder', 'LeftForeArm': 'LeftArm', 'LeftHand': 'LeftForeArm', 'RightShoulder': 'Spine', 'RightArm': 'RightShoulder', 'RightForeArm': 'RightArm', 'RightHand': 'RightForeArm',
          'LeftUpLeg': 'Hips', 'LeftLeg': 'LeftUpLeg', 'LeftFoot': 'LeftLeg', 'LeftToeBase': 'LeftFoot', 'RightUpLeg': 'Hips', 'RightLeg': 'RightUpLeg', 'RightFoot': 'RightLeg', 'RightToeBase': 'RightFoot'}
ORDER = ['Hips', 'Spine02', 'Spine01', 'Spine', 'neck', 'Head', 'head_end', 'headfront', 'LeftShoulder', 'LeftArm', 'LeftForeArm', 'LeftHand', 'RightShoulder', 'RightArm', 'RightForeArm', 'RightHand',
         'LeftUpLeg', 'LeftLeg', 'LeftFoot', 'LeftToeBase', 'RightUpLeg', 'RightLeg', 'RightFoot', 'RightToeBase']
CEN = (5.7, 15.3)


def vadd(a, b): return V(a.x + b.x, a.y + b.y, a.z + b.z)
def vsub(a, b): return V(a.x - b.x, a.y - b.y, a.z - b.z)
def vmul(a, k): return V(a.x * k, a.y * k, a.z * k)
def vlen(a): return math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z)
def vnorm(a):
    l = vlen(a); return V(a.x / l, a.y / l, a.z / l) if l > 1e-6 else V(0, 0, 1)
def vcross(a, b): return V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x)
def vdot(a, b): return a.x * b.x + a.y * b.y + a.z * b.z
def frame(d, h): return ML.make_rot_from_xz(vnorm(d), vnorm(h)).quaternion()
def rotz(v, deg):
    a = math.radians(deg); c, s_ = math.cos(a), math.sin(a); return V(v.x * c - v.y * s_, v.x * s_ + v.y * c, v.z)


def ik2(a, t, l1, l2, pole):
    d = vsub(t, a); L = max(1e-3, min(vlen(d), (l1 + l2) * 0.999)); dn = vnorm(d)
    x = (l1 * l1 - l2 * l2 + L * L) / (2 * L); h = math.sqrt(max(0.0, l1 * l1 - x * x))
    side = vsub(pole, vmul(dn, vdot(pole, dn))); side = vnorm(side) if vlen(side) > 1e-4 else V(0, 0, -1)
    mid = vadd(vadd(a, vmul(dn, x)), vmul(side, h)); return mid, vadd(a, vmul(dn, L))


def ref_pose():
    anim = EAL.load_asset('/Game/Kagu/Kagu_Run_Anim'); X = unreal.AnimPoseExtensions
    pose = X.get_anim_pose_at_frame(anim, 0, unreal.AnimPoseEvaluationOptions())
    W = {}; Lc = {}
    for n in ORDER:
        W[n] = X.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.WORLD); Lc[n] = X.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
    return W, Lc


def solve(P, W):
    """joint positions for a pose, in component space"""
    c = V(CEN[0], CEN[1], 0); J = {}
    rp = lambda n: W[n].translation
    fwd = rotz(V(0, 1, 0), P.get('pt', 0)); cf = rotz(V(0, 1, 0), P.get('tw', 0)); cl = rotz(V(1, 0, 0), P.get('tw', 0)); pl = rotz(V(1, 0, 0), P.get('pt', 0))
    hips = vadd(c, V(*P['hip'])); J['Hips'] = hips
    def lean(deg, side, f, l):
        a, b = math.radians(deg), math.radians(side); return vnorm(vadd(vadd(vmul(f, math.sin(a)), vmul(l, math.sin(b))), V(0, 0, math.cos(a) * math.cos(b))))
    d1 = lean(P.get('ln', 0) * 0.5, P.get('sd', 0) * 0.5, fwd, pl); d2 = lean(P.get('ln', 0), P.get('sd', 0), cf, cl); d3 = lean(P.get('ln2', P.get('ln', 0)), P.get('sd', 0), cf, cl)
    dh = lean(P.get('ln2', P.get('ln', 0)) * 0.4 + P.get('hd', 0), P.get('hs', 0), rotz(cf, P.get('hy', 0)), rotz(cl, P.get('hy', 0)))
    seg = lambda a, b: vlen(vsub(rp(b), rp(a)))
    J['Spine02'] = vadd(hips, vmul(d1, seg('Hips', 'Spine02'))); J['Spine01'] = vadd(J['Spine02'], vmul(d2, seg('Spine02', 'Spine01'))); J['Spine'] = vadd(J['Spine01'], vmul(d3, seg('Spine01', 'Spine')))
    J['neck'] = vadd(J['Spine'], vmul(d3, seg('Spine', 'neck'))); J['Head'] = vadd(J['neck'], vmul(dh, seg('neck', 'Head')))
    hf = rotz(cf, P.get('hy', 0)); J['_headF'] = hf; J['_headU'] = dh; J['_chestF'] = cf; J['_chestU'] = d3; J['_pelvF'] = fwd; J['_pelvU'] = d1
    chestL = vnorm(vcross(d3, cf)); chestL = vmul(chestL, -1) if vdot(chestL, cl) < 0 else chestL
    pelvL = vnorm(vcross(d1, fwd)); pelvL = vmul(pelvL, -1) if vdot(pelvL, pl) < 0 else pelvL
    for S, sg_ in (('Left', 1), ('Right', -1)):
        k = 'L' if sg_ > 0 else 'R'
        ro = vsub(rp(S + 'Arm'), rp('Spine')); sh = vadd(J['Spine'], vadd(vadd(vmul(chestL, abs(ro.x) * sg_), vmul(d3, ro.z)), vmul(cf, ro.y)))
        ros = vsub(rp(S + 'Shoulder'), rp('Spine')); J[S + 'Shoulder'] = vadd(J['Spine'], vadd(vadd(vmul(chestL, abs(ros.x) * sg_), vmul(d3, ros.z)), vmul(cf, ros.y))); J[S + 'Arm'] = sh
        pole = V(*P.get('e' + k, (sg_ * 0.5, -0.5, -1)))
        el, wr = ik2(sh, vadd(c, V(*P['h' + k])), seg(S + 'Arm', S + 'ForeArm'), seg(S + 'ForeArm', S + 'Hand'), pole); J[S + 'ForeArm'] = el; J[S + 'Hand'] = wr
        rh = vsub(rp(S + 'UpLeg'), rp('Hips')); hj = vadd(hips, vadd(vadd(vmul(pelvL, abs(rh.x) * sg_), vmul(d1, rh.z)), vmul(fwd, rh.y))); J[S + 'UpLeg'] = hj
        kp = V(*P.get('k' + k, (sg_ * 0.35, 1, 0.2)))
        kn, an = ik2(hj, vadd(c, V(*P['f' + k])), seg(S + 'UpLeg', S + 'Leg'), seg(S + 'Leg', S + 'Foot'), kp); J[S + 'Leg'] = kn; J[S + 'Foot'] = an
        td = V(*P.get('t' + k, (sg_ * 0.25, 1, -0.55))); J[S + 'ToeBase'] = vadd(an, vmul(vnorm(td), seg(S + 'Foot', S + 'ToeBase')))
    return J


CHILD = {'Hips': 'Spine02', 'Spine02': 'Spine01', 'Spine01': 'Spine', 'Spine': 'neck', 'neck': 'Head', 'LeftShoulder': 'LeftArm', 'LeftArm': 'LeftForeArm', 'LeftForeArm': 'LeftHand', 'RightShoulder': 'RightArm', 'RightArm': 'RightForeArm', 'RightForeArm': 'RightHand',
         'LeftUpLeg': 'LeftLeg', 'LeftLeg': 'LeftFoot', 'LeftFoot': 'LeftToeBase', 'RightUpLeg': 'RightLeg', 'RightLeg': 'RightFoot', 'RightFoot': 'RightToeBase'}


def hint_of(n, J, ref):
    """a second axis for each bone so its roll is defined: the body's forward for the trunk, the bend plane for limbs"""
    g = (lambda k: J[k]) if not ref else None
    if n in ('Hips', 'Spine02'): return V(0, 1, 0) if ref else J['_pelvF']
    if n in ('Spine01', 'Spine', 'LeftShoulder', 'RightShoulder'): return V(0, 1, 0) if ref else J['_chestF']
    if n in ('neck', 'Head'): return V(0, 1, 0) if ref else J['_headF']
    S = 'Left' if n.startswith('Left') else 'Right'
    if 'Arm' in n:
        a, b, c_ = J[S + 'Arm'], J[S + 'ForeArm'], J[S + 'Hand']; x = vcross(vsub(b, a), vsub(c_, b)); return x if vlen(x) > 0.5 else V(0, 0, 1)
    if n.endswith('Foot'): return V(0, 0, 1)
    a, b, c_ = J[S + 'UpLeg'], J[S + 'Leg'], J[S + 'Foot']; x = vcross(vsub(b, a), vsub(c_, b)); return x if vlen(x) > 0.5 else V(1, 0, 0)


def bake(name, P, W, Lc, skel):
    J = solve(P, W); Jr = {n: W[n].translation for n in ORDER}
    NW = {}; keys = {}
    for n in ORDER:
        par = PARENT.get(n)
        if n in CHILD:
            c_ = CHILD[n]; Qt = frame(vsub(J[c_], J[n]), hint_of(n, J, False)); Qb = frame(vsub(Jr[c_], Jr[n]), hint_of(n, Jr, True))
            NW[n] = Qt * Qb.inversed() * W[n].rotation
        else: NW[n] = NW[par] * Lc[n].rotation if par else W[n].rotation
        lr = NW[par].inversed() * NW[n] if par else NW[n]
        keys[n] = (J['Hips'] if n == 'Hips' else Lc[n].translation, lr)
    path = '/Game/Kagu/Anim/' + name
    if EAL.does_asset_exist(path): seq = EAL.load_asset(path)
    else:
        f = unreal.AnimSequenceFactory(); f.set_editor_property('target_skeleton', skel); seq = AT.create_asset(name, '/Game/Kagu/Anim', unreal.AnimSequence, f)
    ct = seq.controller; ct.open_bracket(unreal.Text('pose'), False); ct.set_frame_rate(unreal.FrameRate(30, 1), False); ct.set_number_of_frames(unreal.FrameNumber(1), False); ct.remove_all_bone_tracks(False)
    for n in ORDER:
        ct.add_bone_curve(n, False); p_, q = keys[n]; ct.set_bone_track_keys(n, [p_, p_], [q, q], [V(1, 1, 1), V(1, 1, 1)], False)
    ct.close_bracket(False); EAL.save_loaded_asset(seq); log('baked', name); return seq


POSES = {
    # KAGU: upright and loose, weight on the back leg, the lead hand raised open like an offered cup, the other trailing low behind
    'P_Reed': dict(hip=(0, -7, 83), ln=-7, ln2=2, tw=-52, pt=-34, hd=6, hy=42, hL=(13, 46, 132), eL=(0.3, -0.2, -1), hR=(-25, -18, 80), eR=(-0.8, -0.6, 0.2), fL=(9, 26, 9), fR=(-13, -27, 9), kL=(0.5, 1, 0), kR=(-0.9, 0.6, 0), tL=(0.1, 1, -0.5), tR=(-0.8, 0.5, -0.5)),
    # SHIRO: low and coiled, fingertips to the floor, the other arm swept back like a drawn blade
    'P_Coil': dict(hip=(0, -10, 54), ln=38, ln2=46, tw=-36, pt=-24, hd=-34, hy=26, hL=(10, 44, 12), eL=(0.6, 0.2, 0.5), hR=(-30, -40, 100), eR=(-0.6, -0.6, 0.6), fL=(13, 34, 9), fR=(-16, -40, 12), kL=(0.7, 1, 0.3), kR=(-0.9, 0.5, 0.2), tL=(0.2, 1, -0.5), tR=(-0.6, 0.7, -0.75)),
}


def probe():
    try:
        anim = EAL.load_asset('/Game/Kagu/Kagu_Run_Anim'); X = unreal.AnimPoseExtensions
        pose = X.get_anim_pose_at_frame(anim, 0, unreal.AnimPoseEvaluationOptions())
        names = [str(n) for n in X.get_bone_names(pose)]; log('BONES', len(names), names)
        for n in names:
            w = X.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.WORLD); l = X.get_ref_bone_pose(pose, n, unreal.AnimPoseSpaces.LOCAL)
            log('REF', n, [round(v, 2) for v in (w.translation.x, w.translation.y, w.translation.z)], [round(v, 3) for v in (l.rotation.x, l.rotation.y, l.rotation.z, l.rotation.w)], [round(v, 2) for v in (l.translation.x, l.translation.y, l.translation.z)])
        log('CTRL', [m for m in dir(anim.controller) if not m.startswith('_')])
        log('SEQFAC', [m for m in dir(unreal.AnimSequenceFactory) if not m.startswith('_')][:40])
    except Exception: log(traceback.format_exc())


def play_test():
    """starts the game in the editor, takes a run of screenshots while the sparring partners fight, and stops it"""
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    try: les.editor_request_begin_play(); log('play started')
    except Exception: log(traceback.format_exc()); return
    def shot(n):
        def f():
            try: unreal.AutomationLibrary.take_high_res_screenshot(1280, 720, 'pie_%s.png' % n); log('pie shot', n)
            except Exception: log(traceback.format_exc())
        return f
    for i, t in enumerate((5, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41)): later(t, shot(chr(97 + i)))
    def stop():
        try: les.editor_request_end_play(); log('play stopped')
        except Exception: log(traceback.format_exc())
    later(45, stop)


build()
later(14, play_test)
