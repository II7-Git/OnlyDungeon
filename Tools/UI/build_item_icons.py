# -*- coding: utf-8 -*-
"""HUD 장비 칸 아이콘: 무기·물약 메시를 맵 아래 빈 곳에 하나씩 놓고, 그 물체만 보이는 씬 캡처(SceneColorHDR)로
256x256 이미지를 찍어 텍스처로 저장한다. SceneColorHDR의 알파는 '1 - 불투명도'라 배경이 투명해진다.
UI 재질 M_OD_UIIcon이 알파를 뒤집어 배경을 지우고 밝기를 맞춘다. 아이템마다 재질 인스턴스를 만든다.
에디터 Python에서 exec 후 build() 호출. 메시·각도를 바꾸면 다시 돌린다."""
import math
import unreal

eal = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
at = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
R = unreal.RenderingLibrary
DIR = '/Game/OnlyDungeon/UI/Icons'
MAT = DIR + '/M_OD_UIIcon'
SIZE = 256
ORIGIN = unreal.Vector(-60000.0, -60000.0, 20000.0)   # 맵 밖 공중 (해·하늘빛을 받고, 지형·나무에 가리지 않는다)
# 이름, 메시, 메시 회전(pitch, yaw, roll), 화면을 채우는 비율, 밝기
# 카메라가 +Y를 보므로 화면 안에서 돌리려면 Y축 회전을 쓴다. 칼은 (45, 180, 180) = Y축 135도: 칼끝이 오른쪽 위
# 밝기는 촬영 결과의 물체 평균 밝기가 TARGET_LUMA가 되도록 자동으로 맞춘다 (SceneColorHDR은 노출 보정 전 값이라 어둡다)
ITEMS = [
    ('OneHandSword', '/Game/Atris_swords/Meshes/SM_Sword1f', (45.0, 180.0, 180.0), 0.92, 1.0),
    ('TwoHandSword', '/Game/Atris_swords/Meshes/SM_Sword1k', (45.0, 180.0, 180.0), 0.92, 1.0),
    ('Potion', '/Game/MagicPotion/Meshes/SM_Bottle5', (0.0, 20.0, 0.0), 0.80, 1.0),
]
# 카메라: 물체의 -Y 쪽에서 +Y를 본다 (칼날의 넓은 면이 보이는 방향)
CAM_DIST = 600.0
TARGET_LUMA = 0.32
# 촬영용 임시 조명: (카메라 기준 오른쪽, 앞(+Y), 위) 위치(cm), 세기(cd), 색. 정면 주광·보조광·뒤쪽 윤곽광
LIGHTS = [((-150.0, -250.0, 200.0), 60.0, (1.0, 0.93, 0.82)), ((200.0, -200.0, -60.0), 18.0, (0.75, 0.82, 1.0)), ((0.0, 250.0, 150.0), 45.0, (1.0, 0.95, 0.9))]


