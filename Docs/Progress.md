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
| 약공격 (한손검 4타 콤보, 판정 제외) | 완료 (PIE 검증) |
| 테스트 맵 (`L_OD_Test_Combat`, 허수아비 3개) | 완료 |
| 무기 장착·적중 판정·피해·경직 (한손검) | 완료 (PIE 검증) |
| 강공격·패링(한손검), 약공격·강공격·가드(대검) | 완료 (PIE 검증) |
| 테스트 기능 (Tab 무기 교체, F5 초기화, 반격 허수아비) | 완료 |
| 무기별 이동 애니메이션 레이어 / 락온 / 부활 | 예정 |
| 전투 모드 관리 컴포넌트 (필드 ↔ 보스전 전환) | 예정 (구조 승인 필요) |
| 적 / 보스 AI | 예정 |

---

## 2. 완료된 작업

### 2-1. 프로젝트 기반
- `CLAUDE.md`: UE 5.9 고정, 승인 게이트, MCP 규칙, Git 규칙, 네이밍, 코딩 표준, GAS 규칙, 빌드 검증
- `ValidateBuild.bat`: 에디터 타겟 빌드 검증 (에디터를 닫고 실행). 로그 `Saved/ValidateBuild/LastBuild.log`
- `Tools/MCP/od-mcp-bridge.mjs`: Claude 데스크톱 앱 ↔ UE 5.9 내장 MCP(무상태 프로토콜 2026-07-28) 브리지
- Git 원격: `https://github.com/II7-Git/OnlyDungeon.git`. 소스·설정·문서만 추적, `Content/` 미추적
- 브랜치: **`dev`에서 개발·테스트**, 검증이 끝나면 사용자 지시로 `main`에 병합 (Main-Dev 구조, 2026-11-13 도입)

### 2-2. C++ (`Source/OnlyDungeon/`)
| 경로 | 클래스 / 내용 |
|---|---|
| `Animation/ODAnimNotifyState_GameplayEventWindow` | 애니메이션 구간 시작·끝에 게임플레이 이벤트 전송 (콤보 입력 구간, 이후 판정 구간) |
| `Animation/ODAnimNotifyState_WeaponTrace` | 공격 모션의 무기 판정 구간 (장비 컴포넌트의 무기 액터 판정 켜기) |
| `Equipment/ODWeaponData` | 무기 정의 DataAsset (타입 태그, 메시, 부착 소켓·보정, 어빌리티 세트, 애님 레이어, 칼날 판정 구간) |
| `Equipment/ODWeaponActor` | 손에 붙는 무기. 칼날 여러 지점 스윕 판정, 구간당 대상 1회, `Event.Melee.Hit` 전송. 디버그 `OD.Debug.WeaponTrace 1` |
| `Equipment/ODEquipmentComponent` | 무기 장착·해제 (액터 부착, 어빌리티 세트·타입 태그·애님 레이어 적용/회수). ASC 초기화 후 기본 무기 장착. 공격·방어 중 `AbilityAttachSocket`으로 부착 전환 |
| `Core/ODLogChannels` | `LogOD`, `LogODCombat`, `LogODAbility`, `LogODAI` |
| `Core/ODGameplayTags` | 네이티브 태그 (`InputTag.*`, `Mode.*`, `State.*`, `Event.*`, `Data.*`, `Cooldown.*`, `Weapon.Type.*`) |
| `AbilitySystem/ODAbilitySystemComponent` | InputTag 기반 어빌리티 발동 (눌림/뗌) |
| `AbilitySystem/ODGameplayAbility` | 어빌리티 기반. 발동 방식 `OnInputTriggered` / `WhileInputActive` / `OnSpawn` |
| `AbilitySystem/ODAbilitySet` | 어빌리티·GE 묶음 DataAsset, 부여/회수 핸들 |
| `AbilitySystem/Attributes/ODAttributeSet_Health` | 체력·강인도 + 메타(IncomingDamage, IncomingPoiseDamage) |
| `AbilitySystem/Attributes/ODAttributeSet_Combat` | 스태미나·회복 속도·공격력·방어력·무게 |
| `AbilitySystem/Executions/ODDamageExecution` | 임시 피해식 `기본 + 공격력 - 방어력`. 무적이면 무시. **정면(±75°) 패링 성공** → 피해 0, 공격자에게 `Event.PoiseBroken`(Magnitude 1 = 긴 경직). **정면 가드** → 피해 80% 감소, 강인도 피해 0, 감소 전 피해만큼 스태미나 소모, 부족하면 가드 붕괴(경직) |
| `Test/ODTrainingDummy` | [테스트 전용] 허수아비. `AutoAttackInterval`>0이면 주기적으로 플레이어를 보고 공격 |
| `AbilitySystem/Abilities/ODGameplayAbility_Dodge` | 회피 (비락온: 입력 방향 회전 후 정면 구르기 / 입력 없음: 뒤 구르기 / 락온: 8방향). 무적 구간, 스태미나 비용 |
| `AbilitySystem/Abilities/ODGameplayAbility_MeleeCombo` | 섹션 콤보 근접 공격 (입력 예약, 콤보 구간에서 다음 타격, 타격마다 스태미나·입력 방향 회전, 타격별 피해·강인도 피해 → `GE_Damage_Melee`) |
| `AbilitySystem/Abilities/ODGameplayAbility_Defense` | 방어. 패링(판정 구간 동안 `State.Parrying`) / 가드(누르는 동안 `State.Defending`, WhileInputActive). 판정은 피해 계산이 처리 |
| `AbilitySystem/Abilities/ODGameplayAbility_HitReact` | `Event.PoiseBroken`으로 발동하는 경직 (공격자 방향별 몽타주, 끝나면 강인도 회복) |
| `AbilitySystem/Abilities/ODGameplayAbility_StaminaRegen` | 스태미나 자연 회복 패시브 (마지막 소모 1.5초 후 회복 GE 적용, 가득 차면 제거) |
| `Character/ODCharacterBase` | ASC·AttributeSet·**장비 컴포넌트** 보유(적도 무기 사용 가능), 사망/강인도 붕괴 처리, `GetLockOnTarget()`(nullptr), `ResetForTest()`. 메시는 화면 밖에서도 본 갱신 |
| `Character/ODPlayerCharacter` | 카메라, Enhanced Input → InputTag 바인딩, 가드 중 이동 차단, [테스트] `TestWeapons` 순환(Tab)·전체 초기화(F5) |
| `Character/ODEnemyCharacter` | 적 기반 (AI 자동 빙의) |
| `Input/ODInputConfig` | 입력 액션 ↔ InputTag DataAsset |
| `../OnlyDungeonEditor/` (에디터 전용 모듈) | `UODMontageEditorLibrary`: Python에서 몽타주 세그먼트·섹션 구성 (`build_sectioned_montage`) |

