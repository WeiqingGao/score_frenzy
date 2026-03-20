// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ScoreFrenzy : ModuleRules
{
	public ScoreFrenzy(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "NavigationSystem", "AIModule", "GameplayTasks" });
	}
}
