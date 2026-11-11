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
b = lerp(b, b * b * 1.25, 0.25) * 1.2 + 0.012;
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
    L = vparam(m, 'LightDir', C(-0.75, 0.35, 0.55), 40); li = vparam(m, 'Light', C(1.15, 1.0, 0.92), 240); sh = vparam(m, 'Shadow', C(0.34, 0.2, 0.36), 440)
    rd = vparam(m, 'RimDir', C(-1, 0, 0.25), 640); rc = vparam(m, 'Rim', C(3.2, 0.9, 0.5), 840); th = sparam(m, 'Theme', 0.0, 1040)
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
    sp(l.point_light_component, intensity=9000.0, light_color=unreal.Color(255, 150, 70), attenuation_radius=900.0, source_radius=18.0, cast_shadows=True, volumetric_scattering_intensity=2.0)


def fighter(tag, sk, anim, mi, ink, y, yaw, t, scale):
    a = ELL.spawn_actor_from_class(unreal.SkeletalMeshActor, V(0, y, 0), R(0, yaw)); a.set_actor_label(tag); a.set_actor_scale3d(V(scale, scale, scale))
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
            sp(c, animation_data=d, update_animation_in_editor=True)
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
    kagu = inst('MI_Kagu', toon, {'Theme': 0.0}); shiro = inst('MI_Shiro', toon, {'Theme': 1.0}, {'Rim': C(0.9, 1.6, 3.4), 'Shadow': C(0.3, 0.32, 0.62)})
    ink_k = inst('MI_Ink', ink, {'Width': 0.5 / scale})
    stone = mk_lit('M_Stone', C(0.05, 0.045, 0.055), 0.75); red = mk_lit('M_Lacquer', C(0.55, 0.035, 0.03), 0.32); dark = mk_lit('M_DarkWood', C(0.02, 0.014, 0.018), 0.5)
    glow = mk_lit('M_Paper', C(0.8, 0.5, 0.25), 0.9, emis=C(22.0, 8.5, 2.2)); sun = mk_lit('M_Sun', C(1, 0, 0), unlit=True, emis=C(9.0, 0.5, 0.18)); mount = mk_lit('M_Mountain', C(0.03, 0.012, 0.03), 0.95)
    floor = mk_floor()

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if EAL.does_asset_exist('/Game/Maps/Arena'):
        les.load_level('/Game/Maps/Arena')
        for a in ELL.get_all_level_actors():
            try: ELL.destroy_actor(a)
            except Exception: pass
    else: les.new_level('/Game/Maps/Arena')

    # --- light: a low red sun behind the stage throwing long light toward the camera, sky, fog you can see shafts in
    s = ELL.spawn_actor_from_class(unreal.DirectionalLight, V(0, 0, 800), R(-7.5, 8)); s.set_actor_label('Sun')
    sp(s.light_component, intensity=7.0, light_color=unreal.Color(255, 92, 50), atmosphere_sun_light=True, volumetric_scattering_intensity=3.0, enable_light_shaft_bloom=True, bloom_scale=0.25, bloom_tint=unreal.Color(255, 60, 40), light_source_angle=2.0, cast_shadows=True)
    k = ELL.spawn_actor_from_class(unreal.DirectionalLight, V(0, 0, 900), R(-38, 150)); k.set_actor_label('Fill')
    sp(k.light_component, intensity=1.1, light_color=unreal.Color(150, 120, 255), atmosphere_sun_light=False, cast_shadows=False)
    ELL.spawn_actor_from_class(unreal.SkyAtmosphere, V(0, 0, 0), R(0, 0)).set_actor_label('Sky')
    sl = ELL.spawn_actor_from_class(unreal.SkyLight, V(0, 0, 500), R(0, 0)); sp(sl.light_component, real_time_capture=True, intensity=1.4)
    fg = ELL.spawn_actor_from_class(unreal.ExponentialHeightFog, V(0, 0, 0), R(0, 0))
    sp(fg.component, fog_density=0.035, fog_height_falloff=0.12, fog_inscattering_color=C(0.5, 0.06, 0.07), volumetric_fog=True, volumetric_fog_scattering_distribution=0.75, volumetric_fog_extinction_scale=1.6, volumetric_fog_albedo=unreal.Color(255, 190, 190), fog_max_opacity=0.92, start_distance=300.0)
    pp = ELL.spawn_actor_from_class(unreal.PostProcessVolume, V(0, 0, 0), R(0, 0)); sp(pp, unbound=True)
    st = pp.get_editor_property('settings')
    for kk, vv in dict(bloom_intensity=1.35, bloom_threshold=0.2, auto_exposure_method=unreal.AutoExposureMethod.AEM_MANUAL, auto_exposure_bias=10.4, auto_exposure_apply_physical_camera_exposure=False, vignette_intensity=0.55,
                       scene_fringe_intensity=1.2, film_grain_intensity=0.12, color_saturation=unreal.Vector4(1.12, 1.0, 1.08, 1.0), color_contrast=unreal.Vector4(1.1, 1.08, 1.1, 1.0),
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
    a = fighter('KAGU', sk, anim, kagu, ink_k, -175, 0, 0.18, scale); b = fighter('SHIRO', sk, anim, shiro, ink_k, 175, 180, 0.52, scale)

    cam = ELL.spawn_actor_from_class(unreal.CineCameraActor, V(1020, 0, 78), R(4.5, 180)); cam.set_actor_label('MatchCam')
    cc = cam.get_cine_camera_component()
    try:
        fb = cc.get_editor_property('filmback'); sp(fb, sensor_width=36.0, sensor_height=20.25); sp(cc, filmback=fb)
        fs = cc.get_editor_property('focus_settings'); sp(fs, focus_method=unreal.CameraFocusMethod.MANUAL, manual_focus_distance=1020.0); sp(cc, focus_settings=fs)
        sp(cc, current_focal_length=27.0, current_aperture=2.8)
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
    later(20, shot('a')); later(90, shot('b')); later(240, shot('c'))


build()