### 2-3. 에디터 에셋 (`/Game/OnlyDungeon/`)
| 경로 | 내용 |
|---|---|
| `Animation/Retarget/` | Paragon → Manny 리타겟 리소스 |
| `Animation/OneHandSword/` (`OH_*`), `TwoHandSword/` (`TH_*`) | Greystone 한손검, Kwang 대검 모션. `OH_Attack_A~D_Slow(+_Recovery)`는 루트 모션 사용 |
| `Animation/OneHandSword/Montages/AM_OH_LightCombo` | 섹션 A→A_Rec, B→B_Rec, C→C_Rec, D→D_Rec. 판정 구간: 각 공격 시작 +0.05초부터 0.27초(손 속도 분석). 콤보 입력 구간: A·B·C 시작 +0.6초부터 0.5초 |
| `Animation/Common/Montages/AM_HitReact_Front/Back/Left/Right` | 경직 몽타주 (`OH_HitReact_*`, 루트 모션) |
| `Animation/Common/` (`CM_Roll_*` 8방향) | 구르기 모션. `Montages/AM_Roll_*` 8개 (`DefaultSlot`) |
| `Input/Actions/` | `IA_Move`, `IA_Look`, `IA_Attack_Light`, `IA_Attack_Heavy`, `IA_Dodge`, `IA_Defense`, `IA_Skill_1`, `IA_Skill_2`, `IA_LockOn`, `IA_UseItem` |
| `Input/IMC_Boss`, `Input/DA_InputConfig_Player` | 보스전 키 매핑 (좌클릭 약공격, 우클릭 강공격, Shift 방어, Space 회피), 입력 설정 |
| `AbilitySystem/Effects/` | `GE_Init_Player`(초기 수치), `GE_Cost_Dodge`(스태미나 -20), `GE_Cost_Attack_Light`(-10), `GE_Regen_Stamina`(0.1초마다 회복 속도×0.1), `GE_Init_Enemy_Dummy`(체력 1000, 강인도 40), `GE_Damage_Melee`(피해 계산), `GE_Recover_Poise`(강인도 최대치) |
| `AbilitySystem/Abilities/` | `GA_Dodge`, `GA_StaminaRegen`, `GA_Attack_Light`(타격 피해 A10·B10·C12·D18, 강인도 10·10·12·20), `GA_HitReact` |
| `AbilitySystem/DA_AbilitySet_Player` | `GE_Init_Player` + `GA_Dodge`(Space) + `GA_StaminaRegen`(패시브). 공격은 무기 세트로 이동 |
| `AbilitySystem/DA_AbilitySet_Weapon_OneHandSword` | `GA_Attack_Light`(좌클릭), `GA_Attack_Heavy_OH`(우클릭), `GA_Defense_Parry`(Shift) |
| `AbilitySystem/DA_AbilitySet_Weapon_TwoHandSword` | `GA_Attack_Light_TH`(4타), `GA_Attack_Heavy_TH`(내려치기), `GA_Defense_Guard`(Shift 유지) |
| `Equipment/Weapons/DA_Weapon_TwoHandSword` | Atris `SM_Sword1k`, 평소 `hand_r`(Kwang 대기 그립), **공격·방어 중 `ik_hand_gun`**(TH 모션에 Kwang `weapon_r` 궤적을 구워 넣음), 판정 z -12 ~ -126 |
| `AbilitySystem/DA_AbilitySet_Enemy_Dummy` | `GE_Init_Enemy_Dummy` + `GA_HitReact` + `GA_StaminaRegen` (무기: 한손검) |
| `Equipment/Weapons/DA_Weapon_OneHandSword` | Atris `SM_Sword1f`, `hand_r` + 보정(손가락 본 기준 계산), 판정 z -12 ~ -82. `BP_ODPlayer`의 기본 무기 |
| `Characters/Enemies/BP_ODEnemy_Dummy` | `AODTrainingDummy` 기반 허수아비 (Quinn + `ABP_OD_Humanoid`, 한손검). 레벨의 `Dummy_Left`만 2.5초마다 반격 |
| `Characters/BP_ODPlayer` | 플레이어 (Manny + `ABP_OD_Humanoid`) |
| `Characters/Animation/ABP_OD_Humanoid` | `ABP_Unarmed` 복제본. **몽타주 재생 중에는 발 IK 컨트롤 릭(CR_Mannequin_FootIK) Alpha 0**(보간), IK 트레이스도 끔. 플레이어·허수아비 공용. 이후 무기별 Linked Anim Layer의 기반 |
| `Core/BP_ODGameMode` | 기본 폰 `BP_ODPlayer`. 프로젝트 전역 GameMode |
| `Maps/L_OD_Test_Combat` | **전투 테스트 맵 (게임·에디터 시작 맵).** 80m 평지, 허수아비 3개(정면 6m, 좌우 10m) |
| `Maps/L_OD_Prototype` | 템플릿 레벨 복제본 (지형 테스트용) |

