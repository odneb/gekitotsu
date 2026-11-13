import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
for name in ['SM_GkTitleLatin','SM_GkTitleKanji']:
 t=unreal.AssetImportTask();t.filename=str(root/'Art/Identity/Geometry'/(name+'.obj'));t.destination_path='/Game/GkUI';t.destination_name=name;t.automated=True;t.replace_existing=True;t.save=True
 o=unreal.FbxImportUI();o.import_mesh=True;o.import_materials=False;o.import_textures=False;o.import_as_skeletal=False;o.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
 o.static_mesh_import_data.combine_meshes=True;o.static_mesh_import_data.generate_lightmap_u_vs=False;o.static_mesh_import_data.auto_generate_collision=False;o.static_mesh_import_data.convert_scene=False
 t.options=o;unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t]);m=unreal.load_asset('/Game/GkUI/'+name);assert m,name
 unreal.EditorAssetLibrary.save_loaded_asset(m);unreal.log('TITLE MESH '+name+' bounds '+str(m.get_bounding_box()))
