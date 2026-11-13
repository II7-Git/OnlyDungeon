# OnlyDungeon 기획서

> 기획 결정의 기준 문서입니다. 코드나 에셋 작업 중 기획 판단이 필요하면 이 문서를 먼저 확인합니다.
> 기획 변경은 사용자 승인 후에만 이 문서에 반영합니다. 미정 항목은 임의로 정하지 않고 사용자에게 묻습니다.
>
> 최종 수정: 2026-11-13

---

## 1. 핵심 콘셉트

쿼터뷰 핵앤슬래시로 잡몹을 잡고 장비를 모으다가, 보스방에 들어서면 컷신과 함께 3인칭 소울라이크 보스전으로 전환되는 액션 게임입니다. 성장은 오직 장비로만 하며, 두 전투 모드는 같은 모션과 같은 GAS 어빌리티를 공유합니다.

| 항목 | 결정 |
|---|---|
| 게임 구조 | 필드(쿼터뷰 핵앤슬래시) + 보스방(3인칭 소울라이크) |
| 세계관 | 다크 판타지 우선 |
| 개발 범위 | 수직 슬라이스 (필드 구간 1~2개 + 보스 1~2마리) |
| 엔진 / 프레임워크 | UE 5.9, GAS, 싱글플레이 |

---

## 2. 게임 흐름

```
필드 구간(쿼터뷰 파밍) → 보스방 문 → 입장 컷신(쿼터뷰 → 3인칭) → 보스전(3인칭)
                                                                  ├─ 승리 → 보스 장비 획득
                                                                  └─ 사망 → 보스방 앞 부활
보스방 앞 부활 → 재도전(보스방 문) 또는 다시 파밍(필드)
```

- 사망 시 보스방 앞 리스폰 포인트에서 부활합니다.
- 부활하면 **잡몹과 보스가 모두 초기화**됩니다.
- **주운 아이템과 재화는 유지**됩니다.

---

## 3. 두 전투 모드

회피, 약공격(필드에서는 평타), 스킬 2개는 **같은 모션과 같은 GAS 어빌리티**를 공유하고, 카메라·입력·조준 방향·판정 방식만 모드에 따라 바뀝니다.

| 항목 | 필드 (쿼터뷰 핵앤슬래시) | 보스방 (3인칭 소울라이크) |
|---|---|---|
| 카메라 | 위에서 내려다보는 쿼터뷰 | 캐릭터 뒤 3인칭, 락온 |
| 이동 | WASD | WASD |
| 조준 방향 | 마우스 커서 방향 | 락온 대상 또는 카메라 정면 |
| 사용 액션 | 평타, 회피, 스킬 2개 | 약공격, 강공격, 회피, 패링, 스킬 2개 |
| 스태미나 | 사용 | 사용 |
| 공격 판정 | 범위 판정 위주 (다수 적) | 무기 궤적 판정 (1:1) |
| 캔슬 감각 | 빠른 캔슬 | 묵직한 후딜레이 |
| 잡몹 밀도 | 한 화면 20마리 내외 | - |

- 입력 매핑(IMC)은 모드별로 2세트를 두고, 컷신이 끝날 때 교체합니다.
- **기본 키 배치** (InputTag 방식이라 키 커스텀으로 변경 가능)

| 행동 | 키 | InputTag |
|---|---|---|
| 이동 / 시점 | WASD / 마우스 | `InputTag.Move` / `InputTag.Look` |
| 약공격 | 좌클릭 | `InputTag.Attack.Light` |
| 강공격 | 우클릭 | `InputTag.Attack.Heavy` |
| 방어 (패링·가드) | Left Shift | `InputTag.Defense` |
| 회피 | Space | `InputTag.Dodge` |
| 스킬 1 / 스킬 2 | Q / E | `InputTag.Skill.1` / `.2` |
| 락온 | 마우스 휠 클릭 | `InputTag.LockOn` |
| 회복 | R | `InputTag.UseItem` |