---

## 3. 진행 중

- 없음. 다음: 무기별 이동 애니메이션 레이어(Linked Anim Layer, 승인됨 A안)

### 최근 검증 기록
- **회피 (2026-11-13):** 입력 방향 회전 후 구르기 약 3m, 입력 없으면 뒤 구르기, 스태미나 20 소모, 0이면 발동 불가, 무적 약 0.08~0.48초.
- **한손검·대검 (2026-11-13):** 한손검 약공격 5회 90·강공격 40, 대검 약공격 6회 144·강공격 60(공격력 10 포함). 패링: 허수아비 공격 시작 직후 Shift → 피해 0, 허수아비 긴 경직. 대검 가드 6초: 피해 10 → 2, 스태미나 10씩 소모. Tab 교체·F5 초기화 정상.
- **발 IK 수정 (2026-11-13):** 템플릿 ABP의 발 IK가 몽타주 위에 적용되어 구르기·공격 중 두 발이 바닥에 고정되던 문제. 수정 후 구르기·공격 중 발 높이가 원본 애니메이션과 일치(PIE 측정).
- **무기 판정 (2026-11-13):** 허수아비 정면 콤보 시 A·B·C·D 모두 적중(피해 20·20·22·28 = 기본+공격력 10), 강인도 0에서 경직 → 경직 종료 후 강인도 40 회복.
- **약공격 (2026-11-13):** 단타 A→A_Rec 후 종료, 연타 시 A→B→C→D→D_Rec, 타격마다 스태미나 10 소모.
- **스태미나 회복 (2026-11-13):** 마지막 소모 1.51초 후 회복 시작, 초당 25 회복, 회복 중 소모 시 즉시 정지 후 1.5초 대기 재시작, 가득 차면 회복 GE 제거.

## 4. 다음 할 일 (순서는 사용자와 조율)

