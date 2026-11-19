# -*- coding: utf-8 -*-
"""게임플레이 배치: 안개벽 재질, 보이지 않는 경계벽, 세이브 포인트 3곳, 구울 42·사냥꾼 6, 광장 정예 Grux, 골렘, 보스 입구, 내비 메시."""
import math
import random
import sys
import unreal

D = 'D:/UnrealProject/OnlyDungeon/Tools/DungeonMap/'
if D not in sys.path:
    sys.path.insert(0, D)
import importlib
import od_layout as L
importlib.reload(L)

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MEL = unreal.MaterialEditingLibrary
ENV = '/Game/OnlyDungeon/Environment/'
MAP = '/Game/OnlyDungeon/Maps/L_OD_Dungeon_01'
FOG = ENV + 'M_OD_FogWall'
Idle = unreal.ODEnemyIdleBehavior


def cls(path):
    return unreal.load_class(None, path + '.' + path.split('/')[-1] + '_C')


def build_fog_material():
    if eal.does_asset_exist(FOG):
        return unreal.load_asset(FOG)
    at = unreal.AssetToolsHelpers.get_asset_tools()
    m = at.create_asset('M_OD_FogWall', ENV[:-1], unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property('two_sided', True)
    e = lambda c, x, y: MEL.create_material_expression(m, c, x, y)
    uv = e(unreal.MaterialExpressionTextureCoordinate, -1500, 0)
    tm = e(unreal.MaterialExpressionTime, -1500, 200)
    sp = e(unreal.MaterialExpressionMultiply, -1300, 200)
    sp.set_editor_property('const_b', 0.18)
    MEL.connect_material_expressions(tm, '', sp, 'A')
    sc = e(unreal.MaterialExpressionMultiply, -1300, 0)
    sc.set_editor_property('const_b', 3.0)
    MEL.connect_material_expressions(uv, '', sc, 'A')
    ap = e(unreal.MaterialExpressionAppendVector, -1100, 100)
    MEL.connect_material_expressions(sc, '', ap, 'A')
    MEL.connect_material_expressions(sp, '', ap, 'B')
    no = e(unreal.MaterialExpressionNoise, -900, 100)
    no.set_editor_property('scale', 1.0)
    no.set_editor_property('levels', 4)
    no.set_editor_property('output_min', 0.15)
    no.set_editor_property('output_max', 1.0)
    MEL.connect_material_expressions(ap, '', no, 'Position')
    # 판 가장자리를 부드럽게 지운다: 4u(1-u) * 4v(1-v)
    mask_u, mask_v = e(unreal.MaterialExpressionComponentMask, -1100, 400), e(unreal.MaterialExpressionComponentMask, -1100, 600)
    mask_u.set_editor_property('r', True); mask_u.set_editor_property('g', False)
    mask_v.set_editor_property('r', False); mask_v.set_editor_property('g', True)
    MEL.connect_material_expressions(uv, '', mask_u, '')
    MEL.connect_material_expressions(uv, '', mask_v, '')

    def bump(src, y):
        om = e(unreal.MaterialExpressionOneMinus, -900, y)
        MEL.connect_material_expressions(src, '', om, '')
        mu = e(unreal.MaterialExpressionMultiply, -700, y)
        MEL.connect_material_expressions(src, '', mu, 'A')
        MEL.connect_material_expressions(om, '', mu, 'B')
        k = e(unreal.MaterialExpressionMultiply, -500, y)
        k.set_editor_property('const_b', 4.0)
        MEL.connect_material_expressions(mu, '', k, 'A')
        return k

    edge = e(unreal.MaterialExpressionMultiply, -300, 500)
    MEL.connect_material_expressions(bump(mask_u, 400), '', edge, 'A')
    MEL.connect_material_expressions(bump(mask_v, 700), '', edge, 'B')
    op = e(unreal.MaterialExpressionMultiply, -100, 300)
    MEL.connect_material_expressions(no, '', op, 'A')
    MEL.connect_material_expressions(edge, '', op, 'B')
    col = e(unreal.MaterialExpressionConstant3Vector, -300, 0)
    col.set_editor_property('constant', unreal.LinearColor(0.2, 0.25, 0.32, 1.0))
    em = e(unreal.MaterialExpressionMultiply, 100, 100)
    MEL.connect_material_expressions(col, '', em, 'A')
    MEL.connect_material_expressions(op, '', em, 'B')
    MEL.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def setup_entrance_bp(fog):
    path = '/Game/OnlyDungeon/World/BP_ODBossEntrance'
    c = unreal.get_default_object(cls(path))
    comp = [x for x in c.get_components_by_class(unreal.StaticMeshComponent) if x.get_name() == 'FogWall'][0]
    comp.set_editor_property('static_mesh', unreal.load_asset('/Engine/BasicShapes/Plane'))
    comp.set_editor_property('override_materials', [fog])
    bp = unreal.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)


