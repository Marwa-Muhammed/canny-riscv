
from PIL import Image, ImageDraw

img = Image.new('L', (256, 256), 0)

draw = ImageDraw.Draw(img)

draw.rectangle([40, 40, 150, 150], outline=255, fill=100)

draw.ellipse([100, 100, 220, 220], outline=255, fill=150)

draw.line([0, 128, 256, 128], fill=255, width=2)

draw.line([128, 0, 128, 256], fill=255, width=2)

open('test.raw', 'wb').write(img.tobytes())

print('Generated test.raw')

