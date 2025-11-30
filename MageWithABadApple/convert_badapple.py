#!/usr/bin/env python3

import os
import argparse
from PIL import Image
import sys

def pack_row_bits_msb(row_pixels):
    """Pack a row of 0/1 pixels into bytes MSB-first (leftmost is highest bit)."""
    out = bytearray()
    for i in range(0, len(row_pixels), 8):
        byte = 0
        for bit in range(8):
            byte <<= 1
            byte |= (row_pixels[i + bit] & 1)
        out.append(byte)
    return out

def pack_row_bits_lsb(row_pixels):
    """Pack a row of 0/1 pixels into bytes LSB-first (leftmost is bit0)."""
    out = bytearray()
    for i in range(0, len(row_pixels), 8):
        byte = 0
        for bit in range(8):
            byte |= ((row_pixels[i + bit] & 1) << bit)
        out.append(byte)
    return out

def convert_image_to_raw(img_path, width, height, out_path, lsb=False):
    im = Image.open(img_path).convert('L')  # grayscale
    im = im.resize((width, height), Image.LANCZOS)
    # Threshold to get 0/1 values. Use 128 threshold; tweak later.
    bw = im.point(lambda p: 1 if p < 128 else 0, '1')  # '1' mode: 1-bit pixels, black and white
    pixels = list(bw.getdata())
    # ensure each pixel is 0 or 1
    pixels = [1 if p else 0 for p in pixels]

    with open(out_path, 'wb') as f:
        for y in range(height):
            row = pixels[y * width:(y + 1) * width]
            # pad row to multiple of 8
            if len(row) % 8 != 0:
                pad = 8 - (len(row) % 8)
                row += [0] * pad
            if lsb:
                f.write(pack_row_bits_lsb(row))
            else:
                f.write(pack_row_bits_msb(row))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--src', required=True, help='Source frames directory (png,jpg...)')
    parser.add_argument('--width', required=True, type=int, help='Target width in pixels')
    parser.add_argument('--height', required=True, type=int, help='Target height in pixels')
    parser.add_argument('--out', required=True, help='Output directory for .raw frames')
    parser.add_argument('--ext', default='.raw', help='Output extension (default .raw)')
    parser.add_argument('--prefix', default='frame_', help='Output filename prefix')
    parser.add_argument('--lsb', action='store_true', help='Use LSB-first packing (default is MSB-first)')
    args = parser.parse_args()

    if not os.path.isdir(args.src):
        print("Source directory not found:", args.src)
        sys.exit(1)
    os.makedirs(args.out, exist_ok=True)

    # sort input files to preserve order (user should extract zero-padded frames)
    files = sorted([f for f in os.listdir(args.src) if os.path.isfile(os.path.join(args.src, f))])
    if not files:
        print("No files found in source directory.")
        return

    for i, fname in enumerate(files):
        src = os.path.join(args.src, fname)
        out_name = f"{args.prefix}{i:05d}{args.ext}"
        out = os.path.join(args.out, out_name)
        try:
            convert_image_to_raw(src, args.width, args.height, out, lsb=args.lsb)
            print(f"Converted {src} -> {out}")
        except Exception as e:
            print(f"Failed to convert {src}: {e}")

if __name__ == '__main__':
    main()