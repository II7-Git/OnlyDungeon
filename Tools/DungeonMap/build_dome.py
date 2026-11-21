# -*- coding: utf-8 -*-
"""돔 보스룸: 돔·원통 벽·바닥 메시 생성(Geometry Script)과 던전 키트 벽·기둥 배치.
내부 지름 약 44m, 바닥~돔 꼭대기 약 38m. 문은 화면 아래쪽(월드 요 225도)을 향한다."""
import math
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
G = unreal.GeometryScript_Primitives
U = unreal.GeometryScript_UVs
NA = unreal.GeometryScript_NewAssetUtils
DIR = '/Game/OnlyDungeon/Environment/BossRoom/'
K = '/Game/Dungeon/Meshes/Props/'
FLOOR_Z = L.FLOOR_RISE * 100.0
SPRING = 2260.0     # 돔이 시작되는 높이
R_IN, H_IN, OCULUS = 2400.0, 1500.0, 350.0
DOOR_YAW = 225.0    # 방 중심에서 문 방향
GAP_DEG = 14.0      # 원통 벽의 문 틈 (바깥면 폭 약 6.6m < 문 6.8m라 문짝이 가장자리를 덮는다). 기존 8.0
DOOR_TOP = 860.0    # 문 위 벽이 시작하는 높이 (문 8m + 상인방 1m가 덮음). 기존 470
BAYS = 16
CX, CY = L.to_world(*L.DOME_CENTER)


def V2(x, y):
    return unreal.Vector2D(x, y)


def mesh_asset(name, paths, material, uv=500.0, degrees=360.0, offset=0.0, steps=72, caps=True, planar=False):
    """닫힌 단면들을 Z축으로 회전시켜 스태틱 메시 생성 (복합 콜리전)"""
    path = DIR + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    dm = unreal.new_object(unreal.DynamicMesh)
    po = unreal.GeometryScriptPrimitiveOptions()
    ro = unreal.GeometryScriptRevolveOptions()
    ro.set_editor_property('revolve_degrees', degrees)
    ro.set_editor_property('degree_offset', offset)
    ro.set_editor_property('fill_partial_revolve_endcaps', caps)
    ro.set_editor_property('hard_normals', False)
    for loop in paths:
        G.append_revolve_path(dm, po, unreal.Transform(), [V2(*p) for p in loop], ro, steps, False)
    box = unreal.Transform(scale=unreal.Vector(uv, uv, uv))
    if planar:  # 바닥은 박스 투영의 이음매가 보이므로 평면 투영
        U.set_mesh_u_vs_from_planar_projection(dm, 0, box, unreal.GeometryScriptMeshSelection())
    else:
        U.set_mesh_u_vs_from_box_projection(dm, 0, box, unreal.GeometryScriptMeshSelection())
    unreal.GeometryScript_Normals.compute_split_normals(dm, unreal.GeometryScriptSplitNormalsOptions(), unreal.GeometryScriptCalculateNormalsOptions())
    o = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    o.set_editor_property('enable_nanite', False)
    o.set_editor_property('enable_collision', True)
    o.set_editor_property('collision_mode', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    sm, _ = NA.create_new_static_mesh_asset_from_mesh(dm, path, o)
    sm.set_material(0, material)
    eal.save_loaded_asset(sm, False)
    return sm


def dome_loop():
    """돔 단면 (안쪽 곡면 -> 꼭대기 -> 바깥 곡면)"""
    t0 = math.acos(OCULUS / R_IN)
    inner = [(R_IN * math.cos(t0 * i / 24), SPRING + H_IN * math.sin(t0 * i / 24)) for i in range(25)]
    outer = [((R_IN + 180) * math.cos(t0 * i / 24), SPRING + (H_IN + 180) * math.sin(t0 * i / 24)) for i in range(24, -1, -1)]
    return inner + outer + [inner[0]]


def rib_loop():
    t0 = math.acos((OCULUS + 60) / R_IN)
    a = [((R_IN + 10) * math.cos(t0 * i / 20), SPRING + (H_IN + 10) * math.sin(t0 * i / 20)) for i in range(21)]
    b = [((R_IN - 55) * math.cos(t0 * i / 20), SPRING + (H_IN - 55) * math.sin(t0 * i / 20)) for i in range(20, -1, -1)]
    return a + b + [a[0]]


def rect(r0, r1, z0, z1):
    return [(r0, z0), (r1, z0), (r1, z1), (r0, z1), (r0, z0)]


def build_meshes():
    if not eal.does_directory_exist(DIR):
        eal.make_directory(DIR)
    stone = unreal.load_asset('/Game/Dungeon/Materials/Material_Instances/MI_Stone_Wall_00') if eal.does_asset_exist('/Game/Dungeon/Materials/Material_Instances/MI_Stone_Wall_00') else unreal.load_asset(K + 'SM_Roof').get_material(0)
    tile = unreal.load_asset(K + 'SM_Ground').get_material(0)
    return dict(
        shell=mesh_asset('SM_Dome_Shell', [dome_loop()], stone, 600.0),
        drum=mesh_asset('SM_Dome_Drum', [rect(2450.0, 2700.0, -100.0, SPRING + 90.0)], stone, 600.0, 360.0 - GAP_DEG, GAP_DEG / 2.0),
        floor=mesh_asset('SM_Dome_Floor', [[(1.0, 0.0), (2720.0, 0.0), (2720.0, -120.0), (1.0, -120.0), (1.0, 0.0)]], tile, 480.0, planar=True),
        fill=mesh_asset('SM_Dome_DrumFill', [rect(2450.0, 2700.0, DOOR_TOP, SPRING + 90.0)], stone, 600.0, GAP_DEG + 0.4, -GAP_DEG / 2.0 - 0.2, 6),
        cornice=mesh_asset('SM_Dome_Cornice', [rect(2230.0, 2460.0, SPRING - 70.0, SPRING + 30.0)], stone, 300.0),
        ring=mesh_asset('SM_Dome_OculusRing', [rect(OCULUS - 30.0, OCULUS + 90.0, SPRING + H_IN - 110.0, SPRING + H_IN + 170.0)], stone, 300.0),
        rib=mesh_asset('SM_Dome_Rib', [rib_loop()], stone, 300.0, 2.4, -1.2, 3),
    )


def polar(deg, r, z=0.0):
    a = math.radians(deg)
    return unreal.Vector(CX + r * math.cos(a), CY + r * math.sin(a), FLOOR_Z + z)


def place(mesh, label, loc, yaw=0.0, scale=(1.0, 1.0, 1.0), collide=True):
    a = eas.spawn_actor_from_object(mesh, loc, unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0))
    a.set_actor_label(label)
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_folder_path('BossRoom')
    if not collide:
        a.set_actor_enable_collision(False)
    return a


