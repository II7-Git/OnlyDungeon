# -*- coding: utf-8 -*-
"""조명·분위기: 해 질 녘의 차가운 숲, 횃불과 천창 빛줄기가 있는 돔 내부, 황폐한 지면 재질."""
import math
import sys
import unreal

D = 'D:/UnrealProject/OnlyDungeon/Tools/DungeonMap/'
if D not in sys.path:
    sys.path.insert(0, D)
import od_layout as L

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
MEL = unreal.MaterialEditingLibrary
ENV = '/Game/OnlyDungeon/Environment'
MAT = ENV + '/M_OD_Landscape_Blighted'
CX, CY = L.to_world(*L.DOME_CENTER)
FZ = L.FLOOR_RISE * 100.0


def build_ground():
    """지면 재질: 진흙 광택을 없애고, 넓은 얼룩으로 타일 반복을 가린다"""
    if eal.does_asset_exist(MAT):
        return unreal.load_asset(MAT)
    at = unreal.AssetToolsHelpers.get_asset_tools()
    m = at.create_asset('M_OD_Landscape_Blighted', ENV, unreal.Material, unreal.MaterialFactoryNew())
    e = lambda c, x, y: MEL.create_material_expression(m, c, x, y)
    S = unreal.MaterialSamplerType
    F = '/Game/SandyForestGround03/Textures/2048/T_SandyForestGround03_'
    M = '/Game/MuddyGroundPack/Textures/T_MuddyGround_4K_'

    def co(s, y):
        c = e(unreal.MaterialExpressionLandscapeLayerCoords, -1700, y)
        c.set_editor_property('mapping_scale', s)
        return c

    def tx(p, st, uv, y):
        s = e(unreal.MaterialExpressionTextureSample, -1400, y)
        s.set_editor_property('texture', unreal.load_asset(p))
        s.set_editor_property('sampler_type', st)
        MEL.connect_material_expressions(uv, '', s, 'UVs')
        return s

    uf, um, ub = co(5.0, 0), co(4.0, 800), co(71.0, 1500)
    fb, fn, fr = tx(F + 'BaseColor', S.SAMPLERTYPE_COLOR, uf, -200), tx(F + 'Normal', S.SAMPLERTYPE_NORMAL, uf, 100), tx(F + 'Roughness', S.SAMPLERTYPE_LINEAR_GRAYSCALE, uf, 400)
    mb, mn, mr = tx(M + 'Albedo', S.SAMPLERTYPE_COLOR, um, 700), tx(M + 'Normal', S.SAMPLERTYPE_NORMAL, um, 1000), tx(M + 'Roughness', S.SAMPLERTYPE_COLOR, um, 1300)
    big = tx(F + 'BaseColor', S.SAMPLERTYPE_COLOR, ub, 1600)

    def blend(a, b, y):
        n = e(unreal.MaterialExpressionLandscapeLayerBlend, -1000, y)
        rows = []
        for name in ('Forest', 'Mud'):
            i = unreal.LayerBlendInput()
            i.set_editor_property('layer_name', name)
            i.set_editor_property('blend_type', unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
            rows.append(i)
        n.set_editor_property('layers', rows)
        MEL.connect_material_expressions(a, '', n, 'Layer Forest')
        MEL.connect_material_expressions(b, '', n, 'Layer Mud')
        return n

    # 진흙 길을 숲 바닥보다 밝게 해 쿼터뷰에서 길이 읽히게 한다
    mb2 = e(unreal.MaterialExpressionMultiply, -1200, 700)
    mb2.set_editor_property('const_b', 2.4)
    MEL.connect_material_expressions(mb, '', mb2, 'A')
    bc, nr, ro = blend(fb, mb2, 0), blend(fn, mn, 400), blend(fr, mr, 800)
    de = e(unreal.MaterialExpressionDesaturation, -750, 0)
    MEL.connect_material_expressions(bc, '', de, '')
    fc = e(unreal.MaterialExpressionConstant, -950, 200)
    fc.set_editor_property('r', 0.7)
    MEL.connect_material_expressions(fc, '', de, 'Fraction')
    # 넓은 얼룩: 같은 텍스처를 크게 깔아 밝기를 흩뜨린다
    gr = e(unreal.MaterialExpressionDesaturation, -1100, 1600)
    MEL.connect_material_expressions(big, '', gr, '')
    k = e(unreal.MaterialExpressionMultiply, -900, 1600)
    k.set_editor_property('const_b', 2.6)
    MEL.connect_material_expressions(gr, '', k, 'A')
    mx = e(unreal.MaterialExpressionMultiply, -550, 100)
    MEL.connect_material_expressions(de, '', mx, 'A')
    MEL.connect_material_expressions(k, '', mx, 'B')
    dk = e(unreal.MaterialExpressionMultiply, -350, 100)
    dk.set_editor_property('const_b', 0.62)
    MEL.connect_material_expressions(mx, '', dk, 'A')
    # 거칠기: 젖은 번들거림 방지 (0.78~1.0)
    lp = e(unreal.MaterialExpressionLinearInterpolate, -550, 800)
    lp.set_editor_property('const_a', 0.78)
    lp.set_editor_property('const_b', 1.0)
    MEL.connect_material_expressions(ro, '', lp, 'Alpha')
    MEL.connect_material_property(dk, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nr, '', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(lp, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def get(label):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def add(c, label, loc, rot=(0.0, 0.0, 0.0)):
    a = eas.spawn_actor_from_class(c, unreal.Vector(*loc), unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]))
    a.set_actor_label(label)
    a.set_folder_path('Lighting')
    return a


def build():
    for a in eas.get_all_level_actors():
        if a.get_actor_label().startswith('LT_'):
            eas.destroy_actor(a)
    lss = unreal.get_editor_subsystem(unreal.LandscapeEditorSubsystem)
    land = lss.get_landscapes()[0]
    land.set_editor_property('landscape_material', build_ground())
    # 낮게 깔린 차가운 빛. 화면 옆에서 들어와 나무 그림자가 길을 가로지른다
    sun = get('Sun')
    sun.set_actor_rotation(unreal.Rotator(pitch=-24.0, yaw=318.0, roll=0.0), False)
    lc = sun.light_component
    lc.set_editor_property('intensity', 9.0)
    lc.set_editor_property('light_color', unreal.Color(r=205, g=205, b=225, a=255))
    get('SkyLight').light_component.set_editor_property('intensity', 4.5)
    fc = get('HeightFog').component
    fc.set_editor_property('fog_density', 0.03)
    fc.set_editor_property('fog_height_falloff', 0.12)
    fc.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(0.10, 0.12, 0.15, 1.0))
    fc.set_editor_property('volumetric_fog_extinction_scale', 1.5)
    # 후처리
    pv = add(unreal.PostProcessVolume, 'LT_PostProcess', (0.0, 0.0, 500.0))
    pv.set_editor_property('unbound', True)
    s = pv.get_editor_property('settings')
    for k, v in (('auto_exposure_min_brightness', 0.35), ('auto_exposure_max_brightness', 2.2), ('auto_exposure_bias', 1.1), ('vignette_intensity', 0.55),
                 ('bloom_intensity', 0.7), ('color_saturation', unreal.Vector4(0.78, 0.78, 0.8, 1.0)), ('color_contrast', unreal.Vector4(1.12, 1.12, 1.12, 1.0)),
                 ('color_gamma', unreal.Vector4(0.96, 0.97, 1.0, 1.0))):
        s.set_editor_property('override_' + k, True)
        s.set_editor_property(k, v)
    pv.set_editor_property('settings', s)
    # 돔 안은 숲보다 어둡게 노출을 낮춰 횃불과 빛줄기를 살린다
    room = add(unreal.PostProcessVolume, 'LT_PostProcess_Dome', (CX, CY, FZ + 1900.0))
    room.set_actor_scale3d(unreal.Vector(26.0, 26.0, 21.0))
    room.set_editor_property('priority', 1.0)
    rs = room.get_editor_property('settings')
    rs.set_editor_property('override_auto_exposure_bias', True)
    rs.set_editor_property('auto_exposure_bias', -0.5)
    room.set_editor_property('settings', rs)
    # 돔 천창 빛줄기
    sp = add(unreal.SpotLight, 'LT_OculusShaft', (CX, CY, FZ + 3700.0), (-90.0, 0.0, 0.0))
    c = sp.spot_light_component
    c.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity', 9000.0)
    c.set_editor_property('attenuation_radius', 5200.0)
    c.set_editor_property('inner_cone_angle', 11.0)
    c.set_editor_property('outer_cone_angle', 19.0)
    c.set_editor_property('volumetric_scattering_intensity', 5.0)
    c.set_editor_property('light_color', unreal.Color(r=190, g=210, b=255, a=255))
    # 벽 횃불
    torch = unreal.load_asset('/Game/Dungeon/Meshes/Props/SM_Torch')
    fire = unreal.load_asset('/Game/Dungeon/Fire/Particle_Systems/PS_Fire_00')
    for i in range(16):
        if i % 2 == 0:
            continue
        deg = 225.0 + i * 22.5
        a = math.radians(deg)
        r = 2150.0
        px, py = CX + r * math.cos(a), CY + r * math.sin(a)
        t = eas.spawn_actor_from_object(torch, unreal.Vector(px, py, FZ + 400.0), unreal.Rotator(pitch=0.0, yaw=deg + 90.0, roll=0.0))
        t.set_actor_label('LT_Torch_%02d' % i)
        t.set_actor_scale3d(unreal.Vector(3.0, 3.0, 3.0))
        t.set_folder_path('Lighting')
        fx, fy = CX + (r - 95.0) * math.cos(a), CY + (r - 95.0) * math.sin(a)
        f = eas.spawn_actor_from_object(fire, unreal.Vector(fx, fy, FZ + 505.0))
        f.set_actor_label('LT_TorchFire_%02d' % i)
        f.set_actor_scale3d(unreal.Vector(1.6, 1.6, 1.6))
        f.set_folder_path('Lighting')
        l = add(unreal.PointLight, 'LT_TorchLight_%02d' % i, (fx, fy, FZ + 540.0))
        pc = l.point_light_component
        pc.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
        pc.set_editor_property('intensity', 420.0)
        pc.set_editor_property('attenuation_radius', 2300.0)
        pc.set_editor_property('light_color', unreal.Color(r=255, g=135, b=60, a=255))
        pc.set_editor_property('cast_shadows', i % 4 == 1)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    return sum(1 for a in eas.get_all_level_actors() if a.get_actor_label().startswith('LT_'))
