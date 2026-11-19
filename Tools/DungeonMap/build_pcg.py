# -*- coding: utf-8 -*-
"""숲 배치 PCG 그래프(PCG_OD_Forest) 구성과 볼륨 배치. 에디터 Python에서 exec 후 await build(call_tool)."""
import json
import unreal

T = 'PCGToolset.PCGToolset.'
DIR = '/Game/OnlyDungeon/Environment/PCG'
GRAPH = DIR + '/PCG_OD_Forest'
TR = '/Game/OnlyDungeon/Environment/Trees/SM_BareTree_'
TRUNK = '/Game/BlackCherryBrokenTrunks01/2_Low/Static_Meshes/'
STUMP = '/Game/Stump_Scanned/Meshes/Stump_%d_mesh'
FALLEN = '/Game/Fab/Fallen_Tree/Fallen_Tree_rgzoN_Mid'
BRANCH = '/Game/BrokenBranch03/2_Low/Static_Meshes/SM_BrokenBranch03_LOW'
CUTSTUMP = '/Game/CutTreeStump01/2_Low/Static_Meshes/SM_CutTreeStump01_Low'
LEAF = '/Game/Fab/Dead_Leaves/Dead_Leaves_rbrkA_Mid_rbrkA_Var%s_LOD1'
LEAVES = [(LEAF % v, w) for v, w in (('A', 2), ('B', 3), ('C', 2))]
# 낮은 그루터기(길 위에 둬도 큰 덩어리가 아닌 것)와 키 큰 부러진 줄기(1·7번)를 나눈다
LOW_STUMPS = [(STUMP % i, 1) for i in (2, 3, 4, 5, 6, 8, 9, 10)]
ROCK_MI = '/Game/OnlyDungeon/Environment/MI_OD_Rock_Dark.MI_OD_Rock_Dark'
# 길 위 큰 소품은 몸이 통과하지 않게 막는다(중심 통로는 od_layout.PATH_CLEAR_HALF로 비워 둔다)
COLLIDE = 'CollisionEnabled=QueryAndPhysics,CollisionProfileName="BlockAll"'
NOCOLLIDE = 'CollisionEnabled=NoCollision,CollisionProfileName="NoCollision"'

