# -*- coding: utf-8 -*-
"""L_OD_Dungeon_01 배치 정의와 지형 계산 (에디터 Python, numpy 없음).

좌표계: 쿼터뷰 화면 기준 (u=화면 오른쪽, v=화면 위쪽, 단위 m).
필드 카메라의 고정 요가 45도이므로 화면 위쪽 = 월드 (+X, +Y) 대각선이다.
나무·언덕은 '길의 카메라 쪽(화면 아래)'에 두지 않아 플레이어를 가리지 않는다.
"""
import math

SQ = math.sqrt(0.5)

# 2026-11-19 재구성: 짧고 촘촘한 숲길. 광장을 줄이고 굽은 길·갈래·막다른 공터로 구성한다.
# 이름: (u, v, 반지름)
CLEARINGS = {
    'Start': (-14.0, 64.0, 9.0),       # 시작 + 쉼터(화톳불)
    'Hollow': (-58.0, 206.0, 7.0),     # 막다른 작은 공터 1 (서쪽 샛길 끝)
    'Shrine': (-12.0, 226.0, 13.0),    # 무너진 사당 광장: 일반 몬스터가 나오는 유일한 광장, 세이브 2
    'Thicket': (48.0, 204.0, 7.0),     # 막다른 작은 공터 2
    'Arena': (40.0, 254.0, 16.0),      # 역병의 족장(Grux) 광장
    'Forecourt': (0.0, 282.0, 10.0),   # 보스문 앞, 세이브 3
}
# 길 제어점 (u, v, 반폭 m). Catmull-Rom으로 잇고 반폭도 같이 보간해 넓어졌다 좁아졌다 하게 한다
ROUTES = [
    [(-14, 64, 3.0), (-30, 70, 2.0), (-44, 80, 3.4), (-50, 94, 5.0), (-40, 106, 2.0), (-28, 110, 2.2),
     (-14, 118, 3.6), (-8, 130, 5.5), (-20, 142, 2.0), (-24, 154, 2.6), (-10, 166, 4.5)],          # A 시작길
    [(-10, 166, 4.5), (6, 174, 3.0), (14, 188, 6.0), (6, 204, 2.4), (-6, 214, 3.2), (-12, 226, 4.0)],  # B 동쪽 큰길
    [(-10, 166, 4.5), (-26, 174, 1.8), (-36, 190, 2.4), (-30, 204, 1.7), (-20, 214, 2.5), (-12, 226, 4.0)],  # C 서쪽 샛길
    [(-36, 190, 2.4), (-48, 198, 1.8), (-58, 206, 2.2)],                                           # D 막다른 길 1
    [(-12, 226, 4.0), (6, 232, 2.2), (22, 222, 3.8), (30, 232, 2.0), (34, 244, 3.0), (40, 254, 3.0)],  # E 사당→족장
    [(22, 222, 3.8), (36, 212, 1.8), (48, 204, 2.2)],                                             # F 막다른 길 2
    [(40, 254, 3.0), (26, 266, 2.2), (12, 268, 3.6), (4, 276, 2.6), (0, 282, 3.0), (0, 298, 3.5)],  # G 족장→보스문
]
# 길 위 큰 소품(쓰러진 나무·그루터기·바위)이 들어가지 않는 중심 통로 반폭 (m). 최소 이 두 배 폭은 항상 비어 있다
PATH_CLEAR_HALF = 2.0
# 중간 높이 숲이 카메라 쪽에서 길과 떨어져야 하는 거리 (m)
MID_CAMERA_GAP = 5.0
DOME_CENTER = (0.0, 324.0)
DOME_OUTER_RADIUS = 27.0
DOME_FLAT_RADIUS = 36.0
FLOOR_RISE = 6.0  # 시작 지점 대비 돔 바닥 높이 (m)

U0, U1, V0, V1 = -120, 140, -50, 390
NU, NV = U1 - U0 + 1, V1 - V0 + 1

# 랜드스케이프: 8x8 컴포넌트, 63쿼드, 1m 간격 = 505x505 버텍스
LS_ORIGIN = (-12000.0, -11000.0)
LS_SIZE = 505


def to_world(u, v):
    """화면 좌표(m) -> 월드 XY(cm)"""
    return (SQ * (v - u) * 100.0, SQ * (v + u) * 100.0)


def to_uv(x, y):
    """월드 XY(cm) -> 화면 좌표(m)"""
    x, y = x / 100.0, y / 100.0
    return (SQ * (y - x), SQ * (x + y))


def smooth(a, b, x):
    if a == b:
        return 0.0 if x < a else 1.0
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def base_height(v):
    """길 바닥 높이(m). 묘지 이후 돔까지 완만한 오르막"""
    return FLOOR_RISE * smooth(150.0, 262.0, v)


def _lattice(i, j):
    """격자점 난수 (-1~1). 같은 입력은 항상 같은 값"""
    h = (i * 374761393 + j * 668265263) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((h ^ (h >> 16)) & 0xFFFF) / 32767.5 - 1.0


