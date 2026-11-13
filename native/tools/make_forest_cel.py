"""Generate the forest's post-process material from a fresh editor commandlet."""
import unreal

edit = unreal.MaterialEditingLibrary
directory = "/Game/GkFx"
name = "M_GkForestCel"
path = f"{directory}/{name}"
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError(f"{path} already exists. Generate in a fresh project copy.")

material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    name, directory, unreal.Material, unreal.MaterialFactoryNew()
)
material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_BEFORE_BLOOM)

custom = edit.create_material_expression(material, unreal.MaterialExpressionCustom, -200, 0)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
labels = ("Scene", "Albedo", "Depth")
inputs = []
for label in labels:
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", label)
    inputs.append(entry)
custom.set_editor_property("inputs", inputs)
# Connect after assigning the Custom node's input names.
for index, (label, texture_id) in enumerate((
    ("Scene", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0),
    ("Albedo", unreal.SceneTextureId.PPI_BASE_COLOR),
    ("Depth", unreal.SceneTextureId.PPI_SCENE_DEPTH),
)):
    texture = edit.create_material_expression(material, unreal.MaterialExpressionSceneTexture, -650, index * 150)
    texture.set_editor_property("scene_texture_id", texture_id)
    if not edit.connect_material_expressions(texture, "Color", custom, label):
        raise RuntimeError(f"Could not connect {label}")

custom.set_editor_property("code", """
// Preserve the painted texture and use its luminance to estimate the light on it.
const float3 luma = float3(0.2126, 0.7152, 0.0722);
float reflectance = dot(max(Albedo.rgb, 0), luma);
float illumination = dot(max(Scene.rgb, 0), luma) / max(reflectance, 0.055);
float lightBand = smoothstep(0.24, 0.29, illumination);
float highlightBand = smoothstep(0.57, 0.63, illumination);
float celLight = 0.17 + 0.25 * lightBand + 0.33 * highlightBand;
float adjustment = clamp(celLight - illumination, -0.28, 0.22);
float surface = smoothstep(0.025, 0.10, reflectance) * (1 - smoothstep(2800, 9500, Depth.r));
surface *= 1 - smoothstep(0.75, 1.4, illumination);
return max(Scene.rgb + Albedo.rgb * (0.78 * surface * adjustment), 0);
""")
if not edit.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not connect the post-process output")
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError(f"Could not save {path}")
