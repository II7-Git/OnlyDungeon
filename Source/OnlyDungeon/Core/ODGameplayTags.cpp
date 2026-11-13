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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Defense, "InputTag.Defense", "방어 (무기에 따라 패링 또는 가드)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_1, "InputTag.Skill.1", "스킬 1 (오른손 무기)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Skill_2, "InputTag.Skill.2", "스킬 2 (왼손 무기)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_LockOn, "InputTag.LockOn", "락온 전환");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_UseItem, "InputTag.UseItem", "회복 아이템 사용");

	// ---------- 전투 모드 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Field, "Mode.Field", "필드: 쿼터뷰 핵앤슬래시");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mode_Boss, "Mode.Boss", "보스방: 3인칭 소울라이크");

	// ---------- 상태 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "사망");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "공격 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "회피 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Defending, "State.Defending", "방어 중 (패링/가드)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "경직 (강인도 붕괴, 패링당함 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "무적 (회피 무적 시간 등)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_LockOn, "State.LockOn", "락온 중");

	// ---------- 이벤트 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit, "Event.Hit", "피격됨");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "사망함");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_PoiseBroken, "Event.PoiseBroken", "강인도 붕괴");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_ParrySuccess, "Event.ParrySuccess", "패링 성공");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_HitTrace, "Event.Montage.HitTrace", "공격 몽타주의 판정 구간 (AnimNotify → 어빌리티)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Melee_Hit, "Event.Melee.Hit", "내 무기가 대상을 맞힘 (무기 → 공격 어빌리티)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_ComboWindow, "Event.Montage.ComboWindow", "콤보 입력 구간 (열림 1 / 닫힘 0, AnimNotify → 어빌리티)");

	// ---------- 데이터 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller: 기본 피해량");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_PoiseDamage, "Data.PoiseDamage", "SetByCaller: 강인도 피해량");

	// ---------- 쿨다운 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_1, "Cooldown.Skill.1", "스킬 1 쿨다운");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Skill_2, "Cooldown.Skill.2", "스킬 2 쿨다운");

	// ---------- 무기 종류 ----------
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_OneHandSword, "Weapon.Type.OneHandSword", "한손검");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_TwoHandSword, "Weapon.Type.TwoHandSword", "양손 대검");
}
