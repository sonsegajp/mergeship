"""Walkable paths for the singleplayer Happy Mask Salesman (docs/SINGLEPLAYER.md), from a scene's own collision.

    python tools/sp_navplan.py <oot.o2r> <scene collision name> <cell> x1 y1 z1 x2 y2 z2 [x3 y3 z3 ...]

Loads the scene's CollisionHeader from oot.o2r, samples its floors on a grid (cell units), keeps the points a walker
can stand on -- a floor (not a loading zone or void), no water over it, no ledge within a cell around it -- links
neighbours whose step is small and that no wall separates, and finds the shortest path through each given point in
turn (A*). The path is then shortened to the fewest straight legs that stay on walkable ground. Prints the waypoints.
"""

import heapq
import math
import struct
import sys
import zipfile

STEP = 18.0       # the most a step may rise or fall between neighbours (more over a longer run: SLOPE)
SLOPE = 0.55


def max_rise(run):
    return max(STEP, SLOPE * run)
WALL_LOW, WALL_HIGH = 20.0, 45.0


def load(o2r, name):
    z = zipfile.ZipFile(o2r)
    full = [n for n in z.namelist() if n.endswith(name)][0]
    d = z.read(full)[0x40:]
    o = 0

    def rd(fmt):
        nonlocal o
        v = struct.unpack_from("<" + fmt, d, o)
        o += struct.calcsize("<" + fmt)
        return v

    rd("6h")
    (nv,) = rd("i")
    verts = [rd("3h") for _ in range(nv)]
    (npoly,) = rd("I")
    polys = []
    for _ in range(npoly):
        typ, a, b, c, nx, ny, nz, dist = rd("4H3hh")
        polys.append((typ, a & 0x1FFF, b & 0x1FFF, c, nx / 32767.0, ny / 32767.0, nz / 32767.0))
    (nst,) = rd("I")
    surf = []
    for _ in range(nst):
        d1, d0 = rd("2I")
        surf.append((d0, d1))
    (ncam,) = rd("I")
    for _ in range(ncam):
        rd("Hhi")
    (ncampos,) = rd("i")
    for _ in range(ncampos):
        rd("3h")
    (nwater,) = rd("i")
    water = [rd("5hi") for _ in range(nwater)]
    return verts, polys, surf, water


