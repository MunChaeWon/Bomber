// Copyright (c) Yevhenii Selivanov.

using UnrealBuildTool;

public class BomberTests : ModuleRules
{
	public BomberTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Bomber",
			"GameplayAbilities",
			"MyUtils",
			"UnrealEd"
		});
	}
}
