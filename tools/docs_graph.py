"""Direct dependency diagrams grouped by access, with transparent connection ports."""

from __future__ import annotations

from dataclasses import dataclass
import html
from math import hypot
from pathlib import Path

import docs_render


@dataclass
class Layout:
    width: int
    height: int
    positions: dict
    widths: dict
    routes: list
    ports: dict
    trunks: dict
    headings: list


def bevelled(points):
    """Keep orthogonal lanes readable with short, straight 45-degree corners."""
    points = [point for i, point in enumerate(points) if i == 0 or point != points[i - 1]]
    result = [points[0]]
    for before, point, after in zip(points, points[1:], points[2:]):
        incoming = hypot(point[0] - before[0], point[1] - before[1])
        outgoing = hypot(after[0] - point[0], after[1] - point[1])
        cross = ((point[0] - before[0]) * (after[1] - point[1])
                 - (point[1] - before[1]) * (after[0] - point[0]))
        if not cross:
            continue
        distance = min(8, incoming / 2, outgoing / 2)
        result.append((point[0] + (before[0] - point[0]) * distance / incoming,
                       point[1] + (before[1] - point[1]) * distance / incoming))
        result.append((point[0] + (after[0] - point[0]) * distance / outgoing,
                       point[1] + (after[1] - point[1]) * distance / outgoing))
    result.append(points[-1])
    return result


def path_data(points):
    return " ".join(f'{"M" if i == 0 else "L"}{x:g},{y:g}' for i, (x, y) in enumerate(points))


def arrow_shaft(points):
    """Stop the stroke at the arrow's base; the marker alone reaches the tip."""
    x, y = points[-1]
    previous_x, previous_y = points[-2]
    length = hypot(x - previous_x, y - previous_y)
    return points[:-1] + [(x + (previous_x - x) * 8 / length, y + (previous_y - y) * 8 / length)]


def relationships(model):
    direct = sorted(model.dependencies.get(model.module, []), key=lambda entry: entry[0].casefold())
    consumers = sorted((name, access) for name, dependencies in model.dependencies.items()
                       for dependency, access in dependencies if dependency == model.module)
    edges = [(dependency, model.module, access) for dependency, access in direct]
    edges += [(model.module, name, access) for name, access in consumers]
    return list(dict.fromkeys(edges))


def layout(model, edges):
    """Merge dependencies by destination/access; keep the two access trunks separate."""
    groups = {access: sorted((source for source, target, kind in edges
                              if target == model.module and kind == access), key=str.casefold)
              for access in ("public", "private")}
    rows = max(map(len, groups.values()))
    module_y = 90 + (rows - 1) * 76 + 96 if rows else 80
    positions = {model.module: (288, module_y)}
    routes, ports, trunks, headings = [], {}, {}, []
    for access, x, rail, direction in (("public", 144, 280, 1), ("private", 432, 296, -1)):
        names = groups[access]
        if names:
            headings.append((x, 30, access.title()))
        joins = []
        for i, name in enumerate(names):
            y = 90 + (rows - len(names) + i) * 76
            positions[name] = (x, y)
            px, py = x + direction * 108, y + 20
            ports[name, access] = (px, py)
            join_y = py + abs(rail - px)
            joins.append(join_y)
            routes.append((name, model.module, access, [(px, py), (rail, join_y)]))
        if joins:
            trunks[access] = [(rail, min(joins)), (rail, module_y - 29.5)]

    consumers = {access: sorted((target for source, target, kind in edges
                                 if source == model.module and kind == access), key=str.casefold)
                 for access in ("public", "private")}
    for access, x, rail, direction in (("public", 144, 280, 1), ("private", 432, 296, -1)):
        if consumers[access]:
            ports[model.module, access] = (rail, module_y + 24)
        for i, name in enumerate(consumers[access]):
            y = module_y + 96 + i * 76
            positions[name] = (x, y)
            endpoint = x + direction * 113.5
            points = [(rail, module_y + 24), (rail, y), (endpoint, y)]
            routes.append((model.module, name, access, bevelled(points)))
    widths = {name: 224 for name in positions}
    height = max(y for _, y in positions.values()) + 80
    return Layout(576, height, positions, widths, routes, ports, trunks, headings)


