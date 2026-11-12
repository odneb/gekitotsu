def capture(unreal, log, name, loc=(0,560,95), yaw=-90, fov=30, w=1600, h=800):
    ell = unreal.EditorLevelLibrary
    world = unreal.UnrealEditorSubsystem().get_editor_world()
    for a in ell.get_all_level_actors():
        if a.get_actor_label() == 'GkT_cap': a.destroy_actor()
    cap = ell.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(*loc), unreal.Rotator(roll=0,pitch=0,yaw=yaw))
    cap.set_actor_label('GkT_cap')
    c = cap.capture_component2d
    rt = unreal.RenderingLibrary.create_render_target2d(world, w, h, unreal.TextureRenderTargetFormat.RTF_RGBA8_SRGB)
    c.texture_target = rt
    c.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    c.fov_angle = fov
    c.capture_every_frame = False
    pp = c.post_process_settings
    for k,v in [('auto_exposure_min_brightness',1.0),('auto_exposure_max_brightness',1.0),('auto_exposure_bias',0.0)]:
        pp.set_editor_property('override_'+k, True); pp.set_editor_property(k, v)
    c.post_process_settings = pp
    c.post_process_blend_weight = 1.0
    c.capture_scene()
    import os
    d = os.path.join(unreal.Paths.project_saved_dir(), 'shots')
    unreal.RenderingLibrary.export_render_target(world, rt, d, name)
    log('captured %s' % name)
