# -*- coding: utf-8 -*-
"""L_OD_Dungeon_01 배치 정의와 지형 계산 (에디터 Python, numpy 없음).

좌표계: 쿼터뷰 화면 기준 (u=화면 오른쪽, v=화면 위쪽, 단위 m).
필드 카메라의 고정 요가 45도이므로 화면 위쪽 = 월드 (+X, +Y) 대각선이다.
나무·언덕은 '길의 카메라 쪽(화면 아래)'에 두지 않아 플레이어를 가리지 않는다.
"""
import math

SQ = math.sqrt(0.5)

# 이름: (u, v, 반지름)
CLEARINGS = {
    'Start': (-50.0, 0.0, 15.0),
    'Glade': (10.0, 36.0, 21.0),
    'Plaza': (-50.0, 120.0, 30.0),
    'Rest': (-8.0, 150.0, 10.0),
    'Graveyard': (60.0, 192.0, 21.0),
    'Arena': (74.0, 236.0, 18.0),  # 2026-11-19: 역병의 족장(Grux) 광장, 세이브 3 직전 필수 통과
    'Forecourt': (0.0, 266.0, 18.0),
}
# 길 제어점 (Catmull-Rom). 코너를 만들기 위해 일부러 꺾는다.
ROUTES = [
    [(-50, 0), (-22, 6), (-8, 20), (10, 36)],
    [(10, 36), (6, 62), (-16, 72), (-24, 92), (-50, 120)],
    [(-50, 120), (-26, 138), (-8, 150)],
    [(-8, 150), (20, 146), (40, 160), (60, 192)],
    [(60, 192), (80, 212), (74, 236), (40, 252), (0, 266)],
    [(0, 266), (0, 298)],
]
PATH_HALF_WIDTH = 4.5
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
                        + (3 * p1[i] - p0[i] - 3 * p2[i] + p3[i]) * t3) for i in (0, 1))


def route_samples():
    """길 중심선 샘플 (약 1m 간격)"""
    out = []
    for pts in ROUTES:
        c = [pts[0]] + list(pts) + [pts[-1]]
        for i in range(1, len(c) - 2):
            seg = math.dist(c[i], c[i + 1])
            n = max(2, int(seg))
            for k in range(n):
                out.append(catmull(c[i - 1], c[i], c[i + 1], c[i + 2], k / n))
    out.append(ROUTES[-1][-1])
    return out


class Raster:
    """1m 격자: walk(걸을 수 있음), mud(진흙), d(바깥 거리), t(화면 위쪽으로 길까지 거리)"""

    def __init__(self):
        self.walk = bytearray(NU * NV)
        self.mud = bytearray(NU * NV)
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
        for i, (u, v) in enumerate(route_samples()):
            hw = PATH_HALF_WIDTH + 1.2 * math.sin(i * 0.13) + 0.7 * math.sin(i * 0.31 + 1.0)
            self._disc(self.walk, u, v, lambda a: hw)
            self._disc(self.mud, u, v, lambda a: hw * 0.74)

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
    """(높이 m, 진흙, 나무 구역, 낮은 소품 구역, 길가 돌 구역)"""
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
    tree = 1.0 if (2.5 < d < 58.0 and t >= 18.0 and dd > DOME_OUTER_RADIUS + 9.0) else 0.0
    low = 1.0 if (0.6 < d < 34.0 and dd > DOME_OUTER_RADIUS + 4.0) else 0.0
    # 잔돌: 길 가장자리에 띠로 두르지 않고, 군데군데 무리지어 길 안쪽과 숲으로 흩어지게 한다
    clump = _value(u * 0.13 + 5.1, v * 0.13 - 3.7) + 0.5 * _value(u * 0.37 - 8.2, v * 0.37 + 6.4)
    reach = max(d, 0.0) / 11.0 + (0.0 if d > 0.0 else 0.18 * min(dm, 3.0) / 3.0)
    edge = 1.0 if (clump > 0.28 + reach and d < 12.0 and dd > DOME_OUTER_RADIUS + 4.0) else 0.0
    return h, mud, tree, low, edge
