# -*- coding: utf-8 -*-
# 잡몹·보스 StateTree 스펙 (수치는 초안, 기획서 6-2 "적·보스 AI"와 맞출 것).
# st_builder.py를 먼저 exec한 뒤 exec한다. 수정 후 build_tree로 해당 에셋을 다시 만든다.
DONE = ('OnStateCompleted', 'GotoState', 'Root')

MINION = [
    # 경직 중에는 잠깐 기다렸다 다시 판단
    S('Stunned', conds=[('HasTag', {'tag': tag('State.Stunned')})], tasks=[('Delay', {'duration': 0.3})], trans=[DONE]),
    # 사거리 안: 약공 2타 → 잠깐 쉼
    S('Attack', conds=[('InRange', {'maxRange': 200.0})],
      tasks=[('Ability', {'inputTag': tag('InputTag.Attack.Light'), 'pressCount': 2})],
      trans=[('OnStateCompleted', 'GotoState', 'Cooldown')]),
    # 대상이 있으면 추적
    S('Chase', conds=[('HasTarget', {})], tasks=[('Chase', {'acceptanceRadius': 160.0})], trans=[DONE]),
    # 대상이 없으면 제자리로 돌아가 대기
    S('Idle', tasks=[('Home', {})], trans=[DONE]),
    # 공격 뒤 쉼 (Idle 뒤에 있어 선택되지 않고, 공격이 끝났을 때만 들어온다)
    S('Cooldown', tasks=[('Delay', {'duration': 1.6})], trans=[DONE]),
]

BOSS = [
    S('Stunned', conds=[('HasTag', {'tag': tag('State.Stunned')})], tasks=[('Delay', {'duration': 0.3})], trans=[DONE]),
    # 2페이즈 (체력 50% 이하): 쉼이 짧고, 약공 4타 → 강공 → 뒤로 회피
    S('Phase2', select='TrySelectChildrenInOrder', conds=[('HasTag', {'tag': tag('State.Phase.2')})], children=[
        S('P2_Attack', conds=[('InRange', {'maxRange': 260.0})],
          tasks=[('Ability', {'inputTag': tag('InputTag.Attack.Light'), 'pressCount': 4, 'pressInterval': 0.25})],
          trans=[('OnStateCompleted', 'GotoState', 'P2_Heavy')]),
        S('P2_Chase', conds=[('HasTarget', {})], tasks=[('Chase', {'acceptanceRadius': 220.0})], trans=[DONE]),
        S('P2_Idle', tasks=[('Home', {})], trans=[DONE]),
        S('P2_Heavy', tasks=[('Ability', {'inputTag': tag('InputTag.Attack.Heavy')})],
          trans=[('OnStateCompleted', 'GotoState', 'P2_Evade')]),
        S('P2_Evade', tasks=[('Ability', {'inputTag': tag('InputTag.Dodge')})],
          trans=[('OnStateCompleted', 'GotoState', 'P2_Rest')]),
        S('P2_Rest', tasks=[('Delay', {'duration': 0.4})], trans=[DONE]),
    ]),
    # 1페이즈: 사거리 안에서 약공 3타 또는 강공을 무작위로 → 긴 쉼
    S('P1_Attack', select='TrySelectChildrenAtRandom', conds=[('InRange', {'maxRange': 260.0})], children=[
        S('P1_Combo', tasks=[('Ability', {'inputTag': tag('InputTag.Attack.Light'), 'pressCount': 3})],
          trans=[('OnStateCompleted', 'GotoState', 'P1_Rest')]),
        S('P1_Heavy', tasks=[('Ability', {'inputTag': tag('InputTag.Attack.Heavy')})],
          trans=[('OnStateCompleted', 'GotoState', 'P1_Rest')]),
    ]),
    S('P1_Chase', conds=[('HasTarget', {})], tasks=[('Chase', {'acceptanceRadius': 220.0})], trans=[DONE]),
    S('P1_Idle', tasks=[('Home', {})], trans=[DONE]),
    S('P1_Rest', tasks=[('Delay', {'duration': 1.2})], trans=[DONE]),
]

# ---- 패턴 기반 AI (적 전투 컴포넌트 + 패턴 데이터 DA_Pattern_*). 2026-11-17
# 우선순위: 경직 → 빈틈 공략 → 공격(공통 간격·쿨타임·사거리) → 멀면 추격 → 간 보기(주위 돌기) → 발견 전 행동
GOLEM = [
    S('Stunned', conds=[('HasTag', {'tag': tag('State.Stunned')})], tasks=[('Delay', {'duration': 0.3})], trans=[DONE]),
    S('Punish', conds=[('Opening', {'approachSlack': 300.0})],
      tasks=[('Pattern', {'bPunish': True, 'approachSlack': 300.0, 'approachTimeout': 1.5})], trans=[DONE]),
    S('Attack', conds=[('CanAttack', {'approachSlack': 500.0})],
      tasks=[('Pattern', {'approachSlack': 500.0, 'approachTimeout': 3.0})], trans=[DONE]),
    S('Chase', conds=[('InRange', {'maxRange': 1100.0, 'bInvert': True}), ('HasTarget', {})],
      tasks=[('Chase', {'acceptanceRadius': 700.0})], trans=[DONE]),
    S('Strafe', conds=[('HasTarget', {})], tasks=[('Strafe', {'minDuration': 0.6})], trans=[DONE]),
    S('Idle', tasks=[('IdleBehavior', {})], trans=[DONE]),
]

MINION_PATTERN = [
    S('Stunned', conds=[('HasTag', {'tag': tag('State.Stunned')})], tasks=[('Delay', {'duration': 0.3})], trans=[DONE]),
    S('Punish', conds=[('Opening', {'approachSlack': 200.0})],
      tasks=[('Pattern', {'bPunish': True, 'approachSlack': 200.0, 'approachTimeout': 1.2})], trans=[DONE]),
    S('Attack', conds=[('CanAttack', {'approachSlack': 400.0})],
      tasks=[('Pattern', {'approachSlack': 400.0, 'approachTimeout': 2.5})], trans=[DONE]),
    S('Chase', conds=[('InRange', {'maxRange': 700.0, 'bInvert': True}), ('HasTarget', {})],
      tasks=[('Chase', {'acceptanceRadius': 400.0})], trans=[DONE]),
    S('Strafe', conds=[('HasTarget', {})], tasks=[('Strafe', {'minDuration': 0.8})], trans=[DONE]),
    S('Idle', tasks=[('IdleBehavior', {})], trans=[DONE]),
]
