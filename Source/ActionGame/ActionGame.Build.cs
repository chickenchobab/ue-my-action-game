// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActionGame : ModuleRules
{
	public ActionGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"GameplayTags",
			"AnimGraphRuntime",
			"AnimationBlueprintLibrary",
			"AnimationLocomotionLibraryRuntime",
			"MotionWarping" // modified
        });

		PrivateDependencyModuleNames.AddRange(new string[] { 
		});

		PublicIncludePaths.AddRange(new string[] {
			"ActionGame",
			"ActionGame/Variant_Platforming",
			"ActionGame/Variant_Platforming/Animation",
			"ActionGame/Variant_Combat",
			"ActionGame/Variant_Combat/AI",
			"ActionGame/Variant_Combat/Animation",
			"ActionGame/Variant_Combat/Gameplay",
			"ActionGame/Variant_Combat/Interfaces",
			"ActionGame/Variant_Combat/UI",
			"ActionGame/Variant_SideScrolling",
			"ActionGame/Variant_SideScrolling/AI",
			"ActionGame/Variant_SideScrolling/Gameplay",
			"ActionGame/Variant_SideScrolling/Interfaces",
			"ActionGame/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
