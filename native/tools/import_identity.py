"""Import the original transparent title artwork; all menu controls remain native."""
import unreal
from pathlib import Path
root=Path(unreal.Paths.project_dir())
task=unreal.AssetImportTask()
task.filename=str(root/'Art/Identity/Gekitotsu-Impact-Logo.png')
task.destination_path='/Game/GkUI'
task.destination_name='T_GekitotsuImpact'
task.automated=True
task.replace_existing=True
task.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex=unreal.load_asset('/Game/GkUI/T_GekitotsuImpact')
assert tex
tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
tex.set_editor_property('srgb',True)
assert unreal.EditorAssetLibrary.save_loaded_asset(tex)
unreal.log('IDENTITY IMPORT SAVED')
