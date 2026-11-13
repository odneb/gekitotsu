import unreal
from pathlib import Path
filename=Path(__file__).resolve().parents[1]/'Saved/FilmAssets'/'SM_GkFilmBeam.obj'
t=unreal.AssetImportTask();t.filename=str(filename);t.destination_path='/Game/GkFx';t.destination_name='SM_GkFilmBeam';t.automated=True;t.replace_existing=True;t.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
unreal.log('FILM MESH IMPORT '+str(t.imported_object_paths))
assert t.imported_object_paths
m=unreal.load_asset('/Game/GkFx/SM_GkFilmBeam');unreal.log('FILM MESH BOUNDS '+str(m.get_bounding_box()))