def _value(x, y):
    i, j = math.floor(x), math.floor(y)
    a, b = x - i, y - j
    a, b = a * a * (3 - 2 * a), b * b * (3 - 2 * b)
    n0 = _lattice(i, j) * (1 - a) + _lattice(i + 1, j) * a
    n1 = _lattice(i, j + 1) * (1 - a) + _lattice(i + 1, j + 1) * a
    return n0 * (1 - b) + n1 * b


def noise(u, v):
    """-1~1 저주파 잡음 (값 잡음 3옥타브). 사인 합은 줄무늬가 보여서 쓰지 않는다"""
    return (_value(u * 0.035, v * 0.035) + 0.5 * _value(u * 0.083 + 17.3, v * 0.083 - 9.1)
            + 0.25 * _value(u * 0.19 - 41.7, v * 0.19 + 23.9)) / 1.75


def catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return tuple(0.5 * (2 * p1[i] + (p2[i] - p0[i]) * t + (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2
                        + (3 * p1[i] - p0[i] - 3 * p2[i] + p3[i]) * t3) for i in range(len(p1)))


def route_samples_w():
    """길 중심선 샘플 (u, v, 반폭), 약 1m 간격"""
    out = []
    for pts in ROUTES:
        c = [pts[0]] + list(pts) + [pts[-1]]
        for i in range(1, len(c) - 2):
            n = max(2, int(math.dist(c[i][:2], c[i + 1][:2])))
            for k in range(n):
                out.append(catmull(c[i - 1], c[i], c[i + 1], c[i + 2], k / n))
        out.append(tuple(pts[-1]))
    return out


def route_samples():
    """길 중심선 샘플 (u, v)"""
    return [(u, v) for u, v, _ in route_samples_w()]


class Raster:
    """1m 격자: walk(걸을 수 있음), mud(진흙), d(바깥 거리), t(화면 위쪽으로 길까지 거리)"""

    def __init__(self):
        self.walk = bytearray(NU * NV)
        self.mud = bytearray(NU * NV)
        self.core = bytearray(NU * NV)  # 길 중심 통로(큰 소품 금지)
        self._stamp()
        self.d = self._distance(self.walk)
        self.dm = self._distance(self.mud)
        self.t = self._upscreen()

    def idx(self, u, v):
        return (int(round(u)) - U0) + (int(round(v)) - V0) * NU

    def _disc(self, buf, cu, cv, rf):
        R = int(rf(0.0) * 1.3) + 2
        for iv in range(int(cv) - R, int(cv) + R + 1):
            for iu in range(int(cu) - R, int(cu) + R + 1):
                if U0 <= iu <= U1 and V0 <= iv <= V1:
                    du, dv = iu - cu, iv - cv
                    if math.hypot(du, dv) <= rf(math.atan2(dv, du)):
                        buf[self.idx(iu, iv)] = 1

    def _stamp(self):
        for name, (cu, cv, r) in CLEARINGS.items():
            ph = cu * 0.37 + cv * 0.11
            self._disc(self.walk, cu, cv, lambda a: r * (1 + 0.07 * math.sin(3 * a + ph) + 0.04 * math.sin(7 * a + ph * 2)))
            self._disc(self.mud, cu, cv, lambda a: r * (0.84 + 0.05 * math.sin(4 * a + ph)))
        for name, (cu, cv, r) in CLEARINGS.items():
            self._disc(self.core, cu, cv, lambda a: r * 0.55)
        for i, (u, v, hw0) in enumerate(route_samples_w()):
            # 제어점 반폭에 약한 흔들림을 더해 가장자리가 자연스럽게 출렁이게 한다
            hw = hw0 * (1.0 + 0.16 * math.sin(i * 0.21) + 0.09 * math.sin(i * 0.53 + 1.0))
            self._disc(self.walk, u, v, lambda a: hw)
            self._disc(self.mud, u, v, lambda a: hw * 0.8)
            self._disc(self.core, u, v, lambda a: PATH_CLEAR_HALF)

    def _distance(self, mask):
        """두 번 훑는 거리 변환 (근사)"""
        INF = 1e6
        D = [0.0 if mask[i] else INF for i in range(NU * NV)]
        S2 = 1.4142
        for j in range(NV):
            for i in range(NU):
                k = i + j * NU
                b = D[k]
                if i and D[k - 1] + 1 < b: b = D[k - 1] + 1
                if j:
                    if D[k - NU] + 1 < b: b = D[k - NU] + 1
                    if i and D[k - NU - 1] + S2 < b: b = D[k - NU - 1] + S2
                    if i < NU - 1 and D[k - NU + 1] + S2 < b: b = D[k - NU + 1] + S2
                D[k] = b
        for j in range(NV - 1, -1, -1):
            for i in range(NU - 1, -1, -1):
                k = i + j * NU
                b = D[k]
                if i < NU - 1 and D[k + 1] + 1 < b: b = D[k + 1] + 1
                if j < NV - 1:
                    if D[k + NU] + 1 < b: b = D[k + NU] + 1
                    if i < NU - 1 and D[k + NU + 1] + S2 < b: b = D[k + NU + 1] + S2
                    if i and D[k + NU - 1] + S2 < b: b = D[k + NU - 1] + S2
                D[k] = b
        return D

    def _upscreen(self):
        """각 칸에서 화면 위쪽(+v)으로 가장 가까운 길까지의 거리. 카메라 가림 판정용"""
        T = [99.0] * (NU * NV)
        for i in range(NU):
            last = 99.0
            for j in range(NV - 1, -1, -1):
                k = i + j * NU
                last = 0.0 if self.walk[k] else min(99.0, last + 1.0)
                T[k] = last
        return T

    def sample(self, buf, u, v):
        """쌍선형 샘플 (범위 밖은 가장자리 값)"""
        fu = max(0.0, min(NU - 1.001, u - U0))
        fv = max(0.0, min(NV - 1.001, v - V0))
        i, j = int(fu), int(fv)
        a, b = fu - i, fv - j
        k = i + j * NU
        return (buf[k] * (1 - a) + buf[k + 1] * a) * (1 - b) + (buf[k + NU] * (1 - a) + buf[k + NU + 1] * a) * b


def dome_distance(u, v):
    return math.hypot(u - DOME_CENTER[0], v - DOME_CENTER[1])


def terrain_at(r, u, v):
    """(높이 m, 진흙, 나무 구역, 낮은 소품 구역, 길가 돌 구역, 길 위 소품 구역, 중간 높이 숲 구역)"""
    d = r.sample(r.d, u, v) if (U0 < u < U1 and V0 < v < V1) else 99.0  # 격자 밖은 길에서 먼 곳
    t = r.sample(r.t, u, v)
    dm = r.sample(r.dm, u, v)
    dd = dome_distance(u, v)
    n = noise(u, v)
    base = base_height(v)
    flat = 1.0 - smooth(DOME_FLAT_RADIUS, DOME_FLAT_RADIUS + 14.0, dd)
    base = base + (FLOOR_RISE - base) * flat
    occl = smooth(20.0, 11.0, t)  # 1이면 길 바로 아래쪽(카메라와 길 사이)
    rise = (6.5 + 3.5 * n + 2.0 * noise(v * 0.4, u * 0.4)) * smooth(0.5, 13.0, d)
    rise = rise * (1.0 - occl) + min(rise, 0.7) * occl
    rise *= 1.0 - flat
    micro = 0.10 * noise(u * 4.1, v * 3.7) * (1.0 - flat)
    h = base + rise + micro
    mud = 1.0 - smooth(0.0, 1.6 + 0.6 * n, dm)
    # 보이는 범위만 채운다 (길에서 멀면 쿼터뷰에 보이지 않으므로 인스턴스를 아낀다)
    tree = 1.0 if (2.0 < d < 45.0 and t >= 13.0 and dd > DOME_OUTER_RADIUS + 9.0) else 0.0
    low = 1.0 if (0.6 < d < 34.0 and dd > DOME_OUTER_RADIUS + 4.0) else 0.0
    # 잔돌: 길 가장자리에 띠로 두르지 않고, 군데군데 무리지어 길 안쪽과 숲으로 흩어지게 한다
    clump = _value(u * 0.13 + 5.1, v * 0.13 - 3.7) + 0.5 * _value(u * 0.37 - 8.2, v * 0.37 + 6.4)
    reach = max(d, 0.0) / 11.0 + (0.0 if d > 0.0 else 0.18 * min(dm, 3.0) / 3.0)
    edge = 1.0 if (clump > 0.28 + reach and d < 12.0 and dd > DOME_OUTER_RADIUS + 4.0) else 0.0
    # 길 위 소품: 걸을 수 있는 칸 중 중심 통로 밖(가장자리 쪽). 큰 소품을 놓아도 진행을 막지 않는다
    walk = r.sample(r.walk, u, v) if (U0 < u < U1 and V0 < v < V1) else 0.0
    core = r.sample(r.core, u, v) if (U0 < u < U1 and V0 < v < V1) else 0.0
    onpath = 1.0 if (walk > 0.99 and core < 0.01 and dd > DOME_OUTER_RADIUS + 6.0) else 0.0
    # 중간 높이 숲(3~5m 밑동·덤불): 길 바로 옆부터 채우되, 카메라 쪽(길 아래)은 길에서 MID_CAMERA_GAP 이상 떨어진 곳만.
    # 필드 카메라 피치 55도에서 높이 h인 물체는 플레이어 아래쪽 h/1.43 m 안에 있을 때만 가린다
    mid = 1.0 if (1.2 < d < 30.0 and t >= MID_CAMERA_GAP and dd > DOME_OUTER_RADIUS + 6.0) else 0.0
    return h, mud, tree, low, edge, onpath, mid
