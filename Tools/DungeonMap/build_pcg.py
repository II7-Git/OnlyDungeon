# -*- coding: utf-8 -*-
"""숲 배치 PCG 그래프(PCG_OD_Forest) 구성과 볼륨 배치. 에디터 Python에서 exec 후 await build(call_tool)."""
import json
import unreal

T = 'PCGToolset.PCGToolset.'
DIR = '/Game/OnlyDungeon/Environment/PCG'
GRAPH = DIR + '/PCG_OD_Forest'
TR = '/Game/OnlyDungeon/Environment/Trees/SM_BareTree_'
TRUNK = '/Game/BlackCherryBrokenTrunks01/2_Low/Static_Meshes/'

# 묶음: 이름, 밀도(개/m2), 구역 레이어, 프루닝 반지름, 크기 범위, 기울기, 묻힘, 그림자, 표시 거리(cm), 메시(경로, 가중치)
BANDS = [
    dict(name='Trees', pts=0.2, zone='TreeZone', extent=150.0, scale=(1.05, 1.75), tilt=5.0, sink=-40.0, shadow=True, cull=15000,
         meshes=[(TR + 'Forest_01', 3), (TR + 'Forest_03', 3), (TR + 'Forest_05', 2), (TR + 'Forest_07', 3), (TR + 'Forest_09', 3), (TR + 'Field_02', 1)]),
    dict(name='Saplings', pts=0.012, zone='TreeZone', extent=120.0, scale=(0.7, 1.3), tilt=8.0, sink=-10.0, shadow=False, cull=8000,
         meshes=[(TR + 'Sapling_03', 1)]),
    dict(name='Boulders', pts=0.0035, zone='TreeZone', extent=500.0, scale=(0.55, 1.7), tilt=25.0, sink=-40.0, shadow=True, cull=12000,
         meshes=[('ROCK', 1)]),
    dict(name='Logs', pts=0.02, zone='LowZone', extent=180.0, scale=(1.0, 2.2), tilt=8.0, sink=-8.0, shadow=False, cull=6000,
         meshes=[(TRUNK + 'SM_BlackCherryBrokenTrunk01_LOW', 1), (TRUNK + 'SM_BlackCherryBrokenTrunk02_LOW', 1)]),
    dict(name='Stones', pts=0.012, zone='LowZone', extent=70.0, scale=(0.04, 0.2), tilt=40.0, sink=-5.0, shadow=False, cull=6000,
         meshes=[('ROCK', 1)]),
    dict(name='Ruins', pts=0.0016, zone='LowZone', extent=260.0, scale=(0.9, 1.5), tilt=4.0, sink=-12.0, shadow=True, cull=9000,
         meshes=[('PILLAR', 1)]),
    # 길 가장자리를 돌로 둘러 쿼터뷰에서 길의 경계가 보이게 한다
    dict(name='EdgeStones', pts=0.3, zone='EdgeZone', extent=45.0, scale=(0.05, 0.17), tilt=40.0, sink=-6.0, shadow=False, cull=6000,
         meshes=[('ROCK', 1)]),
]


def find_mesh(root):
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    for a in ar.get_assets_by_path(root, True):
        if str(a.asset_class_path.asset_name) == 'StaticMesh':
            return str(a.package_name)
    raise RuntimeError(root)


def set_meshes(node_ref, meshes, shadow, cull):
    node = unreal.find_object(None, node_ref['refPath'])
    s = node.get_settings()
    s.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
    p = s.get_editor_property('mesh_selector_parameters')
    rows = []
    for path, w in meshes:
        name = path.split('/')[-1]
        e = unreal.PCGMeshSelectorWeightedEntry()
        # 죽은 나무는 흔들리지 않으므로 WPO를 꺼서 가상 섀도맵 캐시를 살린다. 길 밖 소품이라 콜리전도 끈다
        e.import_text('(Descriptor=(StaticMesh="%s.%s",InstanceEndCullDistance=%d,bCastShadow=%s,bCastDynamicShadow=%s,bEvaluateWorldPositionOffset=False,'
                      'bAffectDistanceFieldLighting=False,bVisibleInRayTracing=False,BodyInstance=(CollisionEnabled=NoCollision,CollisionProfileName="NoCollision")),Weight=%d)'
                      % (path, name, cull, shadow, shadow, w))
        rows.append(e)
    p.set_editor_property('mesh_entries', rows)
    return len(p.get_editor_property('mesh_entries'))


async def build(call_tool):
    eal = unreal.EditorAssetLibrary
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
        tf = await add('Transform Points', n + '_Vary', {'offsetMin': {'x': 0, 'y': 0, 'z': b['sink']}, 'offsetMax': {'x': 0, 'y': 0, 'z': 0},
                                                         'rotationMin': {'pitch': -b['tilt'], 'yaw': 0, 'roll': -b['tilt']}, 'rotationMax': {'pitch': b['tilt'], 'yaw': 360, 'roll': b['tilt']},
                                                         'bAbsoluteRotation': True, 'bUniformScale': True, 'scaleMin': {'x': lo, 'y': lo, 'z': lo}, 'scaleMax': {'x': hi, 'y': hi, 'z': hi}, 'bAbsoluteScale': True}, 1200, y)
        sm = await add('Static Mesh Spawner', n + '_Spawn', {'bAllowDescriptorChanges': True}, 1500, y)
        await link(land, 'Out', sp, 'Surface')
        await link(sp, 'Out', fl, 'In')
        await link(fl, 'InsideFilter', pr, 'In')
        await link(pr, 'Out', tf, 'In')
        await link(tf, 'Out', sm, 'In')
        meshes = [({'ROCK': rock, 'PILLAR': pillar}.get(p, p), w) for p, w in b['meshes']]
        out[n] = set_meshes(sm, meshes, b['shadow'], b['cull'])
    pg = unreal.load_asset(GRAPH)
    eal.save_loaded_asset(pg, False)
    return g, out