- 어빌리티는 키가 아니라 InputTag에 연결합니다.
- 모드가 바뀔 때 함께 바뀌는 것: 카메라, 입력 매핑, 캐릭터 회전 방식, 락온, HUD.
- 콤보는 쓸 만한 콤보 애니메이션이 있을 때만 구현합니다.
- **회피(구르기):** 두 모드 공통, 락온 여부에 따라 방식이 다릅니다.
    - **락온 없음:** 입력한 방향으로 즉시 돌아선 뒤 앞구르기 (`CM_Roll_Front`)
    - **락온 중:** 대상을 바라본 채 입력 방향에 맞는 **8방향 구르기** (`CM_Roll_Front/Back/Left/Right/FrontLeft/FrontRight/BackLeft/BackRight`)
    - 모두 1.17초, 루트 모션으로 약 3.3~3.5m 이동. 경로 `/Game/OnlyDungeon/Animation/Common`

---

## 4. 장비 시스템

장비가 수치와 스킬을 모두 결정합니다. **고정 장비**라서 같은 이름의 장비는 항상 같은 성능입니다.

| 슬롯 | 결정하는 것 | 부여 스킬 |
|---|---|---|
| 오른손 무기 | 공격력, 모션 세트(평타·약공·강공), 스태미나 소모량 | 스킬 1 |
| 왼손 무기 | 보조 무기 성능 | 스킬 2 |
| 방어구 | 방어력, 강인도, 무게 | 없음 |
| 장신구 | 특수 효과 (예: 패리 판정, 스태미나 회복 속도) | 없음 |

- **무기 종류:** 한손검(Greystone 모션), 양손 대검(Kwang 모션)
- **방어 방식은 무기마다 다릅니다.** 무기군마다 고유한 방어·반격 모션이 있으면 그것을 사용합니다.
    - 한손검: 패링 (`Deflect_InPlace`)
    - 양손 대검: 패링 없이 **가드(막기)** — 전용 모션 확보 필요
- **한 손만 장착한 경우:** 스킬 1만 쓸 수 있는 대신 스킬 1의 쿨타임 0.75배, 피해량 1.5배 보정 (수치는 초안)
- 무게가 무거울수록 방어력은 높고 회피는 둔해집니다.
- 장비는 DataAsset으로 정의합니다. 메시, 몽타주, 수치 이펙트, 부여 스킬을 데이터로 연결해서, 데이터만 바꾸면 무기나 애니메이션을 교체할 수 있게 합니다.

---

## 5. 회복, 재화

- **회복:** 횟수 제한 회복 아이템을 필드와 보스전 모두에서 사용합니다. 리스폰 포인트에서 쉬거나 사망하면 다시 채워집니다.
- **재화:** 당분간 사용처 없음 (보관만)

---

## 6. GAS 설계 방향

모든 전투는 GAS로 구현합니다. 구체적인 구조는 구현 전에 별도로 승인받습니다 (CLAUDE.md 1장).

### 6-1. 승인된 뼈대 구조 (2026-11-13)
| 결정 | 내용 |
|---|---|
| ASC 위치 | **캐릭터**에 둔다. 사망해도 액터를 파괴하지 않고, 부활은 위치 이동 + 상태 초기화로 처리 |
| AttributeSet | 2개 분리: `UODAttributeSet_Health`(체력, 강인도, 메타 피해) / `UODAttributeSet_Combat`(스태미나, 공격력, 방어력, 무게) |
| 입력 | InputTag 방식. `UODInputConfig`(입력 액션 ↔ InputTag)로 연결 → 키 커스텀 대응 |
| 발동 방식 | `OnInputTriggered` / `WhileInputActive` / `OnSpawn`. 기능에 맞게 지정하고 플레이 테스트 후 조정 |
| 피해 처리 | 피해 GE → `UODDamageExecution`(공격력·방어력) → `IncomingDamage` → Health 차감. 강인도도 같은 방식. Health 0 → `State.Dead` + `Event.Death` (계산식은 임시, 추후 확정) |

| 클래스 | 경로 (`Source/OnlyDungeon/`) | 역할 |
|---|---|---|
| `ODGameplayTags` | `Core/` | 네이티브 Gameplay Tag 모음 |
| `UODAbilitySystemComponent` | `AbilitySystem/` | InputTag로 어빌리티 발동/해제 |
| `UODGameplayAbility` | `AbilitySystem/` | 모든 어빌리티의 기반, 발동 방식 |
| `UODAbilitySet` | `AbilitySystem/` | 함께 부여·회수되는 어빌리티·이펙트 묶음 (기본 능력, 장비 스킬) |
| `UODAttributeSet_Health`, `_Combat` | `AbilitySystem/Attributes/` | 수치 |
| `UODDamageExecution` | `AbilitySystem/Executions/` | 피해 계산 |
| `AODCharacterBase` | `Character/` | ASC·AttributeSet 보유, 사망·강인도 붕괴 처리 |
| `AODPlayerCharacter` | `Character/` | 입력, 카메라 |
| `AODEnemyCharacter` | `Character/` | 잡몹·보스 공통 기반 |
| `UODInputConfig` | `Input/` | 입력 액션 ↔ InputTag 매핑 |

