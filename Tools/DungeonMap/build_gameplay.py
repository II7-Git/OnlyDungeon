# -*- coding: utf-8 -*-
"""게임플레이 배치: 안개벽 재질, 보이지 않는 경계벽, 세이브 포인트 3곳, 구울 45마리, 골렘, 보스 입구, 내비 메시."""
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


def build_boundary(r):
    """걸을 수 있는 영역의 외곽을 따라 보이지 않는 벽 메시 생성"""
    path = ENV + 'SM_OD_BoundaryWall'
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    edge = set()
    for j in range(1, L.NV - 1):
        for i in range(1, L.NU - 1):
            k = i + j * L.NU
            if r.walk[k] and not (r.walk[k - 1] and r.walk[k + 1] and r.walk[k - L.NU] and r.walk[k + L.NU]):
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
        pts = chain[::3] + [chain[0]] if math.dist(chain[0], chain[-1]) < 6 else chain[::3] + [chain[-1]]
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


# 세이브 포인트: 이름, u, v, 부활 시 바라볼 화면 방향(요)
SAVES = [('Save_Start', -56.0, 7.0, 45.0), ('Save_AfterPlaza', -8.0, 153.0, 45.0), ('Save_BossDoor', -10.0, 274.0, 45.0)]
# 구울 묶음: (구역, u, v, 반지름, 서기, 순찰, 매복)
GROUPS = [
    ('Road1', -14.0, 13.0, 4.0, 2, 1, 0),
    ('Glade', 10.0, 36.0, 14.0, 4, 2, 1),
    ('Road2', -6.0, 68.0, 4.0, 1, 1, 1),
    ('Plaza', -50.0, 120.0, 21.0, 10, 4, 2),
    ('Road3', 30.0, 151.0, 4.0, 1, 1, 0),
    ('Graveyard', 60.0, 192.0, 14.0, 4, 2, 3),
    ('Road4', 76.0, 232.0, 4.0, 1, 1, 1),
    ('Forecourt', 4.0, 262.0, 11.0, 2, 1, 0),
]


def place_enemies(r):
    rng = random.Random(7)
    ghoul = cls('/Game/OnlyDungeon/Characters/Enemies/BP_ODEnemy_Skeleton')
    total = 0
    for name, cu, cv, rad, stand, patrol, ambush in GROUPS:
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
            e = spawn(ghoul, 'GP_Ghoul_%s_%02d' % (name, n), unreal.Vector(x, y, ground(u, v, r) + 110.0), rng.uniform(0, 360), 'Gameplay/Ghouls')
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
    sx, sy = L.to_world(-52.0, -5.0)
    spawn(unreal.PlayerStart, 'GP_PlayerStart', unreal.Vector(sx, sy, ground(-52.0, -5.0, r) + 110.0), 45.0)
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
    out['ghouls'] = place_enemies(r)
    # 내비 메시
    nv = spawn(unreal.NavMeshBoundsVolume, 'GP_NavBounds', unreal.Vector(13000.0, 14000.0, 300.0))
    nv.set_actor_scale3d(unreal.Vector(256.0, 256.0, 30.0))
    nv.set_actor_location(unreal.Vector(13200.0, 14200.0, 500.0), False, False)
    les.save_current_level()
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'RebuildNavigation')
    return out
