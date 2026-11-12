# Retarget the UE4-mannequin packs (Fight Animations Pack, Special Moves) onto Kagu and Noa.
import unreal, traceback
eal = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools(); ar = unreal.AssetRegistryHelpers.get_asset_registry()
DST = '/Game/GkAnim'; ST = unreal.RetargetSourceOrTarget
exec(open(PROJECT + '/tools/fix_noa_scale.py').read())
def fresh(name, cls, factory):
    p = DST + '/' + name
    if eal.does_asset_exist(p): eal.delete_asset(p)
    return at.create_asset(name, DST, cls, factory)
def cls(a): return str(a.asset_class_path.asset_name)
KAGU = unreal.load_asset('/Game/Kagu/Kagu_V3'); NOA = unreal.load_asset('/Game/MeshBites_Stylized_Anime_Shader/Art/Models/Characters/Noa_00/Meshes/SKM_Noa_00')
TG = [('Kagu', KAGU, unreal.load_asset('/Game/Kagu/IK_Kagu')), ('Noa', NOA, unreal.load_asset('/Game/GkAnim/IK_GkNoa'))]
try:
    for tag, root in (('Fight', '/Game/FightAnimations'), ('Special', '/Game/SpecialMoves_Energy')):
        allA = ar.get_assets_by_path(root, recursive=True)
        meshes = [a for a in allA if cls(a) == 'SkeletalMesh']
        anims = [a for a in allA if cls(a) == 'AnimSequence' and not str(a.asset_name).endswith(('_Kagu', '_Noa'))]
        log('%s: %d clips, meshes %s' % (tag, len(anims), [str(m.package_name) for m in meshes]))
        if not meshes or not anims: continue
        src_mesh = meshes[0].get_asset()
        src = fresh('IK_Gk%sSrc' % tag, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        sc = unreal.IKRigController.get_controller(src); sc.set_skeletal_mesh(src_mesh)
        log(' auto %s root %s chains %s' % (sc.apply_auto_generated_retarget_definition(), sc.get_retarget_root(), [str(c.get_editor_property('chain_name')) for c in sc.get_retarget_chains()]))
        eal.save_asset(DST + '/IK_Gk%sSrc' % tag, False)
        for tn, tmesh, trig in TG:
            name = 'RTG_%sTo%s' % (tag, tn)
            rtg = fresh(name, unreal.IKRetargeter, unreal.IKRetargetFactory()); rc = unreal.IKRetargeterController.get_controller(rtg)
            rc.set_ik_rig(ST.SOURCE, src); rc.set_ik_rig(ST.TARGET, trig)
            for step in ('rc.set_preview_mesh(ST.SOURCE, src_mesh)', 'rc.set_preview_mesh(ST.TARGET, tmesh)', 'rc.add_default_ops()', 'rc.assign_ik_rig_to_all_ops(ST.SOURCE, src)', 'rc.assign_ik_rig_to_all_ops(ST.TARGET, trig)',
                         'rc.auto_map_chains(unreal.AutoMapChainType.EXACT, True)', 'rc.auto_align_all_bones(ST.TARGET, unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)'):
                try: eval(step)
                except Exception as e: log('  %s FAILED %s' % (step, str(e)[:160]))
            eal.save_asset(DST + '/' + name, False)
            out = unreal.IKRetargetBatchOperation.duplicate_and_retarget(anims, src_mesh, tmesh, rtg, '', '', '', '_' + tn, '', True)
            n = 0
            for o in out:
                a = unreal.load_asset(str(o.package_name))
                if not isinstance(a, unreal.AnimSequence): continue
                if tn == 'Noa':
                    try: fix(a)
                    except Exception as e: log('  fix fail %s %s' % (o.package_name, str(e)[:80]))
                eal.save_loaded_asset(a, False); n += 1
            log(' %s -> %s: %d clips' % (tag, tn, n))
except Exception as e:
    log('ERR ' + traceback.format_exc())
log('DONE ue4packs')
