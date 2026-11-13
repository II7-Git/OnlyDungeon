# OnlyDungeon 진행 현황

> **이 문서의 용도:** 다른 대화(Claude 앱, 에디터 내 어시스턴트, Claude Code 등)에서도 현재 진행 상태를 바로 파악하기 위한 기록입니다.
> 기획 결정은 `Docs/GameDesign.md`, 작업 규칙은 `CLAUDE.md`가 기준이며, 이 문서는 **"지금 어디까지 했는가"**만 다룹니다.
>
> **마지막 갱신:** 2026-11-13

---

## 1. 한눈에 보기

| 영역 | 상태 |
|---|---|
| 프로젝트 기반 (CLAUDE.md, Git, MCP 연결, 빌드 검증) | 완료 |
| 기획서 (`Docs/GameDesign.md`) | 1차 완료, 미정 항목 남음 (기획서 8장) |
| 전투 애니메이션 (Paragon 한손검·대검, 8방향 구르기 리타겟) | 완료 |
| GAS 뼈대 (ASC, AttributeSet 2종, InputTag 입력, 피해 계산) | 완료 |
| 기본 에셋 (입력, 초기 수치 GE, 어빌리티 세트, 플레이어 BP, GameMode, 맵) | 완료 |
| 회피 어빌리티 | 완료 (PIE 검증) |
| 스태미나 자연 회복 | 완료 (PIE 검증) |
| 약공격 / 강공격 / 방어(패링·가드) / 락온 | 예정 |
| 전투 모드 관리 컴포넌트 (필드 ↔ 보스전 전환) | 예정 (구조 승인 필요) |
| 적 / 보스 AI | 예정 |

---

## 2. 완료된 작업

### 2-1. 프로젝트 기반
- `CLAUDE.md`: UE 5.9 고정, 승인 게이트, MCP 규칙, Git 규칙, 네이밍, 코딩 표준, GAS 규칙, 빌드 검증
- `ValidateBuild.bat`: 에디터 타겟 빌드 검증 (에디터를 닫고 실행). 로그 `Saved/ValidateBuild/LastBuild.log`
- `Tools/MCP/od-mcp-bridge.mjs`: Claude 데스크톱 앱 ↔ UE 5.9 내장 MCP(무상태 프로토콜 2026-07-28) 브리지
- Git 원격: `https://github.com/II7-Git/OnlyDungeon.git` (`main`). 소스·설정·문서만 추적, `Content/` 미추적

### 2-2. C++ (`Source/OnlyDungeon/`)
| 경로 | 클래스 / 내용 |
|---|---|
| `Core/ODLogChannels` | `LogOD`, `LogODCombat`, `LogODAbility`, `LogODAI` |
| `Core/ODGameplayTags` | 네이티브 태그 (`InputTag.*`, `Mode.*`, `State.*`, `Event.*`, `Data.*`, `Cooldown.*`, `Weapon.Type.*`) |
| `AbilitySystem/ODAbilitySystemComponent` | InputTag 기반 어빌리티 발동 (눌림/뗌) |
| `AbilitySystem/ODGameplayAbility` | 어빌리티 기반. 발동 방식 `OnInputTriggered` / `WhileInputActive` / `OnSpawn` |
| `AbilitySystem/ODAbilitySet` | 어빌리티·GE 묶음 DataAsset, 부여/회수 핸들 |
| `AbilitySystem/Attributes/ODAttributeSet_Health` | 체력·강인도 + 메타(IncomingDamage, IncomingPoiseDamage) |
| `AbilitySystem/Attributes/ODAttributeSet_Combat` | 스태미나·회복 속도·공격력·방어력·무게 |
| `AbilitySystem/Executions/ODDamageExecution` | 임시 피해식 `기본 + 공격력 - 방어력`. 대상이 `State.Invulnerable`이면 무시 |
| `AbilitySystem/Abilities/ODGameplayAbility_Dodge` | 회피 (비락온: 입력 방향 회전 후 정면 구르기 / 입력 없음: 뒤 구르기 / 락온: 8방향). 무적 구간, 스태미나 비용 |
| `AbilitySystem/Abilities/ODGameplayAbility_StaminaRegen` | 스태미나 자연 회복 패시브 (마지막 소모 1.5초 후 회복 GE 적용, 가득 차면 제거) |
| `Character/ODCharacterBase` | ASC·AttributeSet 보유, 사망/강인도 붕괴 처리, `GetLockOnTarget()`(락온 구현 전 nullptr) |
| `Character/ODPlayerCharacter` | 카메라, Enhanced Input → InputTag 바인딩 |
| `Character/ODEnemyCharacter` | 적 기반 (AI 자동 빙의) |
| `Input/ODInputConfig` | 입력 액션 ↔ InputTag DataAsset |