### 6-2. 기본 에셋 (2026-11-13 생성)
| 에셋 | 경로 | 내용 |
|---|---|---|
| 입력 액션 10개 | `/Game/OnlyDungeon/Input/Actions/` | `IA_Move`, `IA_Look`(템플릿 복제), 버튼 액션 8개 |
| `IMC_Boss` | `/Game/OnlyDungeon/Input/` | 보스전(3인칭) 키 매핑. 필드용 `IMC_Field`는 모드 전환 단계에서 |
| `DA_InputConfig_Player` | `/Game/OnlyDungeon/Input/` | 입력 액션 ↔ InputTag |
| `GE_Init_Player` | `/Game/OnlyDungeon/AbilitySystem/Effects/` | 초기 수치: 체력 100, 강인도 50, 스태미나 100(회복 25/초), 공격력 10, 방어력 0, 무게 0 (초안) |
| `DA_AbilitySet_Player` | `/Game/OnlyDungeon/AbilitySystem/` | `GE_Init_Player` + `GA_Dodge`(`InputTag.Dodge`) + `GA_StaminaRegen`(패시브) + `GA_Attack_Light`(`InputTag.Attack.Light`) |
| `BP_ODPlayer` | `/Game/OnlyDungeon/Characters/` | `AODPlayerCharacter` 기반, Manny 메시 + `ABP_Unarmed` |
| `BP_ODGameMode` | `/Game/OnlyDungeon/Core/` | 기본 폰 `BP_ODPlayer`, 엔진 기본 PlayerController. 프로젝트 전역 GameMode |
| `L_OD_Test_Combat` | `/Game/OnlyDungeon/Maps/` | 전투 테스트 맵. 게임·에디터 시작 맵. 허수아비 `BP_ODEnemy_Dummy` 3개 |
| `L_OD_Prototype` | `/Game/OnlyDungeon/Maps/` | 템플릿 레벨 복제본 |
| `GA_Attack_Light` | `/Game/OnlyDungeon/AbilitySystem/Abilities/` | `UODGameplayAbility_MeleeCombo` 기반. `AM_OH_LightCombo`(A~D 4타), 1타당 스태미나 10(`GE_Cost_Attack_Light`) |
| `GA_Dodge` | `/Game/OnlyDungeon/AbilitySystem/Abilities/` | `UODGameplayAbility_Dodge` 기반. 상태 `State.Dodging`, 차단 Dead/Stunned/Dodging |
| `GE_Cost_Dodge` | `/Game/OnlyDungeon/AbilitySystem/Effects/` | 스태미나 -20 (초안) |
| `GA_StaminaRegen` | `/Game/OnlyDungeon/AbilitySystem/Abilities/` | `UODGameplayAbility_StaminaRegen` 기반 패시브 |
| `GE_Regen_Stamina` | `/Game/OnlyDungeon/AbilitySystem/Effects/` | 무한 지속, 0.1초마다 `StaminaRegenRate × 0.1` 회복 |
| `AM_Roll_*` 8개 | `/Game/OnlyDungeon/Animation/Common/Montages/` | `CM_Roll_*` 기반 몽타주, `DefaultSlot`, 루트 모션 |

**회피 구현 (2026-11-13, PIE 검증):** 비락온은 입력 방향으로 즉시 회전 후 정면 구르기, 입력이 없으면 뒤로 구르기. 락온 중에는 대상을 바라보고 8방향 구르기(락온 시스템 구현 후 동작). 무적은 시작 0.05초부터 0.4초간 `State.Invulnerable`이며, 이 태그가 있으면 피해 계산이 피해·강인도 피해를 무시합니다. 스태미나가 부족하면 발동하지 않습니다.

