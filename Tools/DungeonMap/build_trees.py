# -*- coding: utf-8 -*-
"""European Hornbeam에서 잎을 지운 앙상한 나무 메시 생성. 원본 Fab 에셋은 수정하지 않는다.
사용: exec 후 make('Forest_01')  (무거우므로 한 호출에 하나씩)"""
import unreal

SRC = '/Game/EuropeanHornbeam/Geometry/SimpleWind/SM_EuropeanHornbeam_'
DST = '/Game/OnlyDungeon/Environment/Trees/'
REMOVE = ('TwoSided', 'Decoration', 'Impostor')  # 잎·잔가지 카드, 원거리 대체 메시
A = unreal.GeometryScript_AssetUtils
N = unreal.GeometryScript_NewAssetUtils
M = unreal.GeometryScript_Materials
eal = unreal.EditorAssetLibrary


def make(name, source_lod=2):
    out = 'SM_BareTree_' + name
    path = DST + out
    if eal.does_asset_exist(path):
        return out + ' (이미 있음)'
    src = unreal.load_asset(SRC + name)
    slots = [str(s.material_slot_name) for s in src.static_materials]
    mats = [s.material_interface for s in src.static_materials]
    dm = unreal.new_object(unreal.DynamicMesh)
    opt = unreal.GeometryScriptCopyMeshFromAssetOptions()
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property('lod_type', unreal.GeometryScriptLODType.SOURCE_MODEL)
    lod.set_editor_property('lod_index', source_lod)
    dm, _ = A.copy_mesh_from_static_mesh_v2(src, dm, opt, lod, False)
    before = dm.get_triangle_count() if hasattr(dm, 'get_triangle_count') else -1
    for i, s in enumerate(slots):
        if s in REMOVE:
            r = M.delete_triangles_by_material_id(dm, i)
    after = dm.get_triangle_count() if hasattr(dm, 'get_triangle_count') else -1
    o = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    o.set_editor_property('enable_nanite', True)
    o.set_editor_property('enable_collision', False)
    o.set_editor_property('enable_recompute_normals', False)
    o.set_editor_property('enable_recompute_tangents', False)
    sm, res = N.create_new_static_mesh_asset_from_mesh(dm, path, o)
    # 슬롯 번호를 원본과 맞춘다 (지운 슬롯은 비어도 무방)
    sm.set_editor_property('static_materials', [unreal.StaticMaterial(material_interface=m, material_slot_name=s) for m, s in zip(mats, slots)])
    eal.save_loaded_asset(sm, False)
    return dict(name=out, slots=slots, before=before, after=after, ok=str(res))