# 묶음: 이름, 밀도(개/m2), 구역 레이어, 프루닝 반지름, 크기 범위, 기울기, 묻힘, 그림자, 표시 거리(cm), 메시(경로, 가중치)
BANDS = [
    dict(name='Trees', pts=0.5, zone='TreeZone', extent=110.0, scale=(1.05, 1.75), tilt=5.0, sink=-40.0, shadow=True, cull=15000,
         meshes=[(TR + 'Forest_01', 3), (TR + 'Forest_03', 3), (TR + 'Forest_05', 2), (TR + 'Forest_07', 3), (TR + 'Forest_09', 3), (TR + 'Field_02', 1)]),
    dict(name='Saplings', pts=0.04, zone='TreeZone', extent=120.0, scale=(0.7, 1.3), tilt=8.0, sink=-10.0, shadow=False, cull=8000,
         meshes=[(TR + 'Sapling_03', 1)]),
    # 키 1.7m 정도의 죽은 덤불은 카메라 쪽(길 아래)에 둬도 플레이어를 가리지 않아 낮은 소품 구역 전체에 깐다
    dict(name='Brush', pts=0.1, zone='LowZone', extent=120.0, scale=(0.8, 1.4), tilt=8.0, sink=-10.0, shadow=False, cull=7000,
         meshes=[(TR + 'Sapling_03', 1)]),
    # 길 바로 옆을 막는 중간 높이 숲 (MidZone): 키 큰 밑동과 큰 죽은 덤불로 길이 숲 사이로 난 느낌을 준다
    dict(name='MidSnags', pts=0.03, zone='MidZone', extent=120.0, scale=(1.1, 1.9), tilt=8.0, sink=-12.0, shadow=True, cull=9000,
         meshes=[(STUMP % 1, 1), (STUMP % 7, 1)]),
    dict(name='MidBrush', pts=0.06, zone='MidZone', extent=140.0, scale=(1.5, 2.4), tilt=10.0, sink=-15.0, shadow=True, cull=9000,
         meshes=[(TR + 'Sapling_03', 1)]),
    dict(name='Boulders', pts=0.0035, zone='TreeZone', extent=500.0, scale=(0.55, 1.7), tilt=25.0, sink=-40.0, shadow=True, cull=12000,
         meshes=[('ROCK', 1)]),
    dict(name='Logs', pts=0.08, zone='LowZone', extent=180.0, scale=(0.75, 1.4), tilt=8.0, sink=-8.0, shadow=False, cull=6000,
         meshes=[(TRUNK + 'SM_BlackCherryBrokenTrunk01_LOW', 1), (TRUNK + 'SM_BlackCherryBrokenTrunk02_LOW', 1), (FALLEN, 3), (BRANCH, 2)]),
    # 2026-11-19: 그루터기·부러진 줄기 (숲 바닥)
    dict(name='Stumps', pts=0.035, zone='LowZone', extent=130.0, scale=(0.9, 1.6), tilt=6.0, sink=-6.0, shadow=True, cull=8000,
         meshes=LOW_STUMPS + [(CUTSTUMP, 1)]),
    # 키 큰 부러진 줄기(그루터기 1·7번)는 숲 안쪽에 크게 세워 죽은 나무 밑동처럼 보이게 한다
    dict(name='Snags', pts=0.03, zone='TreeZone', extent=140.0, scale=(1.4, 2.3), tilt=7.0, sink=-15.0, shadow=True, cull=12000,
         meshes=[(STUMP % 1, 1), (STUMP % 7, 1)]),
    # 뿌리째 쓰러진 큰 통나무 (Megascans Fallen Tree를 키워서)
    dict(name='FallenLogs', pts=0.02, zone='LowZone', extent=260.0, scale=(1.5, 2.6), tilt=5.0, sink=-12.0, shadow=True, cull=9000,
         meshes=[(FALLEN, 1)]),
    # 숲 바닥 낙엽 더미
    dict(name='ForestLeaves', pts=0.5, zone='LowZone', extent=50.0, scale=(0.8, 1.5), tilt=1.5, sink=0.0, zoff=(2.0, 5.0), shadow=False, cull=4500, meshes=LEAVES),
    dict(name='Stones', pts=0.012, zone='LowZone', extent=70.0, scale=(0.04, 0.13), flat=True, tilt=40.0, sink=-5.0, shadow=False, cull=6000,
         meshes=[('ROCK', 1)]),
    dict(name='Ruins', pts=0.0016, zone='LowZone', extent=260.0, scale=(0.9, 1.5), tilt=4.0, sink=-12.0, shadow=True, cull=9000,
         meshes=[('PILLAR', 1)]),
    # 숲 바닥의 잔가지 (평평해 보이지 않게 작은 나뭇가지를 깐다)
    dict(name='Twigs', pts=0.16, zone='LowZone', extent=30.0, scale=(0.14, 0.34), tilt=6.0, sink=-3.0, shadow=False, cull=4500,
         meshes=[(TRUNK + 'SM_BlackCherryBrokenTrunk01_LOW', 1), (TRUNK + 'SM_BlackCherryBrokenTrunk02_LOW', 1)]),
    # 잔돌은 크기를 나눠 무리지어 흩뿌린다 (EdgeZone은 od_layout의 군집 잡음으로 정해진다)
    dict(name='EdgeStones', pts=0.07, zone='EdgeZone', extent=40.0, scale=(0.035, 0.1), flat=True, tilt=50.0, sink=-5.0, shadow=False, cull=6000,
         meshes=[('ROCK', 1)]),
    dict(name='Pebbles', pts=0.55, zone='EdgeZone', extent=14.0, scale=(0.008, 0.03), flat=True, tilt=70.0, sink=-2.0, shadow=False, cull=4500,
         meshes=[('ROCK', 1)]),
    # ---- 길 위 소품 (진행을 막지 않게) ----
    # 낙엽·잔가지는 길(진흙 레이어) 전체에 깔고 콜리전이 없다
    dict(name='PathLeaves', pts=0.6, zone='Mud', extent=45.0, scale=(0.7, 1.3), tilt=1.5, sink=0.0, zoff=(2.0, 5.0), shadow=False, cull=4500, meshes=LEAVES),
    dict(name='PathTwigs', pts=0.05, zone='Mud', extent=40.0, scale=(0.12, 0.3), tilt=5.0, sink=-2.0, shadow=False, cull=4000,
         meshes=[(TRUNK + 'SM_BlackCherryBrokenTrunk01_LOW', 1), (TRUNK + 'SM_BlackCherryBrokenTrunk02_LOW', 1), (BRANCH, 1)]),
    # 그루터기·바위는 중심 통로 밖(PathZone)에만, 몸이 통과하지 않게 콜리전을 켠다
    dict(name='PathStumps', pts=0.016, zone='PathZone', extent=160.0, scale=(0.7, 1.05), tilt=5.0, sink=-6.0, shadow=True, cull=8000, collide=True,
         meshes=LOW_STUMPS),
    dict(name='PathRocks', pts=0.012, zone='PathZone', extent=150.0, scale=(0.12, 0.28), tilt=20.0, sink=-12.0, shadow=True, cull=8000, collide=True,
         meshes=[('ROCK', 1)]),
    # 쓰러진 가지·통나무는 길을 가로지를 수 있어 콜리전 없이 낮게 둔다(밟고 지나가는 느낌)
    dict(name='PathLogs', pts=0.012, zone='PathZone', extent=170.0, scale=(0.6, 1.0), tilt=4.0, sink=-6.0, shadow=True, cull=6000,
         meshes=[(FALLEN, 2), (BRANCH, 1)]),
]


