# -*- coding: utf-8 -*-
"""
OnlyDungeon StateTree 구성 도구 (에디터 Python 전용, MCP execute_python에서 실행)

상태·조건·태스크·인스턴스 데이터·전이를 스펙(st_trees.py)대로 만든다. 실행할 때마다 Root 아래를 지우고 처음부터 다시 만든다.
필요한 것: OnlyDungeonEditor 모듈의 UODStateTreeEditorLibrary(전이·선택 방식), StateTreeToolset(상태·노드 추가).
call_tool은 execute_python 실행 환경의 전역 함수라서 이 파일은 import하지 않고 exec로 불러온다.

사용 예 (execute_python 스크립트):
    import unreal, json
    D = 'D:/UnrealProject/OnlyDungeon/Tools/StateTree/'
    exec(open(D + 'st_builder.py', encoding='utf-8').read(), globals())
    exec(open(D + 'st_trees.py', encoding='utf-8').read(), globals())
    r = await build_tree('/Game/OnlyDungeon/AI/ST_OD_Minion', 'TrySelectChildrenInOrder', MINION)
    json.dumps(r, ensure_ascii=False)

인스턴스 데이터 키는 camelCase다. (예: maxRange, pressCount, inputTag: {'tagName': 'InputTag.Attack.Light'})
새 StateTree 에셋은 StateTreeFactory에 state_tree_schema_class = StateTreeAIComponentSchema를 먼저 지정해서 만들고,
스키마의 ai_controller_class / context_actor_class를 ODAIController / ODEnemyCharacter로 맞춘다.
"""
import unreal

T = 'state_tree_toolset.state_tree.StateTreeToolset.'
OD = '/Script/OnlyDungeon.'
COND = {
    'HasTarget': OD + 'ODStateTreeHasTargetCondition',
    'InRange': OD + 'ODStateTreeTargetInRangeCondition',
    'HasTag': OD + 'ODStateTreeHasTagCondition',
    'CanAttack': OD + 'ODStateTreeCanAttackCondition',
    'Opening': OD + 'ODStateTreeTargetOpeningCondition',
}
TASK = {
    'Chase': OD + 'ODStateTreeChaseTargetTask',
    'Home': OD + 'ODStateTreeReturnHomeTask',
    'Ability': OD + 'ODStateTreeActivateAbilityTask',
    'Pattern': OD + 'ODStateTreeExecutePatternTask',
    'Strafe': OD + 'ODStateTreeStrafeTask',
    'IdleBehavior': OD + 'ODStateTreeIdleBehaviorTask',
    'Delay': '/Script/StateTreeModule.StateTreeDelayTask',
}


def S(name, select=None, conds=(), tasks=(), trans=(), children=()):
    return dict(name=name, select=select, conds=list(conds), tasks=list(tasks), trans=list(trans), children=list(children))


def tag(name):
    return {'tagName': name}


async def _set_params(ref, node_id, params):
    if not params:
        return
    data = await call_tool(T + 'get_node_instance_data', state_tree=ref, node_id=node_id)
    for k, v in params.items():
        if k not in data:
            raise RuntimeError('unknown param %s in %s' % (k, data.get('_structType')))
        data[k] = v
    ok = await call_tool(T + 'set_node_instance_data', state_tree=ref, node_id=node_id, instance_data=data)
    if not ok:
        raise RuntimeError('set_node_instance_data failed %s' % params)


async def _build_state(ref, parent_ref, spec, by_name, pending):
    st = await call_tool(T + 'add_child_state', state_tree=ref, parent=parent_ref, name=spec['name'])
    obj = unreal.find_object(None, st['refPath'])
    by_name[spec['name']] = obj
    if spec['select']:
        if not unreal.ODStateTreeEditorLibrary.set_selection_behavior(obj, spec['select']):
            raise RuntimeError('selection %s' % spec['select'])
    for kind, params in spec['conds']:
        nid = await call_tool(T + 'add_enter_condition', state_tree=ref, state=st, condition_struct={'refPath': COND[kind]})
        await _set_params(ref, nid, params)
    for kind, params in spec['tasks']:
        nid = await call_tool(T + 'add_task', state_tree=ref, state=st, task_struct={'refPath': TASK[kind]})
        await _set_params(ref, nid, params)
    for tr in spec['trans']:
        pending.append((obj, tr))
    for child in spec['children']:
        await _build_state(ref, st, child, by_name, pending)


async def build_tree(asset_path, root_select, children):
    name = asset_path.split('/')[-1]
    ref = {'refPath': asset_path + '.' + name}
    root = (await call_tool(T + 'get_root_states', state_tree=ref))[0]
    root_obj = unreal.find_object(None, root['refPath'])

    # 처음부터 다시 만든다.
    for child in await call_tool(T + 'get_children', state=root):
        await call_tool(T + 'remove_child_state', state_tree=ref, parent=root, child=child)
    unreal.ODStateTreeEditorLibrary.clear_transitions(root_obj)
    unreal.ODStateTreeEditorLibrary.set_selection_behavior(root_obj, root_select)

    by_name = {'Root': root_obj}
    pending = []
    for spec in children:
        await _build_state(ref, root, spec, by_name, pending)

    # 전이는 모든 상태를 만든 뒤 이름으로 연결한다.
    for obj, (trigger, ttype, target) in pending:
        target_obj = by_name.get(target) if target else None
        if not unreal.ODStateTreeEditorLibrary.add_transition(obj, trigger, ttype, target_obj):
            raise RuntimeError('transition %s %s %s' % (trigger, ttype, target))

    await call_tool(T + 'mark_as_modified', state_tree=ref)
    result = await call_tool(T + 'compile', state_tree=ref)
    st_asset = unreal.load_asset(asset_path)
    saved = unreal.EditorAssetLibrary.save_loaded_asset(st_asset, False)
    msgs = [(m.get('severity'), m.get('message')) for m in result.get('messages', []) if m.get('severity') != 'Info']
    return dict(success=result.get('bSuccess'), saved=saved, messages=msgs,
                tree=unreal.ODStateTreeEditorLibrary.describe_state_tree(st_asset))