def ground(u, v, r):
    return L.terrain_at(r, u, v)[0] * 100.0


def spawn(c, label, loc, yaw=0.0, folder='Gameplay'):
    a = eas.spawn_actor_from_class(c, loc, unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


WALL_MARGIN = 1.0  # m, 걸을 수 있는 칸 바깥으로 벽을 밀어내는 거리


def build_boundary(r):
    """걸을 수 있는 영역의 외곽을 따라 보이지 않는 벽 메시 생성"""
    path = ENV + 'SM_OD_BoundaryWall'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    # 길 가장자리에서 1m 바깥에 세운다. 벽을 직선 토막으로 이으면 급커브에서 길 안쪽을 가로질러 폭이 좁아지기 때문
    inside = bytearray(1 if r.d[k] <= WALL_MARGIN else 0 for k in range(L.NU * L.NV))
    edge = set()
    for j in range(1, L.NV - 1):
        for i in range(1, L.NU - 1):
            k = i + j * L.NU
            if inside[k] and not (inside[k - 1] and inside[k + 1] and inside[k - L.NU] and inside[k + L.NU]):
                edge.add((i, j))
    chains = []
    while edge:
        cur = next(iter(edge))
        edge.discard(cur)
        chain = [cur]
        while True:
            near = [(cur[0] + a, cur[1] + b) for a in range(-3, 4) for b in range(-3, 4) if (cur[0] + a, cur[1] + b) in edge]
            if not near:
                break
            cur = min(near, key=lambda p: (p[0] - chain[-1][0]) ** 2 + (p[1] - chain[-1][1]) ** 2)
            edge.discard(cur)
            chain.append(cur)
        chains.append(chain)
    dm = unreal.new_object(unreal.DynamicMesh)
    po = unreal.GeometryScriptPrimitiveOptions()
    G = unreal.GeometryScript_Primitives
    n = 0
    for chain in chains:
        if len(chain) < 4:
            continue
        pts = chain[::2] + [chain[0]] if math.dist(chain[0], chain[-1]) < 6 else chain[::2] + [chain[-1]]
        for a, b in zip(pts, pts[1:]):
            ua, va, ub, vb = a[0] + L.U0, a[1] + L.V0, b[0] + L.U0, b[1] + L.V0
            mu, mv = (ua + ub) / 2.0, (va + vb) / 2.0
            if L.dome_distance(mu, mv) < 29.5:
                continue    # 건물 벽이 대신 막는다
            ax, ay = L.to_world(ua, va)
            bx, by = L.to_world(ub, vb)
            ln = math.hypot(bx - ax, by - ay)
            if ln < 1.0:
                continue
            yaw = math.degrees(math.atan2(by - ay, bx - ax))
            z = L.base_height(mv) * 100.0 - 300.0
            t = unreal.Transform(unreal.Vector((ax + bx) / 2.0, (ay + by) / 2.0, z), unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0), unreal.Vector(1, 1, 1))
            G.append_box(dm, po, t, ln + 60.0, 40.0, 1400.0)
            n += 1
    o = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    o.set_editor_property('enable_nanite', False)
    o.set_editor_property('enable_collision', True)
    o.set_editor_property('collision_mode', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    sm, _ = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dm, path, o)
    eal.save_loaded_asset(sm, False)
    a = eas.spawn_actor_from_object(sm, unreal.Vector(0, 0, 0))
    a.set_actor_label('GP_BoundaryWall')
    a.set_folder_path('Gameplay')
    a.static_mesh_component.set_visibility(False)
    a.set_actor_hidden_in_game(True)
    return n, len(chains)


