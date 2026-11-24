"""Bake the map textures with Pillow, @resvg/resvg-js and DayZ ImageToPAA.

Run from this checkout with the rasterizer installed under build/map-art.
The checked-in icons come from Lucide commit c26ce5515667640ca2753e095ba319d225fc0d8d.
"""
from math import ceil, cos, radians, sin
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw

from build import ROOT, TOOLS


GUI = ROOT / "source/Predate/gui"
ICONS = ROOT / "assets/map"
WORK = ROOT / "build/map-art"
RASTERIZER = WORK / "node_modules/@resvg/resvg-js"


def to_paa(image, name):
    source = WORK / f"{name}_ca.tga"
    output = GUI / f"{name}_ca.paa"
    image.save(source)
    subprocess.run([TOOLS / "ImageToPAA/ImageToPAA.exe", source, output], check=True)


def write_imageset(name, filename, image, entries):
    width, height = image.size
    definitions = []
    for key, x, y, w, h in entries:
        definitions.append(f'''  ImageSetDefClass {key} {{
   Name "{key}"
   Pos {x} {y}
   Size {w} {h}
   Flags 0
  }}''')
    (GUI / filename).write_text(f'''ImageSetClass {{
 Name "{name}"
 RefSize {width} {height}
 Textures {{
  ImageSetTextureClass {{
   mpix 0
   path "Predate/gui/{Path(filename).stem}_ca.paa"
  }}
 }}
 Images {{
{chr(10).join(definitions)}
 }}
}}
''')


def make_controls():
    atlas = Image.new("RGBA", (256, 128))
    entries = []
    for index, (key, vector) in enumerate((("Player", "navigation-2"), ("Overview", "maximize"), ("Center", "locate-fixed"))):
        target = WORK / f"{vector}.png"
        rasterize = """
            const fs = require('node:fs');
            const [rasterizer, input, output, icon] = process.argv.slice(1);
            const { Resvg } = require(rasterizer);
            let svg = fs.readFileSync(input, 'utf8').replaceAll('currentColor', '#ffffff');
            if (icon === 'navigation-2') svg = svg.replace('fill="none"', 'fill="#ffffff"').replace('stroke="#ffffff"', 'stroke="#25282d"');
            fs.writeFileSync(output, new Resvg(svg, {fitTo:{mode:'width',value:64}}).render().asPng());
        """
        subprocess.run(["node", "-e", rasterize, RASTERIZER, ICONS / f"{vector}.svg", target, vector], check=True)
        glyph = Image.open(target).convert("RGBA")
        x = 8 + index * 80
        atlas.paste(glyph, (x, 8))
        entries.append((key, x, 8, 64, 64))

    colors = {
        "Control": ("#202b35", "#354c5d", "#15202b"),
        "Clear": ("#a72e34", "#be4047", "#87242b"),
    }
    draw = ImageDraw.Draw(atlas)
    states = (("Normal", 0), ("Highlight", 1), ("Pushed", 2), ("Focus", 1), ("Disabled", 0))
    styles = ['<WidgetStyles>', ' <Widget Name="ButtonWidget">']
    for row, (key, palette) in enumerate(colors.items()):
        for index, color in enumerate(palette):
            x, y = 8 + index * 20, 84 + row * 20
            draw.rectangle((x, y, x + 11, y + 11), fill=color)
            entries.append((f"{key}{index}", x+2, y+2, 8, 8))
        styles.append(f'  <Style Name="PredateMap{key}" Font="gui/fonts/sdf_MetronBook24" ImageSet="PredateMapIcons" Color="4294967295">')
        for state, index in states:
            styles.append(f'   <State Name="{state}"><Item Name="Center" Image="{key}{index}"/></State>')
        styles.append('  </Style>')
    styles.extend([' </Widget>', '</WidgetStyles>', ''])
    (GUI / "map_buttons.styles").write_text("\n".join(styles))
    to_paa(atlas, "map_icons")
    write_imageset("PredateMapIcons", "map_icons.imageset", atlas, entries)


def make_tablet():
    art = Image.open(ICONS / "tablet.png").convert("RGBA")
    width, height = art.size
    if (width, height) != (3344, 1882):
        raise ValueError("Tablet artwork dimensions changed. Measure both openings again.")

    # Keep the supplied RGB pixels and antialias only the alpha mask.
    # The original translucent shadow is outside the opaque tablet.
    samples = 2
    mask = Image.new("L", (width * samples, height * samples))
    pen = ImageDraw.Draw(mask)
    pen.rounded_rectangle((16*samples, 12*samples, 3328*samples-1, 1866*samples-1),
                          radius=112*samples, fill=255)
    pen.rounded_rectangle((150*samples, 138*samples, 2206*samples-1, 1748*samples-1),
                          radius=22*samples, fill=0)
    art.putalpha(mask.resize(art.size, Image.Resampling.BOX))

    atlas = Image.new("RGBA", (4096, 2048))
    atlas.paste(art, (4, 4))
    to_paa(atlas, "map_tablet")
    write_imageset("PredateMapTablet", "map_tablet.imageset", atlas, [("Tablet", 4, 4, *art.size)])


def make_arcs():
    # The dimensions and radius steps match PredateMapStrokes.DrawArc.
    atlas = Image.new("RGBA", (2048, 2048), (255, 255, 255, 0))
    entries = []
    x = y = row_height = 4
    samples = 4
    tiles = []
    for index in range(25):
        radius = round(20 * 1.2 ** index)
        # Rotation in DayZ stays within the image widget's bounds. Leave a
        # square around the complete arc at any heading.
        half = ceil(2 * radius * sin(radians(3.75))) + 4
        # Thick pulse outlines use the smaller radius-to-width ratios. Bake
        # those masks densely enough for a crisp edge at 4K.
        texels = 6 if index <= 13 else 2
        size = 2 * half * texels
        tiles.append((size, index, radius, half, texels))
    for size, index, radius, half, texels in sorted(tiles, reverse=True):
        if x + size + 4 > atlas.width:
            x, y, row_height = 4, y + row_height + 8, 0
        if y + size + 4 > atlas.height:
            raise ValueError("Pulse arc atlas is too small")
        mask = Image.new("L", (size * samples, size * samples))
        draw = ImageDraw.Draw(mask)
        pixel_scale = texels * samples
        def circle_line(edge, steps):
            return [
                ((half + edge * sin(radians(step / 16))) * pixel_scale,
                 (half + radius - edge * cos(radians(step / 16))) * pixel_scale)
                for step in steps
            ]
        # Both ends follow the sector rays, giving the stroke square ends.
        draw.polygon(circle_line(radius + 2, range(-120, 121))
                     + circle_line(radius - 2, range(120, -121, -1)), fill=255)
        sprite = Image.new("RGBA", (size, size), "white")
        sprite.putalpha(mask.resize((size, size), Image.Resampling.BOX))
        atlas.paste(sprite, (x, y))
        entries.append((f"Arc{index}", x, y, size, size))
        x += size + 8
        row_height = max(row_height, size)
    to_paa(atlas, "map_arcs")
    write_imageset("PredateMapArcs", "map_arcs.imageset", atlas, entries)


if __name__ == "__main__":
    WORK.mkdir(parents=True, exist_ok=True)
    make_controls()
    make_tablet()
    make_arcs()