**약공격 콤보 (2026-11-13, 판정 제외):** 몽타주 하나를 섹션으로 나눈 4타 콤보(Greystone `Attack_A~D_Slow` + `_Recovery`). 공격 중 입력은 예약되고, 콤보 입력 구간(공격 끝부분~후딜 앞부분)에서 다음 타격으로 넘어갑니다. 입력이 없으면 후딜까지 재생하고 끝납니다. 타격마다 스태미나 10 소모, 입력 방향으로 회전. 회피로 공격을 끊을 수 있습니다.

**무기와 적중 판정 (2026-11-13):** 무기는 `UODWeaponData` 데이터 에셋으로 정의하고 장비 컴포넌트가 장착합니다(메시 부착, 무기 어빌리티 세트·타입 태그·이동 애니메이션 레이어 적용). 판정은 공격 모션의 판정 구간 동안 칼날을 따라 스윕하며 한 번 휘두를 때 대상당 한 번만 맞습니다. 피해 = 타격별 기본 피해 + 공격력 - 방어력, 강인도 피해는 타격별 값. 강인도가 0이 되면 공격자 방향에 맞는 경직 후 강인도가 회복됩니다. 무기 메시: 한손검 Atris `SM_Sword1f`, 대검 `SM_Sword1k`(예정).

**한손검·대검 (2026-11-13):**
| | 한손검 | 대검 |
|---|---|---|
| 약공격 | 4타 콤보, 10·10·12·18 / 스태미나 10 | 4타 콤보, 22·22·25·35 / 스태미나 15 |
| 강공격 | Greystone `Ability_E`(1.27초), 30 / 스태미나 25, 80cm 전진 | Kwang `Ability_R` 내려치기, 50 / 스태미나 35, 60cm 전진 |
| 방어 | 패링(Shift 1회, 판정 0.03~0.28초, 스태미나 10). 성공 시 피해 0, 공격자 긴 경직 | 가드(Shift 유지). 피해 80% 감소, 강인도 피해 없음, 스태미나 소모, 부족하면 가드 붕괴 |
수치는 모두 초안입니다(피해 = 기본 + 공격력 - 방어력). 패링·가드는 정면 ±75°만 유효합니다.

**스태미나 자연 회복:** 스태미나를 소모하는 행동이 없는 상태로 1.5초가 지나면 회복을 시작합니다(초당 `StaminaRegenRate`, 현재 25). 회복 중 소모가 생기면 즉시 멈추고 다시 1.5초를 기다립니다. 가득 차면 회복 GE를 제거합니다.

### 6-3. 기본 방향

- **Attribute:** 체력, 스태미나, 강인도, 공격력, 방어력, 무게
- **공유 어빌리티:** 회피, 약공격(=평타), 스킬 1·2
- **보스 전용 어빌리티:** 강공격, 방어(무기별로 패링 또는 가드). 방어 어빌리티는 무기 데이터가 지정하는 방식으로 동작
- **모드 태그:** `Mode.Field` / `Mode.Boss`. 어빌리티가 이 태그를 보고 조준 방향, 판정 방식, 캔슬 타이밍을 고릅니다. 강공격과 패링은 `Mode.Boss`에서만 활성화됩니다.
- **입력 태그:** `InputTag.Attack.Light`, `InputTag.Attack.Heavy`, `InputTag.Dodge`, `InputTag.Parry`, `InputTag.Skill.1`, `InputTag.Skill.2`
- **장비 연동:** 착용 시 GameplayEffect로 수치 적용 + 스킬 어빌리티 부여, 해제 시 회수
- **모드 전환 담당:** 전투 모드 관리 컴포넌트(예: `UODCombatModeComponent`)가 카메라, 입력 매핑, 회전 방식, 락온, HUD, 모드 태그를 한꺼번에 바꿉니다.
- **보스 AI:** StateTree, 페이즈는 `State.Phase.*` 태그로 관리

---

## 7. 에셋 방침과 현황

- **전투 애니메이션은 Paragon(Epic 무료 공개)을 우선 활용합니다.** 유료 에셋은 사용하지 않습니다.
- Paragon 애니메이션은 자체 스켈레톤을 쓰므로 UE5 Manny로 리타겟해서 씁니다 (에디터 작업, 진행 전 승인).
- 리타겟 설정: `/Game/OnlyDungeon/Animation/Retarget` (`IK_OD_*`, `RTG_OD_*_To_Manny`). 새 Paragon 모션도 이 리타게터로 추가합니다.
- 리타겟본만 게임에서 사용하고 원본 Paragon 에셋은 수정하지 않습니다.