class Scene:
    def __init__(self, o2r, name, cell):
        self.verts, self.polys, self.surf, self.water = load(o2r, name)
        self.cell = cell
        self.floors = []
        self.walls = []
        for typ, a, b, c, nx, ny, nz in self.polys:
            tri = (self.verts[a], self.verts[b], self.verts[c])
            d0 = self.surf[typ][0] if typ < len(self.surf) else 0
            exit_ = (d0 >> 8) & 0x1F
            floorprop = (d0 >> 26) & 0xF
            if ny > 0.6:
                self.floors.append((tri, exit_ == 0 and floorprop not in (0xC, 0xD, 0x9)))
            elif abs(ny) < 0.6:
                self.walls.append(tri)
        # buckets of floors by xz cell
        self.bucket = {}
        for i, (tri, ok) in enumerate(self.floors):
            xs = [p[0] for p in tri]
            zs = [p[2] for p in tri]
            for cx in range(int(math.floor(min(xs) / cell)), int(math.floor(max(xs) / cell)) + 1):
                for cz in range(int(math.floor(min(zs) / cell)), int(math.floor(max(zs) / cell)) + 1):
                    self.bucket.setdefault((cx, cz), []).append(i)
        self.wbucket = {}
        for i, tri in enumerate(self.walls):
            xs = [p[0] for p in tri]
            zs = [p[2] for p in tri]
            for cx in range(int(math.floor(min(xs) / cell)), int(math.floor(max(xs) / cell)) + 1):
                for cz in range(int(math.floor(min(zs) / cell)), int(math.floor(max(zs) / cell)) + 1):
                    self.wbucket.setdefault((cx, cz), []).append(i)
        self.cache = {}

    def floors_at(self, x, z):
        """[(y, walkable)] of every floor over (x, z)."""
        out = []
        for i in self.bucket.get((int(math.floor(x / self.cell)), int(math.floor(z / self.cell))), []):
            tri, ok = self.floors[i]
            (x1, y1, z1), (x2, y2, z2), (x3, y3, z3) = tri
            det = (z2 - z3) * (x1 - x3) + (x3 - x2) * (z1 - z3)
            if det == 0:
                continue
            l1 = ((z2 - z3) * (x - x3) + (x3 - x2) * (z - z3)) / det
            l2 = ((z3 - z1) * (x - x3) + (x1 - x3) * (z - z3)) / det
            l3 = 1 - l1 - l2
            if min(l1, l2, l3) < -1e-6:
                continue
            out.append((l1 * y1 + l2 * y2 + l3 * y3, ok))
        return out

    def under_water(self, x, y, z):
        for xmin, ysurf, zmin, xlen, zlen, _ in self.water:
            if xmin <= x <= xmin + xlen and zmin <= z <= zmin + zlen and ysurf > y + 25:
                return True
        return False

    def stand(self, x, z, near_y):
        """The walkable floor height nearest near_y at (x, z), or None."""
        best = None
        for y, ok in self.floors_at(x, z):
            if abs(y - near_y) > 60:
                continue
            if best is None or abs(y - near_y) < abs(best[0] - near_y):
                best = (y, ok)
        if best is None or not best[1] or self.under_water(x, best[0], z):
            return None
        return best[0]

    def wall_between(self, a, b):
        ax, ay, az = a
        bx, by, bz = b
        cells = set()
        n = max(1, int(math.dist((ax, az), (bx, bz)) / (self.cell * 0.5)))
        for k in range(n + 1):
            t = k / n
            cells.add((int(math.floor((ax + (bx - ax) * t) / self.cell)),
                       int(math.floor((az + (bz - az) * t) / self.cell))))
        for c in cells:
            for i in self.wbucket.get(c, []):
                tri = self.walls[i]
                for h in (WALL_LOW, WALL_HIGH):
                    if seg_tri((ax, ay + h, az), (bx, by + h, bz), tri):
                        return True
        return False


def seg_tri(p, q, tri):
    # Moeller-Trumbore on the segment p->q
    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = tri
    e1 = (x1 - x0, y1 - y0, z1 - z0)
    e2 = (x2 - x0, y2 - y0, z2 - z0)
    dvec = (q[0] - p[0], q[1] - p[1], q[2] - p[2])
    h = (dvec[1] * e2[2] - dvec[2] * e2[1], dvec[2] * e2[0] - dvec[0] * e2[2], dvec[0] * e2[1] - dvec[1] * e2[0])
    a = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2]
    if abs(a) < 1e-9:
        return False
    f = 1.0 / a
    s = (p[0] - x0, p[1] - y0, p[2] - z0)
    u = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2])
    if u < 0 or u > 1:
        return False
    qv = (s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0])
    v = f * (dvec[0] * qv[0] + dvec[1] * qv[1] + dvec[2] * qv[2])
    if v < 0 or u + v > 1:
        return False
    t = f * (e2[0] * qv[0] + e2[1] * qv[1] + e2[2] * qv[2])
    return 0 <= t <= 1


NEIGH = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1)]


RELAXED = False


def node_ok(sc, ix, iz, y):
    """Walkable, with walkable ground a cell away all round (no ledge beside him); RELAXED: walkable only."""
    c = sc.cell
    if sc.stand(ix * c, iz * c, y) is None:
        return None
    yy = sc.stand(ix * c, iz * c, y)
    if RELAXED:
        return yy
    for dx, dz in NEIGH:
        ny = sc.stand((ix + dx) * c, (iz + dz) * c, yy)
        if ny is None or abs(ny - yy) > max_rise(c * math.hypot(dx, dz)) * 1.2:
            return None
    return yy


