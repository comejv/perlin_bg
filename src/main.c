#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>

#define FNL_IMPL
#include "FastNoiseLite.h"
#define MSF_GIF_IMPL
#include "msf_gif.h"

int main(void)
{
  const int screenW = 720;
  const int screenH = 480;
  const int zMax = 200;

  InitWindow(screenW, screenH, "3D Noise");
  SetTargetFPS(30);

  // 1) Setup FastNoiseLite
  fnl_state noise = fnlCreateState();
  noise.noise_type = FNL_NOISE_OPENSIMPLEX2;
  noise.frequency = 0.01f;
  noise.fractal_type = FNL_FRACTAL_RIDGED;
  noise.octaves = 2;
  noise.lacunarity = 0.950f;

  // 2) Allocate buffer
  unsigned char *slice = malloc(screenW * screenH * 4);
  if (!slice)
  {
    CloseWindow();
    return 1;
  }

  // 3) Create the Image + Texture
  Image img = {
      .data = slice,
      .width = screenW,
      .height = screenH,
      .mipmaps = 1,
      .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8};
  ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
  Texture2D tex = LoadTextureFromImage(img);

  // 4) Disable filtering & clamp edges
  SetTextureFilter(tex, TEXTURE_FILTER_POINT);
  SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);

  int z = 0;
  while (!WindowShouldClose())
  {
    // 5) Fill the slice buffer
    for (int y = 0; y < screenH; y++)
      for (int x = 0; x < screenW; x++)
      {
        float v = (fnlGetNoise3D(&noise, x, y, z) + 1.0f) * 0.5f;
        if (v < 0)
          v = 0;
        else if (v > 1)
          v = 1;
        ssize_t i = y * (screenW) + x;
        slice[i * 3 + 0] = v * 255;
        slice[i * 3 + 1] = v * 10;
        slice[i * 3 + 2] = v * 10;
      }
    // 6) Push it to the GPU
    UpdateTexture(tex, slice);

    // 7) Draw it
    BeginDrawing();
    ClearBackground(BLUE);
    DrawTexture(tex, 0, 0, WHITE);
    EndDrawing();

    z = (z + 1) % zMax;
  }

  // Cleanup
  UnloadTexture(tex);
  free(slice);
  CloseWindow();
  return 0;
}
