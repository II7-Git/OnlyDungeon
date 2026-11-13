// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlyDungeon : ModuleRules
{
	public OnlyDungeon(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"OnlyDungeon",
			"OnlyDungeon/Variant_Platforming",
			"OnlyDungeon/Variant_Platforming/Animation",
			"OnlyDungeon/Variant_Combat",
			"OnlyDungeon/Variant_Combat/AI",
			"OnlyDungeon/Variant_Combat/Animation",
			"OnlyDungeon/Variant_Combat/Gameplay",
			"OnlyDungeon/Variant_Combat/Interfaces",
			"OnlyDungeon/Variant_Combat/UI",
			"OnlyDungeon/Variant_SideScrolling",
			"OnlyDungeon/Variant_SideScrolling/AI",
			"OnlyDungeon/Variant_SideScrolling/Gameplay",
			"OnlyDungeon/Variant_SideScrolling/Interfaces",
			"OnlyDungeon/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
