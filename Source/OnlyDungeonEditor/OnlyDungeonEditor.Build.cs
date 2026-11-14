// OnlyDungeon 에디터 전용 모듈 (게임 빌드에는 포함되지 않음)

using UnrealBuildTool;

public class OnlyDungeonEditor : ModuleRules
{
	public OnlyDungeonEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"OnlyDungeon",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks"
		});
	}
}
