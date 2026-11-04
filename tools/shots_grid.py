# Grid of a test's harness screenshots (PNG) -> <testdir>/shots_grid.png, labelled with file names.
import sys, glob, os
from PIL import Image, ImageDraw, ImageFont
d = sys.argv[1]
files = sorted(f for f in glob.glob(os.path.join(d, '*.png')) if not os.path.basename(f).startswith(('shots_grid', 'items_trio', 'farore_quad', 'beams_zoom', 'zora_zoom', 'boom_zoom', 'beam_zoom')))
if not files: sys.exit(0)
cols = 3 if len(files) > 4 else 2
rows = (len(files) + cols - 1) // cols
W, H = 640, 480
out = Image.new('RGB', (cols * W, rows * H))
font = ImageFont.truetype('C:/Windows/Fonts/consola.ttf', 22)
for k, f in enumerate(files):
    im = Image.open(f).convert('RGB').resize((W, H))
    dr = ImageDraw.Draw(im); dr.rectangle((0, 0, W, 28), fill=(0, 0, 0)); dr.text((6, 2), os.path.basename(f), fill=(255, 255, 0), font=font)
    out.paste(im, ((k % cols) * W, (k // cols) * H))
out.save(os.path.join(d, 'shots_grid.png'))
print(os.path.join(d, 'shots_grid.png'))
