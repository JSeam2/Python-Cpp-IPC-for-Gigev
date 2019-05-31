import subprocess
import numpy as np
import cv2
import traceback as tb
import matplotlib.pyplot as plt

# Obtained from running genicam with print statements
IMG_HEIGHT = 1024
IMG_WIDTH = 1280
IMG_DEPTH = 1

process = subprocess.Popen(
    "./cpp/genicam",
    # stdin=subprocess.PIPE,
    stdout=subprocess.PIPE)

while True:
    try:
        data = process.stdout.read(IMG_HEIGHT * IMG_WIDTH)
        image = np.fromstring(data, dtype='uint8')
        image = image.reshape((IMG_HEIGHT, IMG_WIDTH))
        print(image)
        # cv2.imshow('Video', image)
        plt.imshow(image)
        plt.show()
    except ValueError as e:
        tb.print_exc()
        continue


process.stdout.flush()
cv2.destroyAllWindows()