### 2-3. 에디터 에셋 (`/Game/OnlyDungeon/`)
| 경로 | 내용 |
|---|---|
| `Animation/Retarget/` | Paragon → Manny 리타겟 리소스 |
| `Animation/OneHandSword/` (`OH_*`), `TwoHandSword/` (`TH_*`) | Greystone 한손검, Kwang 대검 모션 |
| `Animation/Common/` (`CM_Roll_*` 8방향) | 구르기 모션. `Montages/AM_Roll_*` 8개 (`DefaultSlot`) |
| `Input/Actions/` | `IA_Move`, `IA_Look`, `IA_Attack_Light`, `IA_Attack_Heavy`, `IA_Dodge`, `IA_Defense`, `IA_Skill_1`, `IA_Skill_2`, `IA_LockOn`, `IA_UseItem` |
| `Input/IMC_Boss`, `Input/DA_InputConfig_Player` | 보스전 키 매핑 (좌클릭 약공격, 우클릭 강공격, Shift 방어, Space 회피), 입력 설정 |
| `AbilitySystem/Effects/` | `GE_Init_Player`(초기 수치), `GE_Cost_Dodge`(스태미나 -20), `GE_Regen_Stamina`(0.1초마다 회복 속도×0.1) |
| `AbilitySystem/Abilities/` | `GA_Dodge`, `GA_StaminaRegen` |
| `AbilitySystem/DA_AbilitySet_Player` | `GE_Init_Player` + `GA_Dodge`(`InputTag.Dodge`) + `GA_StaminaRegen`(패시브) |
| `Characters/BP_ODPlayer` | 플레이어 (Manny + `ABP_Unarmed`) |
| `Core/BP_ODGameMode` | 기본 폰 `BP_ODPlayer`. 프로젝트 전역 GameMode |
| `Maps/L_OD_Prototype` | 테스트 맵 (게임·에디터 시작 맵) |

---

## 3. 진행 중

- 없음 (다음 작업 대기)

### 최근 검증 기록
- **회피 (2026-11-13):** 입력 방향 회전 후 구르기 약 3m, 입력 없으면 뒤 구르기, 스태미나 20 소모, 0이면 발동 불가, 무적 약 0.08~0.48초.
- **스태미나 회복 (2026-11-13):** 마지막 소모 1.51초 후 회복 시작, 초당 25 회복, 회복 중 소모 시 즉시 정지 후 1.5초 대기 재시작, 가득 차면 회복 GE 제거.

## 4. 다음 할 일 (순서는 사용자와 조율)

1. 약공격 (한손검 콤보, Paragon Greystone 모션)
2. 강공격 (보스전 전용)
3. 방어: 한손검 패링, 대검 가드 (무기 데이터가 방식 결정)
4. 락온 (`State.LockOn`, `GetLockOnTarget()` 구현 → 회피 8방향 활성화)
5. 전투 모드 관리 컴포넌트 구조안 승인 → 필드/보스전 전환, `IMC_Field`
6. 프로토타입: 필드 1구간 + Stone Golem 보스전 + 모드 전환 컷신

## 5. 알려진 이슈 / 임시 처리

- 피해 계산식, 수치(스태미나 비용, 회복 속도 등)는 모두 초안입니다.
- 락온이 없어서 회피 8방향은 아직 실제로 동작하지 않습니다.
- 사망 시 모든 어빌리티가 취소되므로 스태미나 회복도 멈춥니다. 부활 구현 시 기본 어빌리티 재부여·재발동이 필요합니다.
- `Config/DefaultEditor.ini`의 변경 사항은 아직 커밋하지 않았습니다 (에디터 개인 설정 여부 확인 필요).

---

## 6. 작업 환경 노하우 (다른 대화에서도 참고)

- **빌드**
  - 에디터를 켠 상태: 게임 타겟(`OnlyDungeon Win64 Development`)으로 컴파일 오류를 먼저 확인할 수 있습니다. 스크립트 `Saved/ValidateBuild/run_game_target.bat`.
  - 새 UCLASS 추가나 헤더 구조 변경 후에는 **에디터를 닫고 `ValidateBuild.bat`** 를 실행해야 합니다. 에디터에서 띄운 자식 프로세스는 에디터 종료 시 함께 종료되므로 사용자가 직접 실행합니다.
- **MCP / 에디터 Python**
  - `execute_python`의 인자는 `script: {language: "Python", content: "..."}`, 결과는 마지막 식의 값이 반환됩니다. 스크립트 안에서 `call_tool(이름, **인자)`(await 가능)로 다른 툴을 호출할 수 있습니다.
  - 블로킹 금지: `subprocess.run`, `time.sleep` 대신 `asyncio`를 사용합니다.
  - **레벨을 복제한 직후 같은 스크립트에서 그 레벨을 열면 에디터가 크래시합니다** (GC 미정리 → `EditorServer.cpp` Fatal). 레벨은 `EditorLoadingAndSavingUtils.save_map(world, 새경로)`로 복제하고, 여는 작업은 별도 호출로 합니다.
  - BlueprintReadWrite가 아닌 구조체(GameplayTag, GameplayAttribute, GameplayModifierInfo 등)는 `import_text("(...)")`로 설정합니다.
  - PIE 입력 테스트: `InputToolset.ApplyInput`(액션 경로 지정), 상태 확인은 Python에서 ASC 태그·Attribute를 프레임 단위로 샘플링합니다.
- **파일 규칙:** C++ 소스는 UTF-8 BOM + CRLF, 한글 주석.

---

## 7. 이 문서 갱신 규칙

- 작업 단위(기능 하나)가 끝날 때마다 1~5장을 갱신하고, 커밋에 함께 포함합니다.
- 새로 알게 된 환경 문제와 해결법은 6장에 추가합니다.
