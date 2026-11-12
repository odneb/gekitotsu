using UnrealBuildTool;
using System.Collections.Generic;

public class GekitotsuNativeTarget : TargetRules
{
	public GekitotsuNativeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("GekitotsuNative");
	}
}
