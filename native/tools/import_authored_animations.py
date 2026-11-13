import unreal,json,traceback,os
from pathlib import Path
# Extract the licensed FBX downloads into three named subfolders; never overwrite existing assets.
root=Path(os.environ['GK_ANIMATION_SOURCE']).expanduser().resolve()
if not root.is_dir():raise RuntimeError('GK_ANIMATION_SOURCE must point to the extracted animation packs')
mesh=unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
skel=mesh.get_editor_property('skeleton')
at=unreal.AssetToolsHelpers.get_asset_tools()
results=[]
for pack in ['AnimeMartialArts','AnimePowerUp','SuperheroGround']:
 for f in sorted((root/pack).rglob('*.fbx')):
  dest='/Game/GkNewAnimations/'+pack
  if f.parent.name in ['In_Place','Root_Motion']:dest+='/'+f.parent.name
  path=dest+'/'+f.stem
  try:
   if not unreal.EditorAssetLibrary.does_asset_exist(path):
    opt=unreal.FbxImportUI(); opt.set_editor_property('automated_import_should_detect_type',False)
    opt.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_ANIMATION)
    opt.set_editor_property('original_import_type',unreal.FBXImportType.FBXIT_ANIMATION)
    opt.set_editor_property('import_mesh',False); opt.set_editor_property('import_animations',True)
    opt.set_editor_property('skeleton',skel); opt.set_editor_property('import_materials',False);opt.set_editor_property('import_textures',False)
    t=unreal.AssetImportTask();t.filename=str(f);t.destination_path=dest;t.destination_name=f.stem;t.automated=True;t.save=True;t.replace_existing=False;t.options=opt
    t.factory=unreal.FbxFactory()
    at.import_asset_tasks([t])
   a=unreal.load_asset(path)
   results.append({'pack':pack,'path':path,'length':a.get_play_length() if a else 0,'ok':isinstance(a,unreal.AnimSequence)})
   unreal.log('GK_IMPORT '+path+' '+str(results[-1]['ok']))
  except Exception as e:results.append({'pack':pack,'path':path,'error':str(e)});unreal.log_error(traceback.format_exc())
report=Path(unreal.Paths.project_saved_dir())/'authored-animation-import.json'
report.write_text(json.dumps(results,indent=2))