def build():
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith('BR_'):
            eas.destroy_actor(a)
    m = build_meshes()
    c = unreal.Vector(CX, CY, FLOOR_Z)
    place(m['floor'], 'BR_Floor', c)
    place(m['drum'], 'BR_Drum', c, DOOR_YAW)
    place(m['fill'], 'BR_DrumFill', c, DOOR_YAW)
    place(m['shell'], 'BR_DomeShell', c)
    place(m['cornice'], 'BR_Cornice', c)
    place(m['ring'], 'BR_OculusRing', c)
    wall0, wall1 = unreal.load_asset(K + 'SM_Wall_00'), unreal.load_asset(K + 'SM_Wall_01')
    pillar, arch = unreal.load_asset(K + 'SM_Pillar'), unreal.load_asset(K + 'SM_Arch')
    step = 360.0 / BAYS
    ring = 2250.0
    wx = 2.0 * ring * math.tan(math.radians(step / 2.0)) / 641.0 * 1.03
    tier = 503.0 * 1.5
    for i in range(BAYS):
        deg = DOOR_YAW + i * step
        for t in range(3):
            if i == 0 and t == 0:
                continue    # 문이 있는 칸의 1층은 비운다
            place(wall1 if (i + t) % 2 else wall0, 'BR_Wall_%02d_%d' % (i, t), polar(deg, ring, t * tier), deg + 90.0, (wx, 1.0, 1.5))
        v = deg + step / 2.0
        place(pillar, 'BR_Pillar_%02d' % i, polar(v, 2120.0), v, (2.1, 2.1, 1.65))
        place(pillar, 'BR_PillarUp_%02d' % i, polar(v, 2170.0, tier), v, (1.5, 1.5, 3.2))
        place(m['rib'], 'BR_Rib_%02d' % i, c, v)
        if i not in (0, 8):
            place(pillar, 'BR_Buttress_%02d' % i, polar(deg, 2800.0, -40.0), deg, (3.2, 3.2, 5.0))
    # 문 양옆 기둥과 위 아치
    for s in (-1, 1):
        # 통로를 막지 않게 문 틈 가장자리보다 바깥에 둔다
        place(pillar, 'BR_DoorPillar_%d' % s, polar(DOOR_YAW + s * (GAP_DEG / 2.0 + 2.5), 2400.0), DOOR_YAW, (1.8, 1.8, 1.65))
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return dict(actors=sum(1 for a in eas.get_all_level_actors() if a.get_actor_label().startswith('BR_')), wall_scale=round(wx, 3))


def rebuild_door_opening():
    """문 틈 관련 메시(원통 벽·문 위 벽)와 문 옆 기둥만 다시 만든다. 나머지 보스방 배치는 그대로 둔다"""
    stone = unreal.load_asset('/Game/Dungeon/Materials/Material_Instances/MI_Stone_Wall_00') if eal.does_asset_exist('/Game/Dungeon/Materials/Material_Instances/MI_Stone_Wall_00') else unreal.load_asset(K + 'SM_Roof').get_material(0)
    drum = mesh_asset('SM_Dome_Drum', [rect(2450.0, 2700.0, -100.0, SPRING + 90.0)], stone, 600.0, 360.0 - GAP_DEG, GAP_DEG / 2.0)
    fill = mesh_asset('SM_Dome_DrumFill', [rect(2450.0, 2700.0, DOOR_TOP, SPRING + 90.0)], stone, 600.0, GAP_DEG + 0.4, -GAP_DEG / 2.0 - 0.2, 6)
    pillar = unreal.load_asset(K + 'SM_Pillar')
    out = []
    for a in eas.get_all_level_actors():
        lab = a.get_actor_label()
        if lab == 'BR_Drum':
            a.static_mesh_component.set_static_mesh(drum)
            out.append(lab)
        elif lab == 'BR_DrumFill':
            a.static_mesh_component.set_static_mesh(fill)
            out.append(lab)
        elif lab.startswith('BR_DoorPillar_'):
            s = -1 if lab.endswith('-1') else 1
            a.set_actor_location(polar(DOOR_YAW + s * (GAP_DEG / 2.0 + 2.5), 2400.0), False, False)
            out.append(lab)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return out
