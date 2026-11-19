# -*- coding: utf-8 -*-
"""Megascans FBX(mid)로 받은 소품용 재질. FBX 가져오기는 텍스처가 연결되지 않은 기본 재질을 만들기 때문에
스캔 텍스처를 받는 부모 재질 2종(불투명·잎)과 에셋별 재질 인스턴스를 만들어 메시에 지정한다.
에디터 Python에서 exec 후 build() 호출."""
import unreal

eal = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
DIR = '/Game/OnlyDungeon/Environment/Scan'
SOLID = DIR + '/M_OD_Scan_Solid'
FOLIAGE = DIR + '/M_OD_Scan_Foliage'
# 메시, 텍스처 접두사, 부모, 재질 인스턴스 이름
TARGETS = [
    ('/Game/Fab/Fallen_Tree/Fallen_Tree_rgzoN_Mid', '/Game/Fab/Fallen_Tree/Fallen_Tree_rgzoN_Mid_2K_', SOLID, 'MI_OD_FallenTree'),
    (['/Game/Fab/Dead_Leaves/Dead_Leaves_rbrkA_Mid_rbrkA_Var%s_LOD%d' % (v, l) for v in 'ABC' for l in (0, 1, 2)],
     '/Game/Fab/Dead_Leaves/Dead_Leaves_rbrkA_Mid_2K_', FOLIAGE, 'MI_OD_DeadLeaves'),
]
C = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
N = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
LC = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
MK = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS


def _parent(path, foliage, prefix):
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    name = path.split('/')[-1]
    m = at.create_asset(name, DIR, unreal.Material, unreal.MaterialFactoryNew())
    if foliage:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        m.set_editor_property('two_sided', True)
    e = lambda c, x, y: MEL.create_material_expression(m, c, x, y)

    def tp(name, st, y):
        t = e(unreal.MaterialExpressionTextureSampleParameter2D, -900, y)
        t.set_editor_property('parameter_name', name)
        # 텍스처 파라미터는 기본 텍스처가 없으면 컴파일에 실패해 엔진 기본 재질로 바뀐다 → 대상 에셋의 텍스처를 기본값으로 둔다
        t.set_editor_property('texture', _prepare(prefix, name))
        t.set_editor_property('sampler_type', st)
        return t
    bc, nr, ro = tp('BaseColor', C, -300), tp('Normal', N, 0), tp('Roughness', MK, 250)
    # AO 맵이 없는 에셋(낙엽)은 AO 파라미터를 만들지 않는다
    ao = tp('AO', MK, 500) if _prepare(prefix, 'AO') else None
    # 황폐한 숲에 맞게 채도를 빼고 어둡게 한다 (값은 인스턴스에서 조절)
    ds = e(unreal.MaterialExpressionDesaturation, -500, -300)
    MEL.connect_material_expressions(bc, 'RGB', ds, '')
    sat = e(unreal.MaterialExpressionScalarParameter, -700, -150)
    sat.set_editor_property('parameter_name', 'Desaturate')
    sat.set_editor_property('default_value', 0.45)
    MEL.connect_material_expressions(sat, '', ds, 'Fraction')
    tint = e(unreal.MaterialExpressionVectorParameter, -500, -450)
    tint.set_editor_property('parameter_name', 'Tint')
    tint.set_editor_property('default_value', unreal.LinearColor(0.55, 0.52, 0.48, 1.0))
    mul = e(unreal.MaterialExpressionMultiply, -250, -300)
    MEL.connect_material_expressions(ds, '', mul, 'A')
    MEL.connect_material_expressions(tint, '', mul, 'B')
    MEL.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nr, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(ro, 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    if ao:
        MEL.connect_material_property(ao, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    if foliage:
        op = tp('Opacity', MK, 750)
        MEL.connect_material_property(op, 'R', unreal.MaterialProperty.MP_OPACITY_MASK)
    MEL.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


KEYS = {'BaseColor': (('BaseColor', 'Albedo'), True), 'Normal': (('Normal',), None), 'Roughness': (('Roughness',), False),
        'AO': (('AO',), False), 'Opacity': (('Opacity',), False)}


def _prepare(prefix, param):
    """파라미터에 맞는 텍스처를 찾고 압축 설정을 샘플러 종류와 맞춘다"""
    keys, srgb = KEYS[param]
    t = _tex(prefix, *keys)
    if t is None:
        return None
    if srgb is False and t.get_editor_property('compression_settings') != unreal.TextureCompressionSettings.TC_MASKS:
        t.set_editor_property('srgb', False)
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
        eal.save_loaded_asset(t, False)
    if srgb is None and t.get_editor_property('compression_settings') != unreal.TextureCompressionSettings.TC_NORMALMAP:
        t.set_editor_property('srgb', False)
        t.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        eal.save_loaded_asset(t, False)
    return t


def _tex(prefix, *keys):
    """접두사 폴더에서 이름에 key가 들어간 텍스처를 찾는다"""
    folder = prefix.rsplit('/', 1)[0]
    for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, False):
        n = str(a.asset_name)
        if str(a.asset_class_path.asset_name) == 'Texture2D' and n.startswith(prefix.split('/')[-1]) and 'Billboard' not in n and any(n.endswith(k) for k in keys):
            return unreal.load_asset(str(a.package_name))
    return None


def build():
    if not eal.does_directory_exist(DIR):
        eal.make_directory(DIR)
    out = {}
    parents = {SOLID: _parent(SOLID, False, TARGETS[0][1]), FOLIAGE: _parent(FOLIAGE, True, TARGETS[1][1])}
    for mesh_paths, prefix, parent, mi_name in TARGETS:
        mesh_paths = [mesh_paths] if isinstance(mesh_paths, str) else mesh_paths
        mesh_paths = [p for p in mesh_paths if eal.does_asset_exist(p)]
        if not mesh_paths:
            out[mi_name] = 'mesh missing'
            continue
        mip = DIR + '/' + mi_name
        if eal.does_asset_exist(mip):
            eal.delete_asset(mip)
        mi = at.create_asset(mi_name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', parents[parent])
        found = {}
        for param in KEYS:
            t = _prepare(prefix, param)
            if t is None:
                continue
            MEL.set_material_instance_texture_parameter_value(mi, param, t)
            found[param] = t.get_name()
        if parent == FOLIAGE:
            # 낙엽은 흙과 구분되게 따뜻한 갈색을 남긴다
            MEL.set_material_instance_scalar_parameter_value(mi, 'Desaturate', 0.15)
            MEL.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(0.8, 0.68, 0.55, 1.0))
        MEL.update_material_instance(mi)
        eal.save_loaded_asset(mi, False)
        for mesh_path in mesh_paths:
            m = unreal.load_asset(mesh_path)
            for i in range(len(m.static_materials)):
                m.set_material(i, mi)
            eal.save_loaded_asset(m, False)
        out[mi_name] = found
    return out
