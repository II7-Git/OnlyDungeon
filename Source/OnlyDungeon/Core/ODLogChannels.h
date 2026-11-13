// OnlyDungeon 전용 로그 카테고리 선언
// 새 코드에서는 LogTemp / LogOnlyDungeon 대신 아래 카테고리를 사용한다. (CLAUDE.md 7장)

#pragma once

#include "Logging/LogMacros.h"

/** 일반 / 게임 흐름 로그 */
ONLYDUNGEON_API DECLARE_LOG_CATEGORY_EXTERN(LogOD, Log, All);

/** 전투, 데미지, 히트 판정 로그 */
ONLYDUNGEON_API DECLARE_LOG_CATEGORY_EXTERN(LogODCombat, Log, All);

/** GAS 어빌리티, 이펙트, Attribute 로그 */
ONLYDUNGEON_API DECLARE_LOG_CATEGORY_EXTERN(LogODAbility, Log, All);

/** 적 AI, StateTree 로그 */
ONLYDUNGEON_API DECLARE_LOG_CATEGORY_EXTERN(LogODAI, Log, All);
