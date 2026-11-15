// OnlyDungeon 적 AI 컨트롤러

#include "ODAIController.h"

#include "Components/StateTreeAIComponent.h"
#include "StateTree.h"
#include "Character/ODEnemyCharacter.h"
#include "Core/ODLogChannels.h"

AODAIController::AODAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));

	// 적의 상태(대기·사망)에 맞춰 직접 시작·정지한다.
	bStartAILogicOnPossess = false;
	StateTreeAI->SetStartLogicAutomatically(false);

	// 컨트롤러를 폰 위치에 붙여 둔다. (StateTree 디버깅·EQS 기준 위치)
	bAttachToPawn = true;
}

void AODAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(InPawn);
	if (!IsValid(Enemy))
	{
		return;
	}

	SetGenericTeamId(Enemy->GetGenericTeamId());
	if (UStateTree* Tree = Enemy->GetStateTree())
	{
		StateTreeAI->SetStateTree(Tree);
	}

	// 레벨에 배치된 적은 BeginPlay 전에 빙의된다. 그때는 ASC·전투 상태가 아직 준비되지 않았으므로 적의 BeginPlay가 다시 부른다.
	if (Enemy->HasActorBegunPlay())
	{
		RefreshLogic(true);
	}
}

void AODAIController::OnUnPossess()
{
	StopLogicAndMovement(TEXT("빙의 해제"));
	Super::OnUnPossess();
}

void AODAIController::RefreshLogic(bool bRestart)
{
	const AODEnemyCharacter* Enemy = Cast<AODEnemyCharacter>(GetPawn());
	const bool bShouldRun = IsValid(Enemy) && IsValid(Enemy->GetStateTree()) && !Enemy->IsDead() && Enemy->IsCombatActive();

	if (!bShouldRun)
	{
		StopLogicAndMovement(TEXT("대기 또는 사망"));
		return;
	}

	if (!StateTreeAI->IsRunning())
	{
		StateTreeAI->StartLogic();
		UE_LOG(LogODAI, Verbose, TEXT("%s AI 시작"), *GetNameSafe(Enemy));
	}
	else if (bRestart)
	{
		StateTreeAI->RestartLogic();
		UE_LOG(LogODAI, Verbose, TEXT("%s AI 재시작"), *GetNameSafe(Enemy));
	}
}

void AODAIController::StopLogicAndMovement(const TCHAR* Reason)
{
	if (IsValid(StateTreeAI) && StateTreeAI->IsRunning())
	{
		StateTreeAI->StopLogic(Reason);
		UE_LOG(LogODAI, Verbose, TEXT("%s AI 정지: %s"), *GetNameSafe(GetPawn()), Reason);
	}
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
}