| 구분 | 에셋 | 경로 | 상태 |
|---|---|---|---|
| 양손 대검 모션 | Paragon: Kwang | `/Game/ParagonKwang` → 리타겟본 `/Game/OnlyDungeon/Animation/TwoHandSword` (`TH_*`, 17개) | 리타겟 완료 |
| 한손검 모션 | Paragon: Greystone | `/Game/ParagonGreystone` → 리타겟본 `/Game/OnlyDungeon/Animation/OneHandSword` (`OH_*`, 28개) | 리타겟 완료 |
| 회피 모션 | Dynamic Falling & Rolling Animation Pack | `/Game/DynamicFalling` → 리타겟본 `/Game/OnlyDungeon/Animation/Common/CM_Roll_*` (8방향) | 리타겟·루트 모션 적용 완료 |
| 무기 메시 | Atris swords | `/Game/Atris_swords` | 설치 완료 |
| 피격·사망 (예비) | UE5 마네킹 기본 `MM_HitReact_*`, `MM_Death_*` | `/Game/Characters/Mannequins/Anims` | 템플릿에 포함 |
| 환경 장식 | Dead Bodies Sitting & Lying Poses (정지 포즈 30종) | `/Game/DeadBodies_Poses_nikoff` | 설치 완료 |
| 보스 | Armored Stone Golem - Dark Fantasy Boss | - | 라이브러리 보유, 미설치 |
| 던전 환경 | Medieval Dungeon (UE 5.4), Underground Tunnel | - | 라이브러리 보유, 미설치 |

### 전투 모션 후보 (Paragon)
| 용도 | Kwang (양손 대검) | Greystone (한손검) |
|---|---|---|
| 약공격 / 콤보 | `PrimaryAttack_A~D_Slow` (+ `_Recovery`) | `Attack_A~D_Fast/Med/Slow` (+ `_Recovery`), `Attack_PrimaryA~C` |
| 강공격 / 스킬 | `Ability_RMB`, `Ability_R`, `Ability_Q_Throw` | `Attack_RMB`, `Ability_E/Q/R`, `Jump_Melee` |
| 패링 | - | **`Deflect_InPlace` (확정)** |
| 피격 | `Hitreact_Fwd/Bwd/Left/Right`, `Knockback_Fwd/Bwd` | `HitReact_Front/Back/Left/Right` |
| 경직 (패링당한 적) | `Stun_Start`, `Stun_Loop` | `Stun_Start`, `Stun_Idle` |
| 사망 | `Death_Bwd` | `Death` |

---

## 8. 미정 항목

- [x] 모션 배정: Greystone = 한손검, Kwang = 양손 대검
- [x] 패링 모션: Greystone `Deflect_InPlace`
- [ ] 양손 대검의 스킬 규칙: 스킬 1개만 쓰는지, 한손 장착과 같은 보정을 줄지
- [ ] 왼손 무기 종류: 한손검 외에 어떤 보조 무기(방패, 단검 등)를 둘지
- [x] 회피 모션: 락온 없음 = 돌아서 앞구르기, 락온 중 = 8방향 구르기
- [x] 양손 대검 방어: 패링 대신 가드(막기). 무기군별 고유 방어·반격 모션 사용 원칙
- [ ] 양손 대검 가드 모션 확보 (임시: Kwang `FrontEndPose_01` 정지 포즈)
- [ ] 잡몹 캐릭터 에셋

---

## 9. 다음 단계

1. ~~전투 모션 확정 → UE5 Manny 리타겟~~ (완료)
2. ~~GAS 기본 뼈대 구조안 승인~~ (완료) / ~~회피 어빌리티~~ (완료)
3. ~~스태미나 자연 회복~~ (완료) / ~~한손검 약공격 콤보~~ (완료, 판정 제외) / 무기·장비 구조, 적중 판정, 강공격, 방어(패링·가드), 락온
4. 전투 모드 관리 컴포넌트 구조안 승인
5. 프로토타입: 필드 1구간 + Stone Golem 보스전 + 모드 전환 컷신
