import cv2
import numpy as np
import sys

if len(sys.argv) < 2:
    print("Specify file name!")
    exit()

# Read the raw Bayer BGGR image file as binary
file_path = sys.argv[1]

width = int(file_path.split("_")[0])
height = int(file_path.split("_")[1])

with open(file_path, 'rb') as f:
    raw_data = f.read()

# Convert the raw data to a numpy array
bayer_image = np.frombuffer(raw_data, dtype=np.uint8).reshape((height, width))

# Display the raw Bayer image
cv2.imwrite('raw_bayer_image.png', bayer_image)

# Apply demosaicing
demosaiced_image = cv2.cvtColor(bayer_image, cv2.COLOR_BAYER_BG2BGR)

# Display the demosaiced image
cv2.imwrite('demosaic_image.png', demosaiced_image)