1. 무기별 이동 애니메이션 레이어 (Greystone·Kwang 이동 모션 리타겟, Linked Anim Layer)
2. 락온 (회피 8방향 활성화)
3. 전투 모드 관리 컴포넌트 (강공격·방어를 `Mode.Boss`로 제한)
4. 부활, 피격 시 플레이어 경직 외 피격 모션 (`State.LockOn`, `GetLockOnTarget()` 구현 → 회피 8방향 활성화)
5. 전투 모드 관리 컴포넌트 구조안 승인 → 필드/보스전 전환, `IMC_Field`
6. 프로토타입: 필드 1구간 + Stone Golem 보스전 + 모드 전환 컷신

## 5. 알려진 이슈 / 임시 처리

- 피해 계산식, 수치(스태미나 비용, 회복 속도 등)는 모두 초안입니다.
- 락온이 없어서 회피 8방향은 아직 실제로 동작하지 않습니다.
- 강공격·방어는 아직 모드 제한(`Mode.Boss`)이 없습니다. 모드 컴포넌트 구현 시 추가합니다.
- 방어 수치(정면 ±75°, 가드 감소 80%, 스태미나 1:1)는 `ODDamageExecution`의 상수입니다. 무기 데이터로 옮길 예정.
- 대검 가드 모션은 전용 모션이 없어 Kwang `FrontEndPose_01`(머리 위로 대검을 가로로 든 포즈)을 반복합니다.
- 대검 강공격은 Kwang `Ability_R`(내려치기). `Ability_RMB`는 공격 판정이 맞지 않아 제외.
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
  - 몽타주 섹션은 Python으로 만들 수 없어 `unreal.ODMontageEditorLibrary.build_sectioned_montage(montage, sequences, section_names, next_section_names)`를 씁니다. 노티파이는 `unreal.AnimationLibrary.add_animation_notify_state_event`.
  - 새 레벨은 `LevelEditorSubsystem.new_level_from_template`을 단독 호출로 만들고, 액터 배치는 별도 호출로 합니다.
  - 애니메이션의 본 위치는 `unreal.AnimPoseExtensions.get_anim_pose_at_time` + `get_bone_pose`로 읽을 수 있습니다. 판정 구간은 손 속도가 높은 구간으로 잡았습니다.
  - 애님 그래프 편집: `BlueprintToolsets.BlueprintToolset`의 FindNodes / GetNodeInfos / CreateNode / ConnectPins / SetPinValue가 AnimGraph에서도 동작합니다(Graph DSL은 AnimGraph 미지원). 노드 구조체 값은 `node.export_text()` → 수정 → `import_text`로 바꿀 수 있으나 핀 구성은 바뀌지 않습니다.
  - **PIE 실행 중에는 에셋 저장이 실패합니다.** 에셋 수정은 PIE를 멈춘 뒤 하고, `EditorLoadingAndSavingUtils.save_dirty_packages`로 저장을 확인합니다. (PIE 중 수정한 몽타주 노티파이가 재시작 후 사라진 사례)
  - 리타겟 결과로 에셋을 교체할 때 `consolidate_assets` + 이름 변경은 리다이렉터 문제가 생깁니다. 기존 에셋 삭제 → 새로 리타겟 → 같은 이름으로 이동 → 몽타주 재구성 순서가 안전합니다.
  - Paragon 무기 본(`weapon_r`)은 손과 따로 움직입니다. Manny에는 없으므로 `AnimDataController.set_bone_track_keys`로 `ik_hand_gun`에 손 기준 상대 궤적을 구워 넣었습니다(대검 TH_ 모션). 대검 소켓 위치: Kwang `FX_weapon_base`(y -20), `FX_weapon_tip`(y -149) = 칼날 -Y.
  - 템플릿 ABP(`ABP_Unarmed`)는 수정하지 않고 `ABP_OD_Humanoid`를 씁니다.
  - Live Coding이 켜진 에디터가 살아 있으면 `ValidateBuild.bat`가 실패합니다("Unable to build while Live Coding is active"). 에디터가 완전히 종료된 뒤 실행해야 합니다.
  - PIE 입력 테스트: `InputToolset.ApplyInput`(액션 경로 지정), 상태 확인은 Python에서 ASC 태그·Attribute를 프레임 단위로 샘플링합니다.
- **파일 규칙:** C++ 소스는 UTF-8 BOM + CRLF, 한글 주석.

---

## 7. 이 문서 갱신 규칙

- 작업 단위(기능 하나)가 끝날 때마다 1~5장을 갱신하고, 커밋에 함께 포함합니다.
- 새로 알게 된 환경 문제와 해결법은 6장에 추가합니다.
