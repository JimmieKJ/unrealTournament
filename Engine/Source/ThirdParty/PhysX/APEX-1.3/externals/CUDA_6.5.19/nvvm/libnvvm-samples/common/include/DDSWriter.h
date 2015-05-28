/*
 * Copyright 1993-2012 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */


#ifndef DDSWRITER_H
#define DDSWRITER_H

#include <fstream>

typedef int DWORD;

/// DDS File Structures
struct DDSPixelFormat {
  DWORD Size;
  DWORD Flags;
  DWORD FourCC;
  DWORD RGBBitCount;
  DWORD RBitMask;
  DWORD GBitMask;
  DWORD BBitMask;
  DWORD ABitMask;
};

struct DDSHeader {
  DWORD           Size;
  DWORD           Flags;
  DWORD           Height;
  DWORD           Width;
  DWORD           PitchOrLinearSize;
  DWORD           Depth;
  DWORD           MipMapCount;
  DWORD           Reserved1[11];
  DDSPixelFormat  PixelFormat;
  DWORD           Caps;
  DWORD           Caps2;
  DWORD           Caps3;
  DWORD           Caps4;
  DWORD           Reserved2;
};

#define DDPF_ALPHAPIXELS 0x1
#define DDPF_RGB 0x40

#define DDSD_CAPS 0x1
#define DDSD_HEIGHT 0x2
#define DDSD_WIDTH 0x4
#define DDSD_PIXELFORMAT 0x1000

#define DDSCAPS_TEXTURE 0x1000


/// WriteDDS - Writes image data to a .dds file.  The data is expected to be
/// (R, G, B, A) tuples of 32-bit floating-point data of dimensions
/// width X height.  The floating-point data should be normalized to [0,1] and
/// will be scaled to a [0, 255] 8-bit value.
void writeDDS(const char *filename, const float *data, unsigned width,
              unsigned height) {
  // Write out the result as a .dds file
  // This is a quick and dirty DDS writer
  DDSHeader header;
  memset(&header, 0, sizeof(DDSHeader));

  header.Size = sizeof(DDSHeader);
  header.Flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
  header.Height = height;
  header.Width = width;
  header.PitchOrLinearSize = width*4;
  header.Caps = DDSCAPS_TEXTURE;

  header.PixelFormat.Size = sizeof(DDSPixelFormat);
  header.PixelFormat.Flags = DDPF_RGB | DDPF_ALPHAPIXELS;
  header.PixelFormat.RGBBitCount = 32;
  header.PixelFormat.ABitMask = 0xFF000000;
  header.PixelFormat.RBitMask = 0x00FF0000;
  header.PixelFormat.GBitMask = 0x0000FF00;
  header.PixelFormat.BBitMask = 0x000000FF;

  std::ofstream str(filename, std::ios::binary);
  int magic = 0x20534444;
  str.write((const char*)&magic, 4);
  str.write((const char*)&header, sizeof(header));
  for(unsigned j = 0; j < height; ++j) {
    for(unsigned i = 0; i < width; ++i) {
      unsigned char r, g, b, a;
      r = (unsigned char)(data[j*width*4+i*4+0] * 255.0);
      g = (unsigned char)(data[j*width*4+i*4+1] * 255.0);
      b = (unsigned char)(data[j*width*4+i*4+2] * 255.0);
      a = (unsigned char)(data[j*width*4+i*4+3] * 255.0);
      str.write((const char*)&b, 1);
      str.write((const char*)&g, 1);
      str.write((const char*)&r, 1);
      str.write((const char*)&a, 1);
    }
  }
  str.close();
}

#endif