def graph(model):
    edges = relationships(model)
    drawing = layout(model, edges)
    width, height = drawing.width, drawing.height
    title = html.escape(model.module.replace("_", " ").title())
    output = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" role="img"',
              ' aria-labelledby="graph-title graph-description">',
              f'<title id="graph-title">{title} module dependencies</title>',
              '<desc id="graph-description">Arrows run from dependency to consumer. '
              'Direct dependencies are grouped in alphabetical Public and Private columns. '
              'Solid edges are public; dashed edges are private. Each access group shares one incoming arrow. '
              'Consumers, when present, appear directly below the selected module without repeated headings.</desc>',
              '<defs><marker id="arrow" markerUnits="userSpaceOnUse" viewBox="0 0 8 8" '
              'markerWidth="8" markerHeight="8" refX="0" refY="4" orient="auto">',
              '<path d="M0,0 L8,4 L0,8 Z" fill="#8c8c8c"/></marker>']
    for name in sorted(drawing.positions):
        output.append(f'<mask id="node-{html.escape(name, quote=True)}" maskUnits="userSpaceOnUse" '
                      f'x="0" y="0" width="{width}" height="{height}" style="mask-type:luminance">'
                      f'<rect width="{width}" height="{height}" fill="white"/>')
        for (source, _), (x, y) in sorted(drawing.ports.items()):
            if source == name:
                output.append(f'<circle cx="{x}" cy="{y}" r="8" fill="black"/>')
        output.append('</mask>')
    output.append('</defs>')
    for dependency, consumer, access, points in drawing.routes:
        dash = ' stroke-dasharray="7 5"' if access == "private" else ""
        arrow = ' marker-end="url(#arrow)"' if dependency == model.module else ""
        shaft = arrow_shaft(points) if arrow else points
        output.append(f'<path data-dependency="{html.escape(dependency, quote=True)}" '
                      f'data-consumer="{html.escape(consumer, quote=True)}" d="{path_data(shaft)}" '
                      f'fill="none" stroke="#8c8c8c" stroke-width="2.5"{arrow}{dash}/>')
    for access, points in drawing.trunks.items():
        dash = ' stroke-dasharray="7 5"' if access == "private" else ""
        output.append(f'<path data-trunk="{access}" data-consumer="{html.escape(model.module, quote=True)}" '
                      f'd="{path_data(arrow_shaft(points))}" fill="none" stroke="#8c8c8c" stroke-width="2.5" '
                      f'marker-end="url(#arrow)"{dash}/>')
    for x, y, label in drawing.headings:
        output.append(f'<text x="{x}" y="{y}" text-anchor="middle" fill="#c8c8c8" '
                      'font-family="Futura Bk BT, sans-serif" font-size="19">' + label + '</text>')
    diagram = Path("assets/diagrams") / (model.scope + "-" + model.module + "-dependencies.svg")
    for name, (x, y) in drawing.positions.items():
        module = name.split("::")[0]
        scope = "editor" if module == "editor" else "engine"
        page = model.reference_page(Path("handbook") / scope / (module + ".md"))
        target = None if name.startswith("thirdparty::") or page is None else page
        if target:
            output.append('<a href="' + html.escape(docs_render.href(diagram, target, True), quote=True)
                          + '" target="_top">')
        stroke = "#009dff" if name == model.module else "#171717"
        node_width = drawing.widths[name]
        output.append(f'<rect x="{x - node_width / 2:g}" y="{y - 28}" width="{node_width}" height="56" '
                      f'rx="15" fill="#171717" stroke="{stroke}" stroke-width="3" '
                      f'mask="url(#node-{html.escape(name, quote=True)})"/>')
        label = html.escape(name.removeprefix("thirdparty::"))
        output.append(f'<text x="{x}" y="{y + 6}" text-anchor="middle" fill="#ffffff" '
                      'font-family="Futura Bk BT, sans-serif" font-size="19">' + label + '</text>')
        if target:
            output.append('</a>')
    for (source, access), (x, y) in sorted(drawing.ports.items()):
        # Routes start at the dot centre and keep their dash pattern throughout.
        # Only the dot is filled; the surrounding gap is a hole in the node.
        output.append(f'<circle data-port="{html.escape(source, quote=True)}" data-access="{access}" '
                      f'cx="{x}" cy="{y}" r="4" fill="#8c8c8c"/>')
    output.append('</svg>')
    return "\n".join(output) + "\n", edges
