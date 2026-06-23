#pragma once

// Same struct, two bool flags bolted on — net accretion of boolean state.
struct RenderOptions
{
  int width = 0;
  int height = 0;
  bool vsync = false;
  bool fullscreen = false;
  bool hdr = false;
};