def dark_rock():
    """원본 Fab 재질을 건드리지 않고 어두운 바위 재질 인스턴스를 만든다"""
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(ROCK_MI.split('.')[0]):
        mi = unreal.load_asset(ROCK_MI.split('.')[0])
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset('MI_OD_Rock_Dark', '/Game/OnlyDungeon/Environment', unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', unreal.load_asset('/Game/ApexNature/Dolomites/Rocks/Large/Materials/MI_APXN_DOLR_Rock_Large_02'))
    M = unreal.MaterialEditingLibrary
    M.set_material_instance_vector_parameter_value(mi, '|   Color Tint', unreal.LinearColor(0.17, 0.16, 0.15, 1.0))
    M.set_material_instance_scalar_parameter_value(mi, '|    Fresnel Mask Opacity', 0.0)
    M.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)


def find_mesh(root):
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in ar.get_assets_by_path(root, True):
        if str(a.asset_class_path.asset_name) == 'StaticMesh':
            return str(a.package_name)
    raise RuntimeError(root)


def set_meshes(node_ref, meshes, shadow, cull, collide=False):
    node = unreal.find_object(None, node_ref['refPath'])
    s = node.get_settings()
    s.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
    p = s.get_editor_property('mesh_selector_parameters')
    rows = []
    for path, w in meshes:
        name = path.split('/')[-1]
        e = unreal.PCGMeshSelectorWeightedEntry()
        # 죽은 나무는 흔들리지 않으므로 WPO를 꺼서 가상 섀도맵 캐시를 살린다. 길 밖 소품이라 콜리전도 끈다
        over = 'OverrideMaterials=("%s"),' % ROCK_MI if 'APXN' in name else ''
        text = ('(Descriptor=(StaticMesh="%s.%s",' % (path, name)) + over + (
            'InstanceEndCullDistance=%d,bCastShadow=%s,bCastDynamicShadow=%s,bEvaluateWorldPositionOffset=False,'
            'bAffectDistanceFieldLighting=False,bVisibleInRayTracing=False,BodyInstance=(%s)),Weight=%d)'
            % (cull, shadow, shadow, COLLIDE if collide else NOCOLLIDE, w))
        e.import_text(text)
        rows.append(e)
    p.set_editor_property('mesh_entries', rows)
    return len(p.get_editor_property('mesh_entries'))


