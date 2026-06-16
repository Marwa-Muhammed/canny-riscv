
from PIL import Image, ImageDraw

files = [

    ('test.raw',          'Input'),

    ('out_gaussian.raw',  'Gaussian Blur'),

    ('out_magnitude.raw', 'Gradient Magnitude'),

    ('out_nms.raw',       'NMS'),

    ('out_threshold.raw', 'Double Threshold'),

    ('out_final.raw',     'Final Edges'),

]

W, H = 256, 256

PAD = 30

MARGIN = 10

grid = Image.new('L', (3*(W+MARGIN)+MARGIN, 2*(H+PAD+MARGIN)+MARGIN), 40)

draw = ImageDraw.Draw(grid)

for i, (fname, title) in enumerate(files):

    data = open(fname, 'rb').read()

    img = Image.frombytes('L', (W, H), data)

    col = i % 3

    row = i // 3

    x = MARGIN + col*(W+MARGIN)

    y = MARGIN + row*(H+PAD+MARGIN) + PAD

    grid.paste(img, (x, y))

    draw.text((x + W//2 - len(title)*3, y - PAD + 8), title, fill=220)

grid.save('result.png')

print('Saved result.png')