# 세이브 포인트(쉼터): 이름, u, v, 부활 시 바라볼 화면 방향(요). 2026-11-19 새 코스: 시작 공터·사당 광장 북쪽·보스문 앞
SAVES = [('Save_Start', -18.0, 60.0, 45.0), ('Save_Shrine', -12.0, 238.0, 45.0), ('Save_BossDoor', -4.0, 286.0, 45.0)]
# 구울 묶음: (구역, u, v, 반지름, 서기, 순찰, 매복). 광장 대신 길 모퉁이·좁은 길에 둔다
GROUPS = [
    ('Bend1', -48.0, 90.0, 4.0, 2, 0, 0),       # A 첫 굽이 넓은 곳
    ('Corner1', -10.0, 128.0, 4.0, 1, 0, 1),    # A 첫 코너
    ('Narrow1', -22.0, 150.0, 2.5, 0, 2, 0),    # A 좁은 길
    ('EastRoad', 12.0, 186.0, 5.0, 2, 1, 0),    # B 동쪽 큰길
    ('WestPath', -36.0, 186.0, 2.5, 0, 0, 2),   # C 서쪽 샛길 매복
    ('Hollow', -58.0, 206.0, 4.0, 1, 0, 0),     # 막다른 공터 1
    ('Shrine', -12.0, 226.0, 8.0, 3, 1, 0),     # 무너진 사당 광장
    ('Corner2', 24.0, 224.0, 3.0, 1, 0, 1),     # E 코너
    ('GateRoad', 14.0, 268.0, 2.5, 1, 0, 0),    # G 보스문 전 길
]
# 저주받은 사냥꾼(Khaimera) 묶음: (구역, u, v, 반지름, 서기, 순찰) — 족장 광장에서 25m 넘게 떨어뜨린다(무리 어그로가 정예를 깨우지 않게)
HUNTERS = [
    ('Hollow', -58.0, 206.0, 4.0, 1, 0),
    ('Thicket', 48.0, 204.0, 4.0, 2, 0),
    ('GateRoad', 12.0, 270.0, 2.5, 1, 0),
]
# 역병의 족장(Grux): 광장 가운데, 들어오는 길(E) 쪽을 바라본다
ARENA_GRUX = (40.0, 254.0)
GRUX_FACE = (30.0, 232.0)


def place_enemies(r):
    rng = random.Random(7)
    ghoul = cls('/Game/OnlyDungeon/Characters/Enemies/BP_ODEnemy_Skeleton')
    hunter = cls('/Game/OnlyDungeon/Characters/Enemies/BP_ODEnemy_Khaimera')
    total = 0
    groups = [(ghoul, 'Ghoul', 110.0) + g for g in GROUPS] + [(hunter, 'Hunter', 112.0) + h + (0,) for h in HUNTERS]
    for c, prefix, lift, name, cu, cv, rad, stand, patrol, ambush in groups:
        kinds = [Idle.STAND] * stand + [Idle.PATROL] * patrol + [Idle.AMBUSH] * ambush
        used = []
        for n, kind in enumerate(kinds):
            for _ in range(200):
                a, d = rng.uniform(0, math.tau), rad * math.sqrt(rng.random())
                u, v = cu + d * math.cos(a), cv + d * math.sin(a)
                if r.sample(r.d, u, v) == 0.0 and all(math.dist((u, v), p) > 3.0 for p in used):
                    break
            used.append((u, v))
            x, y = L.to_world(u, v)
            e = spawn(c, 'GP_%s_%s_%02d' % (prefix, name, n), unreal.Vector(x, y, ground(u, v, r) + lift), rng.uniform(0, 360), 'Gameplay/' + prefix + 's')
            e.set_editor_property('idle_behavior', kind)
            if kind == Idle.PATROL:
                pts = []
                for k in range(3):
                    ang = rng.uniform(0, math.tau)
                    du, dv = rng.uniform(4, 7) * math.cos(ang), rng.uniform(4, 7) * math.sin(ang)
                    if r.sample(r.d, u + du, v + dv) > 0.0:
                        du, dv = (cu - u) * 0.5, (cv - v) * 0.5
                    wx, wy = L.to_world(u + du, v + dv)
                    pts.append(unreal.Vector(wx - x, wy - y, 0.0))
                e.set_editor_property('patrol_points', pts)
            total += 1
    return total


