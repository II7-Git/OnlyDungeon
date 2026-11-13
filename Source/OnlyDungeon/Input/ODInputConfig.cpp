// OnlyDungeon 입력 설정

#include "ODInputConfig.h"

#include "InputAction.h"
#include "Core/ODLogChannels.h"

namespace
{
	const UInputAction* FindActionInList(const TArray<FODInputAction>& Actions, const FGameplayTag& InputTag)
	{
		for (const FODInputAction& Action : Actions)
		{
			if (Action.InputAction && Action.InputTag == InputTag)
			{
				return Action.InputAction;
			}
		}
		return nullptr;
	}
}

const UInputAction* UODInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	const UInputAction* Found = FindActionInList(NativeInputActions, InputTag);
	if (!Found && bLogNotFound)
	{
		UE_LOG(LogOD, Warning, TEXT("%s: 네이티브 입력 액션을 찾지 못함 [%s]"), *GetNameSafe(this), *InputTag.ToString());
	}
	return Found;
}

const UInputAction* UODInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	const UInputAction* Found = FindActionInList(AbilityInputActions, InputTag);
	if (!Found && bLogNotFound)
	{
		UE_LOG(LogOD, Warning, TEXT("%s: 어빌리티 입력 액션을 찾지 못함 [%s]"), *GetNameSafe(this), *InputTag.ToString());
	}
	return Found;
}
