# -*- coding: utf-8 -*-
"""새 레벨 생성 + 숲 지면 머티리얼 + 랜드스케이프(높이·레이어). 에디터 Python에서 exec 후 run() 호출."""
import sys
import unreal

D = 'D:/UnrealProject/OnlyDungeon/Tools/DungeonMap/'
if D not in sys.path:
    sys.path.insert(0, D)
import importlib
import od_layout as L
importlib.reload(L)

eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
ENV = '/Game/OnlyDungeon/Environment'
MAP = '/Game/OnlyDungeon/Maps/L_OD_Dungeon_01'
MAT = ENV + '/M_OD_Landscape_Forest'
LAYERS = ['Forest', 'Mud', 'TreeZone', 'LowZone']


def tex(p):
    return unreal.load_asset(p)


def build_material():
    if eal.does_asset_exist(MAT):
        return unreal.load_asset(MAT)
    if not eal.does_directory_exist(ENV):
        eal.make_directory(ENV)
    m = at.create_asset('M_OD_Landscape_Forest', ENV, unreal.Material, unreal.MaterialFactoryNew())

    def expr(cls, x, y):
        return MEL.create_material_expression(m, cls, x, y)

    def coords(scale, x, y):
        c = expr(unreal.MaterialExpressionLandscapeLayerCoords, x, y)
        c.set_editor_property('mapping_scale', scale)
        return c

    def sample(t, stype, uv, x, y):
        s = expr(unreal.MaterialExpressionTextureSample, x, y)
        s.set_editor_property('texture', tex(t))
        s.set_editor_property('sampler_type', stype)
        MEL.connect_material_expressions(uv, '', s, 'UVs')
        return s

    C, N, K = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    F = '/Game/SandyForestGround03/Textures/2048/T_SandyForestGround03_'
    M = '/Game/MuddyGroundPack/Textures/T_MuddyGround_4K_'
    uf, um = coords(5.0, -1400, 0), coords(4.0, -1400, 700)
    fb, fn, fr = sample(F + 'BaseColor', C, uf, -1100, -200), sample(F + 'Normal', N, uf, -1100, 100), sample(F + 'Roughness', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE, uf, -1100, 400)
    mb, mn, mr = sample(M + 'Albedo', C, um, -1100, 700), sample(M + 'Normal', N, um, -1100, 1000), sample(M + 'Roughness', C, um, -1100, 1300)

    def blend(a, b, y, out):
        e = expr(unreal.MaterialExpressionLandscapeLayerBlend, -600, y)
        rows = []
        for n in ('Forest', 'Mud'):
            i = unreal.LayerBlendInput()
            i.set_editor_property('layer_name', n)
            i.set_editor_property('blend_type', unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
            rows.append(i)
        e.set_editor_property('layers', rows)
        MEL.connect_material_expressions(a, '', e, 'Layer Forest')
        MEL.connect_material_expressions(b, '', e, 'Layer Mud')
        return e

    bc = blend(fb, mb, 0, None)
    nr = blend(fn, mn, 400, None)
    ro = blend(fr, mr, 800, None)
    # 황폐한 느낌: 채도를 빼고 어둡게
    de = expr(unreal.MaterialExpressionDesaturation, -300, 0)
    MEL.connect_material_expressions(bc, '', de, '')
    fr_c = expr(unreal.MaterialExpressionConstant, -500, 150)
    fr_c.set_editor_property('r', 0.55)
    MEL.connect_material_expressions(fr_c, '', de, 'Fraction')
    mul = expr(unreal.MaterialExpressionMultiply, -100, 0)
    mul.set_editor_property('const_b', 0.6)
    MEL.connect_material_expressions(de, '', mul, 'A')
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nr, '', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(ro, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def run():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    lss = unreal.get_editor_subsystem(unreal.LandscapeEditorSubsystem)
    out = {}
    m = build_material()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    if eal.does_asset_exist(MAP):
        les.load_level(MAP)
    else:
        les.new_level(MAP)
    ls = lss.get_landscapes()
    if ls:
        land = ls[0]
    else:
        land = lss.create_landscape(unreal.Vector(L.LS_ORIGIN[0], L.LS_ORIGIN[1], 0.0), unreal.Rotator(0, 0, 0), unreal.Vector(100.0, 100.0, 100.0), unreal.IntPoint(8, 8), 1, 63, m)
    # create_landscape의 위치는 중앙 기준이라, 액터 원점(최소 모서리)을 배치 기준에 맞춘다
    land.set_actor_location(unreal.Vector(L.LS_ORIGIN[0], L.LS_ORIGIN[1], 0.0), False, False)
    land.set_actor_label('Landscape_Forest')
    folder = ENV + '/LayerInfos'
    names = [str(n) for n in lss.get_target_layer_names(land)]
    for n in LAYERS:
        p = folder + '/' + n + '_LayerInfo'
        info = unreal.load_asset(p) if eal.does_asset_exist(p) else lss.create_target_layer_info(n, folder)
        if n in ('TreeZone', 'LowZone'):
            info.set_blend_method(unreal.LandscapeTargetLayerBlendMethod.NONE)
        eal.save_loaded_asset(info, False)
        if n not in names:
            lss.add_target_layer(land, n, info)
        else:
            lss.set_target_layer_info(land, n, info)
    out['layers'] = [str(n) for n in lss.get_target_layer_names(land)]

    R = L.Raster()
    H, Fo, Mu, Tr, Lo = [], [], [], [], []
    for j in range(L.LS_SIZE):
        wy = L.LS_ORIGIN[1] + j * 100.0
        for i in range(L.LS_SIZE):
            u, v = L.to_uv(L.LS_ORIGIN[0] + i * 100.0, wy)
            h, mud, tree, low = L.terrain_at(R, u, v)
            H.append(h * 100.0)
            Mu.append(mud)
            Fo.append(1.0 - mud)
            Tr.append(tree)
            Lo.append(low)
    edit = str(lss.get_edit_layer_names(land)[0])
    lss.set_heightmap_data(land, edit, H)
    for n, w in (('Forest', Fo), ('Mud', Mu), ('TreeZone', Tr), ('LowZone', Lo)):
        lss.set_weightmap_data(land, edit, n, w)
    out.update(verts=len(H), h=(round(min(H)), round(max(H))), tree=sum(Tr), low=sum(Lo), walk=sum(R.walk), route=len(L.route_samples()))
    les.save_current_level()
    return out
