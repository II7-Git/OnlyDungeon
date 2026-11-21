# -*- coding: utf-8 -*-
"""무기 교체 모션: Kwang TravelMode_Start(칼을 등에 메는 모션)를 Manny로 리타깃하고,
넣기(0~SHEATH_END초) 뒤에 같은 구간을 거꾸로 이어 붙여 '넣었다 꺼내는' 한 개의 시퀀스로 굽는다.
상체 몽타주(UpperBody)와 무기 교체 구간 노티파이(Event.Montage.WeaponSwap), GA_SwapWeapon, 플레이어 어빌리티 세트 등록까지.
에디터 Python에서 exec 후 build() 호출."""
import unreal

eal = unreal.EditorAssetLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
DIR = '/Game/OnlyDungeon/Animation/Common/WeaponSwap'
SRC_ANIM = '/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/TravelMode_Start'
RTG = '/Game/OnlyDungeon/Animation/Retarget/RTG_OD_Kwang_To_Manny'
SHEATH = DIR + '/CM_Weapon_Sheath_Back'
SWAP = DIR + '/CM_Weapon_Swap_Back'
MONTAGE_DIR = '/Game/OnlyDungeon/Animation/Common/Montages'
MONTAGE = MONTAGE_DIR + '/AM_Weapon_Swap'
GA = '/Game/OnlyDungeon/AbilitySystem/Abilities/GA_SwapWeapon'
ABILITY_SET = '/Game/OnlyDungeon/AbilitySystem/DA_AbilitySet_Player'
SHEATH_END = 0.8   # 손이 어깨 위(등)에 머무는 마지막 시각. 이 뒤로는 빈손이 내려온다
GRAB_TIME = 0.56   # 원본에서 칼이 손을 떠나 등에 고정되는 시각 (weapon_backMount_r 측정)
PLAY_RATE = 1.3


def retarget():
    if eal.does_asset_exist(SHEATH):
        return unreal.load_asset(SHEATH)
    if not eal.does_directory_exist(DIR):
        eal.make_directory(DIR)
    rtg = unreal.load_asset(RTG)
    c = unreal.IKRetargeterController.get_controller(rtg)
    src_mesh = c.get_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE)
    tgt_mesh = c.get_preview_mesh(unreal.RetargetSourceOrTarget.TARGET)
    data = unreal.EditorAssetLibrary.find_asset_data(SRC_ANIM)
    out = unreal.IKRetargetBatchOperation.duplicate_and_retarget([data], src_mesh, tgt_mesh, rtg, '', '', '', '_OD', DIR, False, False, True)
    seq = None
    for d in out:
        if str(d.asset_class_path.asset_name) == 'AnimSequence':
            seq = unreal.load_asset(str(d.package_name))
    eal.rename_asset(seq.get_path_name().split('.')[0], SHEATH)
    seq = unreal.load_asset(SHEATH)
    eal.save_loaded_asset(seq, False)
    return seq


def bake_swap(sheath):
    """넣기 프레임 0..N 다음에 N-1..0을 이어 붙인다 (뒤쪽 절반 = 등에서 칼을 꺼내는 동작)"""
    if eal.does_asset_exist(SWAP):
        eal.delete_asset(SWAP)
    swap = eal.duplicate_asset(SHEATH, SWAP)
    fps = round(unreal.AnimationLibrary.get_num_frames(sheath) / sheath.get_play_length())
    n = int(round(SHEATH_END * fps))
    frames = list(range(0, n + 1)) + list(range(n - 1, -1, -1))
    opt = unreal.AnimPoseEvaluationOptions()
    poses = [unreal.AnimPoseExtensions.get_anim_pose_at_time(sheath, f / fps, opt) for f in range(0, n + 1)]
    bones = unreal.AnimPoseExtensions.get_bone_names(poses[0])
    ctrl = swap.controller
    ctrl.open_bracket('OD weapon swap bake')
    ctrl.set_number_of_frames(unreal.FrameNumber(len(frames) - 1))
    for b in bones:
        P, R, S = [], [], []
        for f in frames:
            t = unreal.AnimPoseExtensions.get_bone_pose(poses[f], b, unreal.AnimPoseSpaces.LOCAL)
            P.append(t.translation)
            R.append(t.rotation)
            S.append(t.scale3d)
        ctrl.set_bone_track_keys(b, P, R, S)
    ctrl.close_bracket()
    eal.save_loaded_asset(swap, False)
    return swap, fps, n


