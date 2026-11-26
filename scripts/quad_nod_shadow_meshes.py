"""Author the low-poly ShadowVolume meshes for the three FOG quad NOD models."""

from collections import Counter
from math import cos, hypot, pi, sin


# Each pair gives the center of a tube's front and rear ends in model space.
# Mirror the pair for the other side. Only the silhouette needed for shadows
# is represented. In particular, the glass, knobs, and lens covers are omitted.
TUBES = {
    "FOG_GPNVG": (
        ((.030, .026, -.104), (.033, .069, -.025), .017, .021),
        ((.086, .033, -.093), (.062, .072, -.022), .017, .019),
    ),
    "FOG_GPNVG_GSGM": (
        ((.027, .030, -.064), (.030, .083, -.008), .017, .021),
        ((.083, .039, -.055), (.064, .087, -.004), .017, .019),
    ),
    "GPNVG_Elite": (
        ((.033, -.035, -.113), (.032, .048, -.059), .017, .022),
        ((.082, -.031, -.109), (.064, .047, -.055), .018, .020),
    ),
}

MOUNTS = {
    "FOG_GPNVG": ((0, -.029, .013), (0, .002, -.011), .024, .011),
    "FOG_GPNVG_GSGM": ((0, -.028, .022), (0, .023, .011), .023, .015),
    "GPNVG_Elite": ((0, -.029, .013), (0, .002, -.011), .024, .011),
}

BRIDGES = {
    "FOG_GPNVG": ((0, .005, -.018), (0, .060, -.056), .014, .014),
    "FOG_GPNVG_GSGM": ((0, .024, .013), (0, .071, -.016), .016, .015),
    "GPNVG_Elite": ((0, -.008, -.021), (0, -.027, -.065), .013, .014),
}


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def unit(v):
    length = hypot(*v)
    return tuple(n / length for n in v)


def prism(start, end, start_radius, end_radius, width_scale=1, sides=8):
    axis = unit(tuple(b - a for a, b in zip(start, end)))
    across = unit(cross((1, 0, 0), axis))
    upright = cross(axis, across)
    vertices = []
    for center, radius in ((start, start_radius), (end, end_radius)):
        for step in range(sides):
            angle = 2 * pi * step / sides + pi / sides
            vertices.append(tuple(c + radius * (width_scale * i * cos(angle) + j * sin(angle))
                                  for c, i, j in zip(center, upright, tuple(-n for n in across))))

    faces = []
    for step in range(sides):
        nxt = (step + 1) % sides
        faces.extend(((step, nxt, step + sides), (nxt, nxt + sides, step + sides)))
    for step in range(1, sides - 1):
        faces.extend(((0, step + 1, step), (sides, sides + step, sides + step + 1)))
    return vertices, faces


def shadow_parts(model):
    yield "head", prism(*MOUNTS[model], width_scale=1.1, sides=6)
    yield "rotate", prism(*BRIDGES[model], sides=6)
    for start, end, start_radius, end_radius in TUBES[model]:
        for sign in (-1, 1):
            yield "rotate", prism(
                (sign * start[0], *start[1:]), (sign * end[0], *end[1:]),
                start_radius, end_radius,
            )


def validate_part(vertices, faces):
    edges = Counter((a, b) for face in faces for a, b in zip(face, (*face[1:], face[0])))
    if not all(count == 1 and edges[b, a] == 1 for (a, b), count in edges.items()):
        raise ValueError("Shadow volume has a boundary edge or inconsistent winding.")
    volume = sum(sum(n * v for n, v in zip(vertices[a], cross(vertices[b], vertices[c])))
                 for a, b, c in faces) / 6
    if volume <= 0:
        raise ValueError("Shadow volume must have positive volume and outward winding.")
