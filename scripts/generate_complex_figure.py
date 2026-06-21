import sys
from PIL import Image, ImageDraw

W = int(sys.argv[1]) if len(sys.argv) > 1 else 256
H = int(sys.argv[2]) if len(sys.argv) > 2 else W

img = Image.new('L', (W, H), 0)
draw = ImageDraw.Draw(img)

# No outline — single clean edge at each boundary
draw.rectangle([W//6, H//6, W//2, H//2],outline=205, fill=100)
draw.ellipse([W//3, H//3, W*6//7, H*6//7], outline=255,fill=150)
draw.line([0, H//2, W, H//2], fill=255, width=2)
draw.line([W//2, 0, W//2, H], fill=255, width=2)

fname = f'test_{W}x{H}.raw'
open(fname, 'wb').write(img.tobytes())
print(f'Generated {fname} ({W}x{H})')
