// OnlyDungeon 네이티브 Gameplay Tag 정의

#include "ODGameplayTags.h"

namespace ODGameplayTags
{
	// ---------- 입력 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Move, "InputTag.Move", "이동 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Look, "InputTag.Look", "시점 입력");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Light, "InputTag.Attack.Light", "약공격 (필드에서는 평타)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Attack_Heavy, "InputTag.Attack.Heavy", "강공격 (보스전 전용)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Dodge, "InputTag.Dodge", "회피 (구르기)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Sprint, "InputTag.Sprint", "달리기 (회피 키를 길게 누르면 플레이어가 발동)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Defense, "InputTag.Defense", "방어 (무기에 따라 패링 또는 가드)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_1, "InputTag.Skill.1", "스킬 1 (오른손 무기)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_2, "InputTag.Skill.2", "스킬 2 (왼손 무기)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_LockOn, "InputTag.LockOn", "락온 전환");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_UseItem, "InputTag.UseItem", "회복 아이템 사용");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Interact, "InputTag.Interact", "상호작용 (문 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Debug_SwapWeapon, "InputTag.Debug.SwapWeapon", "[테스트] 무기 교체");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Debug_Reset, "InputTag.Debug.Reset", "[테스트] 모든 캐릭터 상태 초기화");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Debug_GoToBossDoor, "InputTag.Debug.GoToBossDoor", "[테스트] 보스방 문 앞으로 이동");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Pause, "InputTag.Pause", "일시정지 메뉴 (게임이 멈춘 동안에도 받도록 입력 액션의 bTriggerWhenPaused를 켠다)");

	// ---------- 전투 모드 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Field, "Mode.Field", "필드: 쿼터뷰 핵앤슬래시");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Boss, "Mode.Boss", "보스방: 3인칭 소울라이크");

	// ---------- 상태 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "사망");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "공격 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Cancelable, "State.Cancelable", "캔슬 구간: 지금 행동을 회피로 끊을 수 있음");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_ComboBranch, "State.ComboBranch", "약공 콤보에서 파생 공격(강공)으로 발동되는 중 (ASC가 발동 직전에만 잠시 부여)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Parrying, "State.Parrying", "패링 판정 구간 (이 동안 정면 공격을 받으면 패링 성공)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "회피 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defending, "State.Defending", "가드 중 (정면 공격 피해 감소, 스태미나로 막음)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "경직 (강인도 붕괴, 패링당함 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "무적 (회피 무적 시간 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_LockOn, "State.LockOn", "락온 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Drinking, "State.Drinking", "회복 물약을 마시는 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Resting, "State.Resting", "세이브 포인트에서 휴식 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Sprinting, "State.Sprinting", "달리는 중 (스태미나 지속 소모)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SwappingWeapon, "State.SwappingWeapon", "무기 교체 중 (넣었다 꺼내는 모션)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Phase_1, "State.Phase.1", "보스 1페이즈");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Phase_2, "State.Phase.2", "보스 2페이즈 (체력이 일정 비율 이하)");

	// ---------- 이벤트 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit, "Event.Hit", "피격됨");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "사망함");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_PoiseBroken, "Event.PoiseBroken", "강인도 붕괴");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_ParrySuccess, "Event.ParrySuccess", "패링 성공");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_GuardBlocked, "Event.GuardBlocked", "가드로 공격을 막음");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_HitTrace, "Event.Montage.HitTrace", "공격 몽타주의 판정 구간 (AnimNotify → 어빌리티)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Melee_Hit, "Event.Melee.Hit", "내 무기가 대상을 맞힘 (무기 → 공격 어빌리티)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_CancelWindow, "Event.Montage.CancelWindow", "캔슬 구간 (열림 1 / 닫힘 0, AnimNotify → 어빌리티)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_WeaponSwap, "Event.Montage.WeaponSwap", "무기 교체 구간 (1 = 무기를 등에 넣음 → 다음 무기로 교체, 0 = 새 무기를 꺼내 손에 쥠)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combo_BranchInput, "Event.Combo.BranchInput", "공격 중 파생 공격 입력 (ASC → 진행 중인 콤보, InstigatorTags에 입력 태그)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Action, "Ability.Action", "공격·방어·회피처럼 캔슬 구간에서 끊길 수 있는 행동 어빌리티");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_ComboWindow, "Event.Montage.ComboWindow", "콤보 입력 구간 (열림 1 / 닫힘 0, AnimNotify → 어빌리티)");

	// ---------- 데이터 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller: 기본 피해량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_PoiseDamage, "Data.PoiseDamage", "SetByCaller: 강인도 피해량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StaminaRegen, "Data.StaminaRegen", "SetByCaller: 이번 회복 틱의 스태미나 회복량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_StaminaCost, "Data.StaminaCost", "SetByCaller: 이번 틱의 스태미나 변화량 (소모는 음수, 달리기)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Heal, "Data.Heal", "SetByCaller: 이번 회복 틱의 체력 회복량 (물약)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Item, "Ability.Item", "아이템 사용 어빌리티 (물약 등). 행동(Ability.Action)과 구분");

	// ---------- 쿨다운 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_1, "Cooldown.Skill.1", "스킬 1 쿨다운");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_2, "Cooldown.Skill.2", "스킬 2 쿨다운");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Boss_Intro, "Event.Boss.Intro", "보스가 전투를 시작함 (등장 포효)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Boss_PhaseChange, "Event.Boss.PhaseChange", "보스가 다음 페이즈로 넘어감 (전환 포효)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Rest, "Event.Rest", "세이브 포인트에서 휴식 시작 (OptionalObject = 세이브 포인트)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_SwapRequest, "Event.Weapon.SwapRequest", "무기 교체 요청 (Tab, OptionalObject = 꺼낼 무기 데이터)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Hit, "GameplayCue.Hit", "근접 공격 피격 이펙트 (피해 GE 실행 시, 맞은 캐릭터의 HitImpactEffect)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Parry, "GameplayCue.Parry", "패링 성공 연출 (막은 캐릭터에서 실행, 금속 충돌음 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_GuardBlock, "GameplayCue.GuardBlock", "가드로 막음 연출 (막은 캐릭터에서 실행, 금속 충돌음 등)");

	// ---------- 무기 종류 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_OneHandSword, "Weapon.Type.OneHandSword", "한손검");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_TwoHandSword, "Weapon.Type.TwoHandSword", "양손 대검");
}
