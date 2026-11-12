using UnrealBuildTool;

public class GekitotsuNative : ModuleRules
{
	public GekitotsuNative(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "RenderCore", "RHI" });
		// fight sticks are read straight from USB (GkPad.cpp)
		if (Target.Platform == UnrealTargetPlatform.Mac) { PublicFrameworks.AddRange(new string[] { "IOKit", "CoreFoundation" }); }
	}
}
