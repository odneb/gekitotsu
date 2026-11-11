using UnrealBuildTool;
using System.Collections.Generic;

public class GekitotsuEditorTarget : TargetRules
{
	public GekitotsuEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Gekitotsu");
	}
}
