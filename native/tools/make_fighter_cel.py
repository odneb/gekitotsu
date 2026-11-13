"""Generate the directional fighter material with Unreal's Python commandlet."""
import unreal

edit = unreal.MaterialEditingLibrary
material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_GkFighterCel", "/Game/GkFx", unreal.Material, unreal.MaterialFactoryNew()
)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("used_with_skeletal_mesh", True)
node = edit.create_material_expression(material, unreal.MaterialExpressionCustom, 0, 0)
node.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
expressions = {
    "Normal": edit.create_material_expression(material, unreal.MaterialExpressionPixelNormalWS, -500, 0),
    "View": edit.create_material_expression(material, unreal.MaterialExpressionCameraVectorWS, -500, 100),
}
for i, label in enumerate(("Paint", "Shadow", "Key", "KeyTint", "Rim")):
    expression = edit.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 200 + 100 * i)
    expression.set_editor_property("parameter_name", label)
    expression.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1))
    expressions[label] = expression
inputs = []
for label in expressions:
    entry = unreal.CustomInput()
    entry.set_editor_property("input_name", label)
    inputs.append(entry)
node.set_editor_property("inputs", inputs)
for label, expression in expressions.items():
    if not edit.connect_material_expressions(expression, "", node, label):
        raise RuntimeError(f"Could not connect {label}")
node.set_editor_property("code", """
float3 N = normalize(Normal);
float3 L = normalize(Key.rgb);
float facing = dot(N, L);
float band = smoothstep(0.05, 0.10, facing);
float top = smoothstep(0.66, 0.72, facing);
float3 painted = lerp(Shadow.rgb, Paint.rgb * KeyTint.rgb, band);
painted += Paint.rgb * 0.24 * top;
float3 back = normalize(L - normalize(View) * 0.8 + float3(0,0,0.2));
float edge = 1 - smoothstep(0.11, 0.28, saturate(dot(N, normalize(View))));
float rim = edge * smoothstep(0.10, 0.35, dot(N, back));
return painted + Rim.rgb * rim * 1.25;
""")
if not edit.connect_material_property(node, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR):
    raise RuntimeError("Could not connect fighter output")
if not unreal.EditorAssetLibrary.save_loaded_asset(material):
    raise RuntimeError("Could not save fighter material")
