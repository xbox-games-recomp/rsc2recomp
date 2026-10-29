"""latest.py <frames dir> <out png>: newest BMP in the dir -> 800x600 PNG, prints its name."""
import glob
import os
import sys

from PIL import Image

fs = sorted(glob.glob(os.path.join(sys.argv[1], "*.bmp")), key=os.path.getmtime)
if not fs:
    sys.exit("no frames")
Image.open(fs[-1]).convert("RGB").resize((800, 600)).save(sys.argv[2])
print(os.path.basename(fs[-1]), len(fs))