def astar(sc, start, goal):
    c = sc.cell
    s = (round(start[0] / c), round(start[2] / c))
    g = (round(goal[0] / c), round(goal[2] / c))
    sy = sc.stand(s[0] * c, s[1] * c, start[1])
    if sy is None:
        sy = start[1]
    openq = [(0, s, sy)]
    came = {s: None}
    cost = {s: 0}
    ys = {s: sy}
    while openq:
        _, cur, cy = heapq.heappop(openq)
        if cur == g or math.dist(cur, g) <= 1.5:
            path = [cur]
            while came[path[-1]] is not None:
                path.append(came[path[-1]])
            path.reverse()
            return [(p[0] * c, ys[p], p[1] * c) for p in path]
        for dx, dz in NEIGH:
            nb = (cur[0] + dx, cur[1] + dz)
            key = nb
            ny = node_ok(sc, nb[0], nb[1], cy)
            near_goal = math.dist(nb, g) <= 2.5 or math.dist(nb, s) <= 2.5
            if ny is None and near_goal:
                ny = sc.stand(nb[0] * c, nb[1] * c, cy)
            if ny is None or abs(ny - cy) > max_rise(math.hypot(dx, dz) * c):
                continue
            if sc.wall_between((cur[0] * c, cy, cur[1] * c), (nb[0] * c, ny, nb[1] * c)):
                continue
            step = math.hypot(dx, dz) * c + abs(ny - cy) * 0.5
            nc = cost[cur] + step
            if nc < cost.get(key, 1e18):
                cost[key] = nc
                came[key] = cur
                ys[key] = ny
                heapq.heappush(openq, (nc + math.dist(nb, g) * c, key, ny))
    return None


def walkable_line(sc, a, b):
    n = max(1, int(math.dist((a[0], a[2]), (b[0], b[2])) / (sc.cell * 0.5)))
    y = a[1]
    for k in range(1, n + 1):
        t = k / n
        x = a[0] + (b[0] - a[0]) * t
        z = a[2] + (b[2] - a[2]) * t
        ny = sc.stand(x, z, y)
        if ny is None or abs(ny - y) > max_rise(math.dist((a[0], a[2]), (b[0], b[2])) / n):
            return False
        if node_ok(sc, round(x / sc.cell), round(z / sc.cell), ny) is None and 0.05 < t < 0.95:
            return False
        if sc.wall_between((x - (b[0] - a[0]) / n, y, z - (b[2] - a[2]) / n), (x, ny, z)):
            return False
        y = ny
    return True


def simplify(sc, path):
    out = [path[0]]
    i = 0
    while i < len(path) - 1:
        j = len(path) - 1
        while j > i + 1 and not walkable_line(sc, path[i], path[j]):
            j -= 1
        out.append(path[j])
        i = j
    return out


def main():
    o2r, name, cell = sys.argv[1], sys.argv[2], float(sys.argv[3])
    pts = [tuple(map(float, sys.argv[i:i + 3])) for i in range(4, len(sys.argv), 3)]
    sc = Scene(o2r, name, cell)
    full = []
    global RELAXED
    for a, b in zip(pts, pts[1:]):
        p = astar(sc, a, b)
        if p is None:
            RELAXED = True
            p = astar(sc, a, b)
            print(f"(relaxed: edges not kept clear) {a} -> {b}: {'found' if p else 'none'}")
        if p is None:
            print(f"NO PATH {a} -> {b}")
            return 1
        full += p if not full else p[1:]
    way = simplify(sc, full)
    length = sum(math.dist(u, v) for u, v in zip(way, way[1:]))
    print(f"{len(way)} waypoints, {length:.0f} units")
    print("    " + ", ".join(f"{{ {x:.0f}, {y:.0f}, {z:.0f} }}" for x, y, z in way))
    return 0


if __name__ == "__main__":
    sys.exit(main())
