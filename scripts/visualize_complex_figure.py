import sys
from PIL import Image, ImageDraw

# Get size from command line args, default to 256x256
W = int(sys.argv[1]) if len(sys.argv) > 1 else 256
H = int(sys.argv[2]) if len(sys.argv) > 2 else W

files = [
    (f'test_{W}x{H}.raw',  'Input'),
    ('out_gaussian.raw',    'Gaussian Blur'),
    ('out_magnitude.raw',   'Grad Magnitude'),
    ('out_nms.raw',         'NMS'),
    ('out_threshold.raw',   'Double Threshold'),
    ('out_final.raw',       'Final Edges'),
]

PAD    = 50
MARGIN = 20

grid = Image.new('L', (3*(W+MARGIN)+MARGIN, 2*(H+PAD+MARGIN)+MARGIN), 40)
draw = ImageDraw.Draw(grid)

for i, (fname, title) in enumerate(files):
    data = open(fname, 'rb').read()
    img  = Image.frombytes('L', (W, H), data)
    col  = i % 3
    row  = i // 3
    x    = MARGIN + col*(W+MARGIN)
    y    = MARGIN + row*(H+PAD+MARGIN) + PAD
    grid.paste(img, (x, y))
    draw.text((x + W//2 - len(title)*3, y - PAD + 8), title, fill=220)

out = f'result_{W}x{H}.png'
grid.save(out)
print(f'Saved {out}')