def build_material():
    if eal.does_asset_exist(MAT):
        return unreal.load_asset(MAT)
    if not eal.does_directory_exist(DIR):
        eal.make_directory(DIR)
    m = at.create_asset('M_OD_UIIcon', DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    e = lambda c, x, y: MEL.create_material_expression(m, c, x, y)
    tex = e(unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property('parameter_name', 'Icon')
    tex.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    bright = e(unreal.MaterialExpressionScalarParameter, -700, 250)
    bright.set_editor_property('parameter_name', 'Brightness')
    bright.set_editor_property('default_value', 1.0)
    tint = e(unreal.MaterialExpressionVectorParameter, -700, 350)
    tint.set_editor_property('parameter_name', 'Tint')   # 물약을 다 쓰면 HUD가 회색으로 바꾼다
    tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
    mul = e(unreal.MaterialExpressionMultiply, -400, 0)
    MEL.connect_material_expressions(tex, 'RGB', mul, 'A')
    MEL.connect_material_expressions(bright, '', mul, 'B')
    mul2 = e(unreal.MaterialExpressionMultiply, -200, 0)
    MEL.connect_material_expressions(mul, '', mul2, 'A')
    MEL.connect_material_expressions(tint, 'RGB', mul2, 'B')
    # SceneColorHDR 알파 = 1 - 불투명도 → 뒤집어서 배경을 지운다
    inv = e(unreal.MaterialExpressionOneMinus, -400, 200)
    MEL.connect_material_expressions(tex, 'A', inv, '')
    op = e(unreal.MaterialExpressionMultiply, -200, 200)
    MEL.connect_material_expressions(inv, '', op, 'A')
    MEL.connect_material_expressions(tint, 'A', op, 'B')
    MEL.connect_material_property(mul2, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(m)
    eal.save_loaded_asset(m, False)
    return m


def capture_one(name, mesh_path, rot, fill, brightness, mat):
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    mesh = unreal.load_asset(mesh_path)
    a = eas.spawn_actor_from_object(mesh, ORIGIN, unreal.Rotator(pitch=rot[0], yaw=rot[1], roll=rot[2]))
    a.set_actor_label('TMP_IconMesh')
    # 회전한 메시의 중심을 원점에 맞추고 화면 크기를 정한다
    org, ext = a.get_actor_bounds(False)
    a.set_actor_location(ORIGIN + (ORIGIN - org), False, False)
    org, ext = a.get_actor_bounds(False)
    width = 2.0 * max(ext.x, ext.z) / fill
    rt_path = DIR + '/RT_Icon_' + name
    if eal.does_asset_exist(rt_path):
        eal.delete_asset(rt_path)
    f = unreal.TextureRenderTargetFactoryNew()
    rt = at.create_asset('RT_Icon_' + name, DIR, unreal.TextureRenderTarget2D, f)
    rt.set_editor_property('size_x', SIZE)
    rt.set_editor_property('size_y', SIZE)
    rt.set_editor_property('render_target_format', unreal.TextureRenderTargetFormat.RTF_RGBA16F)
    rt.set_editor_property('clear_color', unreal.LinearColor(0, 0, 0, 1))
    cap = eas.spawn_actor_from_class(unreal.SceneCapture2D, unreal.Vector(org.x, org.y - CAM_DIST, org.z), unreal.Rotator(pitch=0.0, yaw=90.0, roll=0.0))
    cap.set_actor_label('TMP_IconCapture')
    c = cap.capture_component2d
    c.set_editor_property('projection_type', unreal.CameraProjectionMode.ORTHOGRAPHIC)
    c.set_editor_property('ortho_width', width)
    c.set_editor_property('texture_target', rt)
    c.set_editor_property('capture_source', unreal.SceneCaptureSource.SCS_SCENE_COLOR_HDR)
    c.set_editor_property('primitive_render_mode', unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
    c.set_editor_property('capture_every_frame', False)
    c.set_editor_property('capture_on_movement', False)
    c.show_only_actor_components(a)
    lights = []
    for (dx, dy, dz), cd, col in LIGHTS:
        lt = eas.spawn_actor_from_class(unreal.PointLight, unreal.Vector(org.x + dx, org.y + dy, org.z + dz))
        lc = lt.point_light_component
        lc.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
        lc.set_editor_property('intensity', cd)
        lc.set_editor_property('attenuation_radius', 1000.0)
        lc.set_editor_property('cast_shadows', False)
        lc.set_editor_property('light_color', unreal.Color(r=int(col[0] * 255), g=int(col[1] * 255), b=int(col[2] * 255), a=255))
        lights.append(lt)
    c.capture_scene()
    for lt in lights:
        eas.destroy_actor(lt)
    # 물체 픽셀(알파 < 0.5)의 평균 밝기로 아이콘 밝기를 정한다
    px = R.read_render_target_raw(world, rt, False) or []
    lum = [0.2126 * p.r + 0.7152 * p.g + 0.0722 * p.b for p in px if p.a < 0.5]
    mean = sum(lum) / len(lum) if lum else 0.0
    brightness = brightness * (TARGET_LUMA / mean if mean > 1e-6 else 1.0)
    tex_name = 'T_Icon_' + name
    if eal.does_asset_exist(DIR + '/' + tex_name):
        eal.delete_asset(DIR + '/' + tex_name)
    tex = R.render_target_create_static_texture2d_editor_only(rt, tex_name, unreal.TextureCompressionSettings.TC_HDR, unreal.TextureMipGenSettings.TMGS_FROM_TEXTURE_GROUP)
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
    eal.save_loaded_asset(tex, False)
    eas.destroy_actor(cap)
    eas.destroy_actor(a)
    eal.delete_asset(rt_path)
    # 재질 인스턴스
    mi_path = DIR + '/MI_Icon_' + name
    if eal.does_asset_exist(mi_path):
        mi = unreal.load_asset(mi_path)
    else:
        mi = at.create_asset('MI_Icon_' + name, DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', mat)
    MEL.set_material_instance_texture_parameter_value(mi, 'Icon', tex)
    MEL.set_material_instance_scalar_parameter_value(mi, 'Brightness', brightness)
    MEL.update_material_instance(mi)
    eal.save_loaded_asset(mi, False)
    return mi.get_path_name(), round(width), round(mean, 5), round(brightness, 2), len(lum)


def build(only=None):
    mat = build_material()
    out = {}
    for name, mesh, rot, fill, bright in ITEMS:
        if only and name not in only:
            continue
        out[name] = capture_one(name, mesh, rot, fill, bright, mat)
    out['material'] = fix_material_default(mat)
    return out


def fix_material_default(mat):
    """텍스처 파라미터의 기본 텍스처를 캡처한 HDR 아이콘으로 둔다.
    엔진 기본 텍스처(sRGB 색)를 두면 Linear Color 샘플러와 맞지 않아 재질 컴파일이 실패하고 기본 재질로 바뀐다"""
    tex = unreal.load_asset(DIR + '/T_Icon_' + ITEMS[0][0])
    for x in MEL.get_material_expressions(mat):
        if isinstance(x, unreal.MaterialExpressionTextureSampleParameter2D):
            x.set_editor_property('texture', tex)
            x.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    MEL.recompile_material(mat)
    eal.save_loaded_asset(mat, False)
    return tex.get_name()


# 아이콘을 쓰는 곳: (무기 데이터 경로, 아이콘 이름)
WEAPONS = [('/Game/OnlyDungeon/Equipment/Weapons/DA_Weapon_OneHandSword', 'OneHandSword'),
           ('/Game/OnlyDungeon/Equipment/Weapons/DA_Weapon_TwoHandSword', 'TwoHandSword')]
PLAYER_BP = '/Game/OnlyDungeon/Characters/BP_ODPlayer'


def assign():
    """무기 데이터의 HudIcon, 플레이어 물약 컴포넌트의 HudIcon을 지정한다"""
    out = {}
    for path, name in WEAPONS:
        d = unreal.load_asset(path)
        d.set_editor_property('hud_icon', unreal.load_asset(DIR + '/MI_Icon_' + name))
        eal.save_loaded_asset(d, False)
        out[name] = str(d.get_editor_property('hud_icon'))
    bp = unreal.load_asset(PLAYER_BP)
    cdo = unreal.get_default_object(unreal.load_class(None, PLAYER_BP + '.BP_ODPlayer_C'))
    potion = cdo.get_component_by_class(unreal.ODPotionComponent)
    potion.set_editor_property('hud_icon', unreal.load_asset(DIR + '/MI_Icon_Potion'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)
    cdo = unreal.get_default_object(unreal.load_class(None, PLAYER_BP + '.BP_ODPlayer_C'))
    out['Potion'] = str(cdo.get_component_by_class(unreal.ODPotionComponent).get_editor_property('hud_icon'))
    return out
