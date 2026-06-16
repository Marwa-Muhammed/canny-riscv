import numpy as np
import matplotlib.pyplot as plt

W, H = 64, 64

images = ["test_rect", "test_horizontal", "test_vertical", "test_diagonal", "test_circle"]

for img in images:
    fig, axes = plt.subplots(2, 3, figsize=(12, 8))
    fig.suptitle(f'Canny Pipeline: {img}', fontsize=14)

    files = {
        'Input':            f'{img}.raw',
        'Gaussian Blur':    f'{img}_gaussian.raw',
        'Magnitude':        f'{img}_magnitude.raw',
        'NMS':              f'{img}_nms.raw',
        'Double Threshold': f'{img}_threshold.raw',
        'Final Edges':      f'{img}_final.raw',
    }

    for ax, (title, path) in zip(axes.flat, files.items()):
        try:
            data = np.fromfile(path, dtype=np.uint8).reshape((H, W))
            ax.imshow(data, cmap='gray')
        except:
            ax.text(0.5, 0.5, 'Not found', ha='center')
        ax.set_title(title)
        ax.axis('off')

    plt.tight_layout()
    plt.savefig(f'{img}_result.png')
    plt.show()
    print(f"Saved: {img}_result.png")
