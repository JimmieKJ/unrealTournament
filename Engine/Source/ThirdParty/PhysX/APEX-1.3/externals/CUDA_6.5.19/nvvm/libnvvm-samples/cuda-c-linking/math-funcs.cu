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

// Simple implementation of Mandelbrot set from Wikipedia
// http://en.wikipedia.org/wiki/Mandelbrot_set

// Note that this kernel is meant to be a simple, straight-forward
// implementation, and so may not represent optimized GPU code.
extern "C"
__device__
void mandelbrot(float* Data) {

  // Which pixel am I?
  unsigned DataX = blockIdx.x * blockDim.x + threadIdx.x;
  unsigned DataY = blockIdx.y * blockDim.y + threadIdx.y;
  unsigned Width = gridDim.x * blockDim.x;
  unsigned Height = gridDim.y * blockDim.y;

  float R, G, B, A;

  // Scale coordinates to (-2.5, 1) and (-1, 1)

  float NormX = (float)DataX / (float)Width;
  NormX *= 3.5f;
  NormX -= 2.5f;

  float NormY = (float)DataY / (float)Height;
  NormY *= 2.0f;
  NormY -= 1.0f;

  float X0 = NormX;
  float Y0 = NormY;

  float X = 0.0f;
  float Y = 0.0f;

  unsigned Iter = 0;
  unsigned MaxIter = 1000;

  // Iterate
  while(X*X + Y*Y < 4.0f && Iter < MaxIter) {
    float XTemp = X*X - Y*Y + X0;
    Y = 2.0f*X*Y + Y0;

    X = XTemp;

    Iter++;
  }

  unsigned ColorG = Iter % 50;
  unsigned ColorB = Iter % 25;

  R = 0.0f;
  G = (float)ColorG / 50.0f;
  B = (float)ColorB / 25.0f;
  A = 1.0f;

  Data[DataY*Width*4+DataX*4+0] = R;
  Data[DataY*Width*4+DataX*4+1] = G;
  Data[DataY*Width*4+DataX*4+2] = B;
  Data[DataY*Width*4+DataX*4+3] = A;
}