async def build(call_tool):
    eal = unreal.EditorAssetLibrary
    dark_rock()
    rock, pillar = find_mesh('/Game/ApexNature'), find_mesh('/Game/OldBrokenPillar')
    if eal.does_asset_exist(GRAPH):
        eal.delete_asset(GRAPH)
    g = await call_tool(T + 'CreateGraph', name='PCG_OD_Forest', path=DIR)
    add = lambda t, n, p, x, y: call_tool(T + 'AddNode', graph=g, nativeNodeType=t, nodeName=n, jsonParams=json.dumps(p), xPositionIdx=x, yPositionIdx=y)
    link = lambda a, ap, b, bp: call_tool(T + 'ConnectNodePins', fromNode=a, fromPinLabel=ap, toNode=b, toPinLabel=bp)
    land = await add('Get Landscape Data', 'Landscape', {'bUnbounded': False}, 0, 600)
    out = {}
    for i, b in enumerate(BANDS):
        n, y = b['name'], i * 260
        sp = await add('Surface Sampler', n + '_Sample', {'pointsPerSquaredMeter': b['pts'], 'pointExtents': {'x': b['extent'], 'y': b['extent'], 'z': 100}, 'looseness': 1.0}, 300, y)
        fl = await add('Filter Attribute Elements', n + '_Zone', {'operator': 'Greater', 'targetAttribute': b['zone'], 'bUseConstantThreshold': True}, 600, y)
        await call_tool(T + 'UpdateNode', node=fl, jsonParams=json.dumps({'attributeTypes': {'type': 'Double', 'doubleValue': 0.5}}))
        pr = await add('Self Pruning', n + '_Prune', {}, 900, y)
        lo, hi = b['scale']
        zlo, zhi = b.get('zoff', (b['sink'], 0.0))
        tf = await add('Transform Points', n + '_Vary', {'offsetMin': {'x': 0, 'y': 0, 'z': zlo}, 'offsetMax': {'x': 0, 'y': 0, 'z': zhi},
                                                         'rotationMin': {'pitch': -b['tilt'], 'yaw': 0, 'roll': -b['tilt']}, 'rotationMax': {'pitch': b['tilt'], 'yaw': 360, 'roll': b['tilt']},
                                                         'bAbsoluteRotation': True, 'bUniformScale': not b.get('flat'), 'scaleMin': {'x': lo, 'y': lo, 'z': lo * (0.5 if b.get('flat') else 1.0)},
                                                         'scaleMax': {'x': hi, 'y': hi * (0.7 if b.get('flat') else 1.0), 'z': hi * (0.6 if b.get('flat') else 1.0)}, 'bAbsoluteScale': True}, 1200, y)
        sm = await add('Static Mesh Spawner', n + '_Spawn', {'bAllowDescriptorChanges': True}, 1500, y)
        await link(land, 'Out', sp, 'Surface')
        await link(sp, 'Out', fl, 'In')
        await link(fl, 'InsideFilter', pr, 'In')
        await link(pr, 'Out', tf, 'In')
        await link(tf, 'Out', sm, 'In')
        meshes = [({'ROCK': rock, 'PILLAR': pillar}.get(p, p), w) for p, w in b['meshes']]
        out[n] = set_meshes(sm, meshes, b['shadow'], b['cull'], b.get('collide', False))
    pg = unreal.load_asset(GRAPH)
    eal.save_loaded_asset(pg, False)
    return g, out
