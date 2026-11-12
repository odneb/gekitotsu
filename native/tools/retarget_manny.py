# Every bought clip onto the game's own mannequin (suffix _Manny), so the robot plays them directly.
import unreal, traceback
eal = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools(); ar = unreal.AssetRegistryHelpers.get_asset_registry()
DST = '/Game/GkAnim'; ST = unreal.RetargetSourceOrTarget
def fresh(name, cls, factory):
    p = DST + '/' + name
    if eal.does_asset_exist(p): eal.delete_asset(p)
    return at.create_asset(name, DST, cls, factory)
def cls(a): return str(a.asset_class_path.asset_name)
try:
    MAN = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
    trig = fresh('IK_GkManny', unreal.IKRigDefinition, unreal.IKRigDefinitionFactory()); tc = unreal.IKRigController.get_controller(trig); tc.set_skeletal_mesh(MAN)
    log('manny auto %s' % tc.apply_auto_generated_retarget_definition()); eal.save_asset(DST + '/IK_GkManny', False)
    P5 = unreal.load_asset('/Game/UnarmFightAnimBundle/DEMO/Character_Unarmed/Mannequins/Meshes/SKM_Manny_Simple')
    JOBS = [('U5', ['/Game/UnarmFightAnimBundle', '/Game/Superhero_Combat'], unreal.load_asset('/Game/GkAnim/IK_GkMannequin'), P5),
            ('Fight', ['/Game/FightAnimations'], unreal.load_asset('/Game/GkAnim/IK_GkFightSrc'), unreal.load_asset('/Game/FightAnimations/Mannequin/Character/Mesh/SK_Mannequin')),
            ('Special', ['/Game/SpecialMoves_Energy'], unreal.load_asset('/Game/GkAnim/IK_GkSpecialSrc'), unreal.load_asset('/Game/SpecialMoves_Energy/Demo/Character/Mesh/SK_Mannequin_Female'))]
    for tag, roots, src, src_mesh in JOBS:
        have = set(); anims = []
        for r in roots:
            for a in ar.get_assets_by_path(r, recursive=True):
                if cls(a) == 'AnimSequence': have.add(str(a.asset_name))
        for r in roots:
            for a in ar.get_assets_by_path(r, recursive=True):
                n = str(a.asset_name)
                if cls(a) == 'AnimSequence' and n + '_Kagu' in have and n + '_Manny' not in have: anims.append(a)
        name = 'RTG_%sToManny' % tag
        rtg = fresh(name, unreal.IKRetargeter, unreal.IKRetargetFactory()); rc = unreal.IKRetargeterController.get_controller(rtg)
        rc.set_ik_rig(ST.SOURCE, src); rc.set_ik_rig(ST.TARGET, trig)
        for step in ('rc.set_preview_mesh(ST.SOURCE, src_mesh)', 'rc.set_preview_mesh(ST.TARGET, MAN)', 'rc.add_default_ops()', 'rc.assign_ik_rig_to_all_ops(ST.SOURCE, src)', 'rc.assign_ik_rig_to_all_ops(ST.TARGET, trig)',
                     'rc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)', 'rc.auto_align_all_bones(ST.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)'):
            try: eval(step)
            except Exception as e: log('  %s FAILED %s' % (step, str(e)[:160]))
        eal.save_asset(DST + '/' + name, False)
        out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(anims, src_mesh, MAN, rtg, '', '', '', '_Manny', '', True)
        n = 0
        for o in out:
            a = unreal.load_asset(str(o.package_name))
            if isinstance(a, unreal.AnimSequence): eal.save_loaded_asset(a, False); n += 1
        log(' %s -> Manny: %d of %d' % (tag, n, len(anims)))
    names = sorted(str(a.package_name) + '|%.2f' % 0 for r in ('/Game/UnarmFightAnimBundle', '/Game/Superhero_Combat', '/Game/FightAnimations', '/Game/SpecialMoves_Energy') for a in ar.get_assets_by_path(r, recursive=True) if cls(a) == 'AnimSequence' and str(a.asset_name).endswith('_Manny'))
    open(PROJECT + '/Saved/manny_clips.txt', 'w').write('\n'.join(names))
except Exception as e:
    log('ERR ' + traceback.format_exc())
log('DONE manny')