def build():
    if str(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name()).split('.')[0] != MAP:
        les.load_level(MAP)
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith('GP_') or a.get_class().get_name() in ('PlayerStart', 'NavMeshBoundsVolume'):
            eas.destroy_actor(a)
    fog = build_fog_material()
    setup_entrance_bp(fog)
    r = L.Raster()
    out = {'boundary': build_boundary(r)}
    # 시작 지점
    sx, sy = L.to_world(-10.0, 66.0)
    spawn(unreal.PlayerStart, 'GP_PlayerStart', unreal.Vector(sx, sy, ground(-10.0, 66.0, r) + 110.0), 45.0)
    # 세이브 포인트
    rp = cls('/Game/OnlyDungeon/World/BP_ODRespawnPoint')
    pit = unreal.load_asset('/Game/Dungeon/Meshes/Props/SM_Charcoal_Pit')
    fire = unreal.load_asset('/Game/Dungeon/Fire/Particle_Systems/PS_Fire_00')
    for name, u, v, yaw in SAVES:
        x, y = L.to_world(u, v)
        z = ground(u, v, r)
        a = spawn(rp, 'GP_' + name, unreal.Vector(x, y, z), yaw)
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        c.set_static_mesh(pit)
        c.set_editor_property('override_materials', [])
        c.set_editor_property('relative_scale3d', unreal.Vector(4.0, 4.0, 4.0))
        c.set_editor_property('relative_location', unreal.Vector(0.0, 0.0, 0.0))
        fx = eas.spawn_actor_from_object(fire, unreal.Vector(x, y, z + 45.0))
        fx.set_actor_label('GP_%s_Fire' % name)
        fx.set_folder_path('Gameplay')
        lt = spawn(unreal.PointLight, 'GP_%s_Light' % name, unreal.Vector(x, y, z + 170.0))
        lc = lt.point_light_component
        lc.set_editor_property('intensity', 260.0)
        lc.set_editor_property('attenuation_radius', 1500.0)
        lc.set_editor_property('light_color', unreal.Color(r=255, g=150, b=70, a=255))
    # 골렘과 입구
    cx, cy = L.to_world(*L.DOME_CENTER)
    golem = spawn(cls('/Game/OnlyDungeon/Characters/Enemies/BP_ODEnemy_Golem'), 'GP_Boss_Golem', unreal.Vector(cx, cy, L.FLOOR_RISE * 100.0 + 170.0), 225.0)
    golem.set_editor_property('start_dormant', True)
    golem.set_editor_property('aggro_range', 8000.0)
    golem.set_editor_property('leash_range', 9000.0)
    a = math.radians(225.0)
    ent = spawn(cls('/Game/OnlyDungeon/World/BP_ODBossEntrance'), 'GP_BossEntrance', unreal.Vector(cx + 2700.0 * math.cos(a), cy + 2700.0 * math.sin(a), L.FLOOR_RISE * 100.0), 45.0)
    ent.set_editor_property('bosses', [golem])
    door = unreal.load_asset('/Game/Dungeon/Meshes/Props/SM_Dungeon_door').get_material(0)
    stone = unreal.load_asset('/Game/Dungeon/Meshes/Props/SM_Roof').get_material(0)
    for c in ent.get_components_by_class(unreal.StaticMeshComponent):
        if c.get_name() in ('LeftDoor', 'RightDoor'):
            c.set_material(0, door)
        elif c.get_name() == 'Lintel':
            c.set_material(0, stone)
    out['enemies'] = place_enemies(r)
    # 광장 정예
    gu, gv = ARENA_GRUX
    gx, gy = L.to_world(gu, gv)
    fx, fy = L.to_world(*GRUX_FACE)
    grux = spawn(cls('/Game/OnlyDungeon/Characters/Enemies/BP_ODEnemy_Grux'), 'GP_Elite_Grux', unreal.Vector(gx, gy, ground(gu, gv, r) + 160.0),
                 math.degrees(math.atan2(fy - gy, fx - gx)), 'Gameplay')
    out['grux'] = grux.get_actor_label()
    # 내비 메시
    nv = spawn(unreal.NavMeshBoundsVolume, 'GP_NavBounds', unreal.Vector(13000.0, 14000.0, 300.0))
    nv.set_actor_scale3d(unreal.Vector(256.0, 256.0, 30.0))
    nv.set_actor_location(unreal.Vector(13200.0, 14200.0, 500.0), False, False)
    les.save_current_level()
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'RebuildNavigation')
    return out
