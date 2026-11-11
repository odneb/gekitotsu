using UnrealBuildTool;
using System.Collections.Generic;

public class GekitotsuTarget : TargetRules
{
	public GekitotsuTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Gekitotsu");
	}
}