def build_montage(swap, fps, n):
    if eal.does_asset_exist(MONTAGE):
        eal.delete_asset(MONTAGE)
    f = unreal.AnimMontageFactory()
    f.set_editor_property('target_skeleton', swap.get_editor_property('skeleton'))
    f.set_editor_property('source_animation', swap)
    m = at.create_asset('AM_Weapon_Swap', MONTAGE_DIR, unreal.AnimMontage, f)
    length = swap.get_play_length()
    unreal.ODMontageEditorLibrary.build_range_montage(m, swap, 0.0, length, 'UpperBody', 1.0)
    for prop, t in (('blend_in', 0.15), ('blend_out', 0.25)):
        blend = m.get_editor_property(prop)
        blend.set_editor_property('blend_time', t)
        m.set_editor_property(prop, blend)
    # 칼이 등에 닿는 순간(GRAB_TIME) ~ 거꾸로 재생에서 다시 그 자세가 되는 순간
    grab_back = 2.0 * n / fps - GRAB_TIME
    unreal.AnimationLibrary.add_animation_notify_track(m, 'WeaponSwap')
    st = unreal.AnimationLibrary.add_animation_notify_state_event(m, 'WeaponSwap', GRAB_TIME, grab_back - GRAB_TIME, unreal.ODAnimNotifyState_GameplayEventWindow)
    tag = unreal.GameplayTag()
    tag.import_text('(TagName="Event.Montage.WeaponSwap")')
    st.set_editor_property('event_tag', tag)
    eal.save_loaded_asset(m, False)
    return m, length, GRAB_TIME, grab_back


def build_ability(m):
    parent = unreal.ODGameplayAbility_SwapWeapon.static_class()
    if not eal.does_asset_exist(GA):
        f = unreal.BlueprintFactory()
        f.set_editor_property('parent_class', parent)
        bp = at.create_asset('GA_SwapWeapon', GA.rsplit('/', 1)[0], unreal.Blueprint, f)
    bp = unreal.load_asset(GA)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(unreal.load_class(None, GA + '.GA_SwapWeapon_C'))
    cdo.set_editor_property('swap_montage', m)
    cdo.set_editor_property('montage_play_rate', PLAY_RATE)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp, False)
    # 플레이어 어빌리티 세트에 추가 (입력 태그 없음: Event.Weapon.SwapRequest로 발동)
    s = unreal.load_asset(ABILITY_SET)
    arr = list(s.get_editor_property('granted_gameplay_abilities'))
    cls = unreal.load_class(None, GA + '.GA_SwapWeapon_C')
    if not any(e.get_editor_property('ability') == cls for e in arr):
        e = unreal.ODAbilitySet_GameplayAbility()
        # Ability는 인스턴스에서 편집할 수 없는 속성이라 텍스트로 넣는다
        e.import_text('(Ability="%s",AbilityLevel=1,InputTag=(TagName=""))' % cls.get_path_name())
        arr.append(e)
        s.set_editor_property('granted_gameplay_abilities', arr)
        eal.save_loaded_asset(s, False)
    return [x.get_editor_property('ability').get_name() for x in s.get_editor_property('granted_gameplay_abilities')]


def build():
    sheath = retarget()
    swap, fps, n = bake_swap(sheath)
    m, length, a, b = build_montage(swap, fps, n)
    abilities = build_ability(m)
    return {'fps': fps, 'frames': n, 'swap_len': round(length, 3), 'notify': (round(a, 2), round(b, 2)), 'montage_time': round(length / PLAY_RATE, 2), 'set': abilities}
