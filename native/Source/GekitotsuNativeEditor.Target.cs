using UnrealBuildTool;
using System.Collections.Generic;

public class GekitotsuNativeEditorTarget : TargetRules
{
	public GekitotsuNativeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GekitotsuNative");
	}
}
