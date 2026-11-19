# -*- coding: utf-8 -*-
# 캐릭터 발 고정(미끄러짐) 점검 — CLAUDE.md 11장. 에디터 Python(MCP execute_python)에서 PIE 중에 사용.
# 사용 예:
#   ns = dict(globals()); exec(open('D:/UnrealProject/OnlyDungeon/Tools/AnimCheck/od_foot_check.py', encoding='utf-8').read(), ns)
#   result = await ns['check'](character, 2.0)   # 캐릭터가 움직이는 동안 호출
# 기준(2026-11-19 실측): 플레이어 조깅 slip 약 0.4, 보폭 88cm. slip이 0.8 이상이거나 보폭이 거의 0이면 발이 몸에 붙어 끌려가는 것(발 고정).
# 발 고정(미끄러짐) 점검: 이동 중 각 발의 월드 속도 하위 25%(디딤 구간) 평균 ÷ 몸 속도 = 미끄러짐 비율.
# 제대로 디디면 0에 가깝고, 발이 몸에 붙어 같이 끌려가면(발 고정) 1에 가깝다. 보폭은 골반 기준 발 앞뒤 폭.
import unreal, asyncio


async def check(e, secs=3.0, dt=1.0 / 30.0, min_body=80.0):
    prev = None
    sp = [[], []]
    bodies = []
    stride = []
    w = e.get_world()
    t_prev = None
    for _ in range(int(secs / dt)):
        await asyncio.sleep(dt)
        t = unreal.GameplayStatics.get_time_seconds(w)
        f = [e.mesh.get_socket_location(b) for b in ('foot_l', 'foot_r')]
        v = e.get_velocity()
        body = v.length()
        if prev is not None and body >= min_body and t > t_prev:
            d = t - t_prev
            for k in range(2):
                sp[k].append((f[k] - prev[k]).length() / d)
            bodies.append(body)
            stride.append((f[0] - e.mesh.get_socket_location('pelvis')).dot(v * (1.0 / body)))
        prev, t_prev = f, t
    if not bodies:
        return dict(moving=0)
    body = sum(bodies) / len(bodies)
    res = dict(frames=len(bodies), speed=round(body), stride=round(max(stride) - min(stride)))
    for k, n in ((0, 'slip_l'), (1, 'slip_r')):
        s = sorted(sp[k])[:max(1, len(sp[k]) // 4)]
        res[n] = round(sum(s) / len(s) / body, 2)
    return res
