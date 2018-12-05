#define __USE_MINGW_ANSI_STDIO 1
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <cassert>
#include <limits>
#include <ctime>
#include <cstdarg>
#include "engine.h"
#include "items_helper.h"

Canvas::Canvas(SDL_Surface *s) {
  // Convert the surface to a bit-compatible format.
  auto conv = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGBA32, 0);

  // Setup internal fields.
  w = conv->w;
  h = conv->h;
  d.resize(w * h);
  texel_cache.resize(w * h);

  // Copy the data and destroy the temporary surface.
  memcpy(d.data(), conv->pixels, w * h * 4);
  SDL_FreeSurface(conv);
}

SDL_Surface* Canvas::to_surface() const {
  return SDL_CreateRGBSurfaceFrom(
      (void*)d.data(), w, h, 32, w * 4,
      0xff << 0, 0xff << 8, 0xff << 16, 0xff << 24);
}

void Canvas::reset() {
  std::fill(d.begin(), d.end(), RGBA{0, 0, 0, 0});
}

void Canvas::dump(const std::string& fname) {
  /*FILE *f = fopen(fname.c_str(), "wb");
  if (f == nullptr) {
    return;
  }

  fwrite(d.data(), 1, d.size() * sizeof(RGBA), f);

  fclose(f);*/
  auto s = this->to_surface();
  IMG_SavePNG(s, fname.c_str());
  SDL_FreeSurface(s);
}

void Canvas::line(Coords s, Coords e, RGBA color) {
  int diffx = e.x - s.x;
  int diffy = e.y - s.y;
  double length = sqrt((double)(diffx * diffx + diffy * diffy));
  double dx = (double)diffx / length;
  double dy = (double)diffy / length;
  double x = s.x;
  double y = s.y;

  for (double step = 0.0; step <= length; step += 1.0, x += dx, y += dy) {
    int px = (int)x;
    int py = (int)y;

    if (px < 0 || (unsigned int)px >= w) continue;
    if (py < 0 || (unsigned int)py >= h) continue;

    d[px + py * w] = color;
  }
}

void Canvas::copy_fast(Canvas *src) {
  assert(w == src->w);  // Don't use copy_fast if this is not the case.
  assert(h <= src->h);

  memcpy(d.data(), src->d.data(), d.size() * 4);
}

void Canvas::copy_fast(Canvas *src, int dst_y) {
  assert(w == src->w);  // Don't use copy_fast if this is not the case.
  assert(h <= dst_y + src->h);

  memcpy(d.data() + dst_y * w, src->d.data(), src->d.size() * 4);
}

void Canvas::copy(Canvas *src,
                 int src_x, int src_y,
                 int dst_x, int dst_y,
                 int src_w, int src_h) {
  assert(dst_x >= 0 && dst_y >= 0);  // I might want to support this, but...
  assert(src_x >= 0 && src_y >= 0);
  src_w = std::min(src_w, (int)w - dst_x);
  src_h = std::min(src_h, (int)h - dst_y);

  for (int j = 0; j < src_h; j++) {
    for (int i = 0; i < src_w; i++) {
      const size_t src_idx = i + src_x + (j + src_y) * src->w;
      const size_t dst_idx = i + dst_x + (j + dst_y) * w;

      const RGBA& src_px = src->d[src_idx];
      RGBA& dst_px = d[dst_idx];
      dst_px = src_px;
    }
  }
}

void Canvas::blit(Canvas *src,
                 int src_x, int src_y,
                 int dst_x, int dst_y,
                 int src_w, int src_h) {
  assert(dst_x >= 0 && dst_y >= 0);  // I might want to support this, but...
  assert(src_x >= 0 && src_y >= 0);
  src_w = std::min(src_w, (int)w - dst_x);
  src_h = std::min(src_h, (int)h - dst_y);

  for (int j = 0; j < src_h; j++) {
    for (int i = 0; i < src_w; i++) {
      const size_t src_idx = i + src_x + (j + src_y) * src->w;
      const size_t dst_idx = i + dst_x + (j + dst_y) * w;

      const RGBA& src_px = src->d[src_idx];
      RGBA& dst_px = d[dst_idx];
      if (src_px.a == 255) {
        dst_px = src_px;
        continue;
      }

      if (src_px.a == 0) {
        continue;
      }

      dst_px = RGBA{
        (uint8_t)(((uint32_t)src_px.r * src_px.a + (uint32_t)dst_px.r * (255 - src_px.a)) >> 8),
        (uint8_t)(((uint32_t)src_px.g * src_px.a + (uint32_t)dst_px.g * (255 - src_px.a)) >> 8),
        (uint8_t)(((uint32_t)src_px.b * src_px.a + (uint32_t)dst_px.b * (255 - src_px.a)) >> 8),
        std::max(src_px.a, dst_px.a)
      };
    }
  }
}

void Canvas::blit(Canvas *src, int dst_x, int dst_y) {
  blit(src, 0, 0, dst_x, dst_y, src->w, src->h);
}

RGBA Canvas::get_texel(Coordsf p, Coordsf sz) {
  int sz_x = std::max(1, (int)(sz.x * w));
  int sz_y = std::max(1, (int)(sz.y * h));

  int x = (int)(std::clamp(p.x, 0.0f, 1.0f) * (float)(w - 1));
  int y = (int)(std::clamp(p.y, 0.0f, 1.0f) * (float)(h - 1));

  if (sz_x == 1 && sz_y == 1) {
    return d[x + y * w];
  }

  // Try cache first.
  auto& texel_pixel_cache = texel_cache[x + y * w];
  uint32_t texel_sig = sz_x + (sz_y << 16);
  auto texel_it = texel_pixel_cache.find(texel_sig);
  if (texel_it != texel_pixel_cache.end()) {
    return texel_it->second;
  }

  // The long way.
  unsigned int count = 0;
  unsigned int r = 0;
  unsigned int g = 0;
  unsigned int b = 0;
  uint8_t a = 0;  // Use max of alpha instead of avg.

  for (unsigned int j = std::max(0, y - sz_y / 2);
                    j <= (unsigned int)std::min((int)h - 1, y + sz_y / 2);
                    j++) {
    for (unsigned int i = std::max(0, x - sz_x / 2);
                      i <= (unsigned int)std::min((int)w - 1, x + sz_x / 2);
                      i++) {
      const unsigned idx = i + j * w;
      if (d[idx].a != 255) continue;
      r += d[idx].r;
      g += d[idx].g;
      b += d[idx].b;
      a = std::max(a, d[idx].a);
      count++;
    }
  }

  if (count == 0 || a != 255) {
    return RGBA{0, 0, 0, 0};
  }

  RGBA res{
    (uint8_t)(r / count),
    (uint8_t)(g / count),
    (uint8_t)(b / count),
    a
  };

  // Fit into cache.
  texel_pixel_cache[texel_sig] = res;

  return res;
}

bool ImageManager::load(const std::string& id,
                        const std::string& fname) {
  auto s = IMG_Load(fname.c_str());
  if (s == nullptr) {
    return false;
  }

  images[id] = std::make_unique<Canvas>(s);
  SDL_FreeSurface(s);
  return true;
}

void ImageManager::add(const std::string& id, Canvas *c) {
  assert(images.find(id) == images.end());
  images[id].reset(c);
}

Canvas* ImageManager::get(std::string id) {
  if (images.find(id) == images.end()) {
    return nullptr;
  }

  return images[id].get();
}


Coords Render3D::point3D_to_2D(Coords3D p) {
  // This will behave weird with p.z == 0, but we don't really care.
  const float z = (p.z) * PERSPECTIVE_CORRECTION;

  // The 1.8f in Y coord is the height of the player.
  const float flat_x = p.x / z;
  const float flat_y = (p.y - EYE_LEVEL) / z;

  const float pixel_x = flat_x * SCALE_3D + (WIDTH_3DF / 2.0f);
  const float pixel_y = flat_y * SCALE_3D + (WIDTH_3DF - HEIGHT_3DF) / 2.0f;

  return Coords{(int)pixel_x, (int)pixel_y};
}

float Render3D::y_scanline_to_z(int y_scanline, float y) {
  return ((y - EYE_LEVEL) * SCALE_3D) /
         (PERSPECTIVE_CORRECTION * (
              (float)y_scanline - (WIDTH_3DF - HEIGHT_3DF) / 2.0f));
}

float Render3D::x_scanline_to_z(int x_scanline, Coords3D near, Coords3D far) {
  const float Xn = near.x;
  const float Xd = far.x - near.x;

  const float Zn = near.z;
  const float Zd = far.z - near.z;

  const float K =
      PERSPECTIVE_CORRECTION * ( (float)x_scanline - 0.5f * WIDTH_3DF );

  const float Z =
      ( SCALE_3D * ( -Zn * Xd + Xn * Zd ) ) /
      ( K * Zd - Xd * SCALE_3D );

  return Z;
}

float Render3D::first_z_on_screen(float z, float y) {

  const float boundary_z = (y >= EYE_LEVEL) ?
      y_scanline_to_z(HEIGHT_3D - 1, y) :  // Lower than eyesight.
      y_scanline_to_z(0, y);  // Higher than eyesight.

  // Nothing to do if z is already within the boundary.
  if (z >= boundary_z) {
    return z;
  }

  return boundary_z;
}

void Render3D::line3D(Coords3D s, Coords3D e, RGBA color) {
  const float STEP = 0.01f;

  const float diffx = e.x - s.x;
  const float diffy = e.y - s.y;
  const float diffz = e.z - s.z;
  const float length = sqrt(diffx * diffx + diffy * diffy + diffz * diffz);
  const float dx = (diffx / length) * STEP;
  const float dy = (diffy / length) * STEP;
  const float dz = (diffz / length) * STEP;
  float x = s.x;
  float y = s.y;
  float z = s.z;

  for (float step = 0.0; step <= length;
       step += STEP, x += dx, y += dy, z += dz) {
    if (z <= 0.0f) {  // In front of screen, do not render.
      continue;
    }

    Coords p = point3D_to_2D(Coords3D{x, y, z});

    int px = p.x;
    int py = p.y;

    if (px < 0 || (unsigned int)px >= c->w) continue;
    if (py < 0 || (unsigned int)py >= c->h) continue;

    pixel3D(Coords{p.x, p.y}, z, color);
  }
}

void Render3D::vquad(Coords3D s, Coords3D e, std::string texture_id) {
  // "n" is near, "f" is far
  const float Zn = s.z < e.z ? s.z : e.z;
  const float Zf = s.z < e.z ? e.z : s.z;
  const float Xn = s.z < e.z ? s.x : e.x;
  const float Xf = s.z < e.z ? e.x : s.x;

  // So, if Zf is in front of the screen that's it.
  if (Zf <= 0.0f) {
    return;
  }

  // "t" is top, "b" is bottom
  const float Yt = std::min(s.y, e.y);
  const float Yb = std::max(s.y, e.y);

  // "d" is for delta or difference
  const float Zd = Zf - Zn;
  const float Xd = Xf - Xn;

  // We might need to fix Z in case it's in front of the screen.
  float fixed_Zn = Zn;
  float fixed_Xn = Xn;

  const Coords3D near{Xn, 0.0f, Zn};
  const Coords3D far{Xf, 0.0f, Zf};

  if (Zn <= 0.0f && Zd != 0.0f) {  // Strict comparing a float.
                                   // I too like to live dangerously.
    float Zcandidate;
    if (Xd != 0.0f) {
      Zcandidate = (Xd < 0.0f) ?
        x_scanline_to_z(0.0f, near, far) :
        x_scanline_to_z(WIDTH_3DF - 1.0f, near, far);
    } else {
      Zcandidate = (Xn < 0.0f) ?
        x_scanline_to_z(0.0f, near, far) :
        x_scanline_to_z(WIDTH_3DF - 1.0f, near, far);
    }

    if (Zcandidate >= Zn && Zcandidate <= Zf) {
      fixed_Zn = Zcandidate;

      const float p = (fixed_Zn - Zn) / Zd;
      fixed_Xn = p * Xd + Xn;
    } else if (Zcandidate > Zf) {
      // Actually not on screen.
      return;
    }
  }

  const int edge_near_2D = point3D_to_2D(Coords3D{fixed_Xn, 0.0f, fixed_Zn}).x;
  const int edge_far_2D = point3D_to_2D(Coords3D{Xf, 0.0f, Zf}).x;

  const int edge_left_2D = std::min(edge_near_2D, edge_far_2D);
  const int edge_right_2D = std::max(edge_near_2D, edge_far_2D);
  const int edge_hor_diff = edge_right_2D - edge_left_2D;

  //printf("edges: %i <--> %i\n", edge_left_2D, edge_right_2D);

  if (edge_right_2D < 0 || edge_left_2D >= WIDTH_3D) {
    return;
  }

  const int clamped_edge_left_2D = std::clamp(edge_left_2D, 0, WIDTH_3D - 1);
  const int clamped_edge_right_2D = std::clamp(edge_right_2D, 0, WIDTH_3D - 1);

  Canvas *texture = img->get(texture_id);
  if (texture == nullptr) {
    printf("error: missing texture '%s'\n", texture_id.c_str());
    return;
  }

  // For each vertical scanline (scancolumn?).

  const float texel_sz_hor = 1.0f / (float)(edge_hor_diff);
  for (int i = clamped_edge_left_2D; i <= clamped_edge_right_2D; i++) {
    const float z = x_scanline_to_z(i, near, far);
    const float p = edge_hor_diff == 0 ?
        0.0f : (float)(i - edge_left_2D) / (float)(edge_hor_diff);
    const float x = p * Xd + Xn;

    int edge_top_2D = point3D_to_2D(Coords3D{x, Yt, z}).y;
    int edge_bottom_2D = point3D_to_2D(Coords3D{x, Yb, z}).y;

    if (edge_bottom_2D < 0 || edge_top_2D >= HEIGHT_3D) {
      continue;
    }

    int clamped_edge_top_2D = std::clamp(edge_top_2D, 0, HEIGHT_3D - 1);
    int clamped_edge_bottom_2D = std::clamp(edge_bottom_2D, 0, HEIGHT_3D - 1);

    int edge_vert_diff = edge_bottom_2D - edge_top_2D;
    if (edge_vert_diff == 0) {
      edge_vert_diff = 1;
    }

    const float texel_sz_vert = 1.0f / (float)(edge_vert_diff);
    for (int j = clamped_edge_top_2D; j <= clamped_edge_bottom_2D; j++) {
      // This might be a little out of place, but it gives a large FPS boost if checked early.
      if (!zbuffer_ignore && z >= zbuffer[i + j * WIDTH_3D]) {
        continue;
      }

      const float r = (float)(j - edge_top_2D) / (float)(edge_vert_diff);

      const Coordsf texel_pos = Coordsf{p, r};
      const Coordsf texel_sz = Coordsf{texel_sz_hor, texel_sz_vert};

      pixel3D(Coords{i, j}, z, texture->get_texel(texel_pos, texel_sz));
    }
  }
}

void Render3D::tile(Coords3D m, Coords3D sz, std::string texture_id) {
  // Typical use case:
  //
  //               near_z
  //        _____________________
  //        \                   /
  //         \   ceiling tile  /
  //          \               /
  //           \             /
  //            '''''''''''''
  //                far_z
  //            .............
  //           /             \
  //          /               \
  //         /   floor tile    \
  //        /___________________\
  //
  //               near_z
  //

  // TODO: Add an option to apply the texture mirrored or rotated.
  assert(sz.z > 0.0f);
  assert(sz.x > 0.0f);

  float far_z = m.z + sz.z * 0.5f;
  float near_z = m.z - sz.z * 0.5f;

  // If far_z is in front of the screen, there is nothing to do.
  if (far_z <= 0.0f) {
    return;
  }

  // If 2D(far_z) is below or above the canvas edge, there is nothing to do
  // either.
  Coords far_edge_2D = point3D_to_2D(Coords3D{m.x, m.y, far_z});
  if (far_edge_2D.y >= HEIGHT_3D || far_edge_2D.y < 0) {
    return;
  }

  // near_z might still be off screen - in that case it needs to be adjusted.
  const float fixed_near_z = first_z_on_screen(near_z, m.y);

  // Draw scanlines from top of the screen to bottom of the screen.
  Coords near_edge_2D = point3D_to_2D(Coords3D{m.x, m.y, fixed_near_z});
  const int top_edge_2D = std::min(far_edge_2D.y, near_edge_2D.y);
  const int bottom_edge_2D = std::max(far_edge_2D.y, near_edge_2D.y);

  const float left_edge_3D = m.x - sz.x * 0.5f;
  const float right_edge_3D = m.x + sz.x * 0.5f;

  Canvas *texture = img->get(texture_id);
  if (texture == nullptr) {
    printf("error: missing texture %s\n", texture_id.c_str());
    return;
  }

  float last_z = y_scanline_to_z(top_edge_2D - 1, m.y);

  for (int j = top_edge_2D; j <= bottom_edge_2D; j++) {
    const float z = y_scanline_to_z(j, m.y);
    const int start_x_2D = point3D_to_2D(Coords3D{left_edge_3D, m.y, z}).x;
    const int end_x_2D = point3D_to_2D(Coords3D{right_edge_3D, m.y, z}).x;


    const float progress_z = (z - near_z) / (far_z - near_z);

    const float texel_size_z = fabs((z - last_z) / sz.z);

    float last_x = left_edge_3D;
    for (int i = start_x_2D; i <= end_x_2D; i++) {
      const float progress_x =
          (float)(i - start_x_2D) / (float)(end_x_2D - start_x_2D + 1);

      const float x = left_edge_3D + progress_x * sz.x;
      const float texel_size_x = fabs((x - last_x) / sz.x);
      last_x = x;

      if (i < 0 || i >= WIDTH_3D) {  // Don't bother for stuff off screen.
        continue;
      }

      const Coordsf texel_sz{texel_size_x, texel_size_z};
      const Coordsf texel_pos{progress_x, progress_z};
      pixel3D(Coords{i, j}, z, texture->get_texel(texel_pos, texel_sz));

    }

    last_z = z;
  }
}

void Render3D::fog_setup(bool enable, RGBA color, float intensity) {
  this->fog_enable = enable;
  if (enable) {
    this->fog_color = color;
    this->fog_intensity = intensity;
  }
}

RGBA Render3D::fog_get_color(float z) {
  const float none_distance = FOG_NONE_DISTANCE / this->fog_intensity;
  const float max_distance = FOG_MAX_DISTANCE / this->fog_intensity;

  float alphaf;
  if (z >= max_distance) {
    alphaf = 1.0f;
  } else if (z <= none_distance) {
    alphaf = 0.0f;
  } else {
    alphaf = (z - none_distance) / (max_distance - none_distance);
  }

  const uint8_t alpha = (uint8_t)(std::clamp(alphaf, 0.0f, 1.0f) * 255.0f);
  return RGBA{
      this->fog_color.r,
      this->fog_color.g,
      this->fog_color.b,
      alpha
  };
}


// X,Z coords are of the middle of the block. It's assumed the blocks floor
// is at the z=0. The block is actually made of 5 blocks stacked on top of each
// other, each 1m in height.
void Render3D::helper_3D_block(float x, float z) {
  Coords3D points[] = {
    // Top (clockwise).
    { x - TILE_SZ * 0.5f, -3.0f, z - TILE_SZ * 0.5f },
    { x - TILE_SZ * 0.5f, -3.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, -3.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, -3.0f, z - TILE_SZ * 0.5f },

    // Bottom (clockwise).
    { x - TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f },
    { x - TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f }
  };

  vquad(points[0], points[5], "wall");
  vquad(points[3], points[6], "wall");
  vquad(points[0], points[7], "wall");

  /*line3D(points[0], points[4], color);
  line3D(points[1], points[5], color);
  line3D(points[2], points[6], color);
  line3D(points[3], points[7], color);

  line3D(points[0], points[1], color);
  line3D(points[1], points[2], color);
  line3D(points[2], points[3], color);
  line3D(points[3], points[0], color);

  line3D(points[4], points[5], color);
  line3D(points[5], points[6], color);
  line3D(points[6], points[7], color);
  line3D(points[7], points[4], color);*/

}

void Render3D::helper_3D_floor(float x, float z) {
  this->tile(
      Coords3D{x, 0.0f, z},  // Tile position.
      Coords3D{TILE_SZ, 0.0f, TILE_SZ},  // Tile size.
      "dirt");

/*
  Coords3D points[] = {
    // Bottom (clockwise).
    { x - TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f },
    { x - TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f }
  };

  line3D(points[0], points[1], color);
  line3D(points[1], points[2], color);
  line3D(points[2], points[3], color);
  line3D(points[3], points[0], color);
  */
}

void TextRenderer::hint_frame_change() {
  std::unordered_map<std::string, CachedText*>& deprecated = cache[cache_current];

  for (auto i : deprecated) {
    delete i.second->text;
    delete i.second;
  }

  deprecated.clear();

  cache_current ^= 1;
}

int TextRenderer::printf(
    Canvas *dst, int x, int y, int w, int h, const char *fmt, ...) {

  std::string formatted_text = "\0";

  va_list args;

  va_start(args, fmt);
  int required_size = vsnprintf(
      formatted_text.data(), formatted_text.size(), fmt, args);
  va_end(args);

  formatted_text.resize(required_size + 1);  // +1 for \0 (vsnprintf needs it).

  va_start(args, fmt);
  vsnprintf(
      formatted_text.data(), formatted_text.size(), fmt, args);
  va_end(args);

  formatted_text.pop_back();  // Remove the \0, std::string doesn't need it.
  return render(dst, x, y, w, h, formatted_text);
}

int TextRenderer::render(
    Canvas *dst, int x, int y, int w, int h, const std::string& text) {
  int lines_rendered;

  // The returned canvas is owned by the caching part of TextRenderer.
  Canvas *rendered_text = render_worker(text, w, h, &lines_rendered);
  dst->blit(rendered_text, 0, 0, x, y, w, h);

  return lines_rendered;
}

Canvas *TextRenderer::render_worker(
    const std::string& text, int w, int h, int *lines_rendered) {
  std::string key = cache_key(text, w, h);

  // Attempt to get the text from cache.
  CachedText *cached_text = cache_get(key);
  if (cached_text != nullptr) {
    *lines_rendered = cached_text->lines_rendered;
    return cached_text->text;
  }

  // Actually render the text.
  Canvas *rendered_text = new Canvas{(unsigned int)w, (unsigned int)h};

  // Ubuntu color scheme.
  // https://en.wikipedia.org/wiki/ANSI_escape_code#3/4_bit
  const RGBA color_scheme[] {
    { 1,1,1,255 },
    { 222,56,43,255 },
    { 57,181,74,255 },
    { 255,199,6,255 },
    { 0,111,184,255 },
    { 118,38,113,255 },
    { 44,181,233,255 },
    { 204,204,204,255 },
    { 128,128,128,255 },
    { 255,0,0,255 },
    { 0,255,0,255 },
    { 255,255,0,255 },
    { 0,0,255,255 },
    { 255,0,255,255 },
    { 0,255,255,255 },
    { 255,255,255,255 }
  };

  RGBA current_color = color_scheme[DEFAULT_TEXT_COLOR];

  // Rendering is done on a word-by-word basis.
  int x = 1;
  int y = 1;
  size_t i = 0;
  int lines_rendered_counter = 1;  // Always at least one line is rendered.
  while (i != text.size()) {
    // Handle late line wraps.
    if (x + FONT_W + 1 > w) {
      x = 1;
      y += FONT_H;
      lines_rendered_counter++;
    }

    if (y + FONT_H + 1 > h) {
      break;
    }

    unsigned char ch = (unsigned char)text[i];

    // Only handle white-chars here.
    if (ch == '\n') {
      i++;
      x = 1;
      y += FONT_H;
      lines_rendered_counter++;
      continue;
    }

    if (ch == ' ' || ch == '\t') {
      i++;
      x += FONT_W;
      continue;
    }

    // Calculate the length of the next word up to either a white-char or end
    // of string. Control characters are considered to be 0-width and non-breaking.
    // Spell characters are considered to be FONT_W * 2.
    int word_w = 0;
    size_t end_i = i;
    for (size_t j = i; j < text.size(); j++) {
      unsigned char ch = (unsigned char)text[j];

      if (ch == 0x0f || (ch >= 0x10 && ch <= 0x1f)) {
        // Control character.
        end_i++;
        continue;
      }

      if (ch == ' ' || ch == '\n' || ch == '\t') {
        break;
      }

      // Spell characters are actually handled seamlessly.
      word_w += FONT_W;
      end_i++;
    }

    // There are three cases for word wrapping from here:
    // - The word fits in the space left:
    //     --> just render it
    // - The word doesn't fit, but in general fits a line:
    //     --> render it in next line
    // - The word doesn't fit a line:
    //     --> render whatever you can in this line and continue in next lines
    int space_left = w - x;
    if (word_w + 1 > space_left && word_w + 1 <= w) {
      x = 1;
      y += FONT_H;
      lines_rendered_counter++;

      // Run out of space?
      if (y + FONT_H + 1 > h) {
        break;
      }
    }

    // Just render whatever is remaining char-by-char until either the end of
    // the world^H^Hd or space runs out.
    bool is_next_spell = false;
    for (size_t j = i; j < end_i; j++) {
      unsigned char ch = (unsigned char)text[j];

      if (is_next_spell) {
        is_next_spell = false;

        // Is there enough space for a 2-char-wide icon?
        int space_left = w - x;
        if (space_left < 2 * FONT_W + 1) {
          x = 1;
          y += FONT_H;
          lines_rendered_counter++;
        }

        // Run out of space?
        if (y + FONT_H + 1 > h) {
          break;  // This should be a double-break. Will be caught upstairs.
        }

        // Render the spell character.
        render_rune(rendered_text, x, y, ch);
        render_outline(rendered_text, x - 1, y - 1, FONT_W * 2 + 2, FONT_H + 2,
                       RGBA{0, 0, 0, 128});
        x += FONT_W * 2;
        continue;
      }

      if (ch == 0x0f) {
        current_color = color_scheme[DEFAULT_TEXT_COLOR];
        continue;
      }

      if (ch >= 0x10 && ch <= 0x1f) {
        current_color = color_scheme[ch - 0x10];
        continue;
      }

      if (ch == 0xff) {
        is_next_spell = true;
        continue;
      }

      // A normal character then. Is there enough space to render it?
      int space_left = w - x;
      if (space_left < FONT_W + 1) {
        x = 1;
        y += FONT_H;
        lines_rendered_counter++;
      }

      // Run out of space?
      if (y + FONT_H + 1 > h) {
        break;  // This should be a double-break. Will be caught upstairs.
      }

      // Render the character (if it's a printable one).
      // Note: used font doesn't seem to have ~ character.
      if (ch >= 0x20 && ch <= 0x7d) {
        render_char(rendered_text, x, y, ch, current_color);
        render_outline(rendered_text, x - 1, y - 1, FONT_W + 2, FONT_H + 2,
                       RGBA{0, 0, 0, 192});
      }
      x += FONT_W;
    }

    i = end_i;
  }

  if (i != text.size()) {
    lines_rendered_counter = -lines_rendered_counter;
  }

  // Add it to the cache.
  CachedText *new_cached_text = new CachedText;
  new_cached_text->lines_rendered = lines_rendered_counter;
  new_cached_text->text = rendered_text;
  cache_store(key, new_cached_text);

  *lines_rendered = lines_rendered_counter;
  return rendered_text;
}

void TextRenderer::render_char(
    Canvas *dst, int x, int y, unsigned char ch, RGBA color) {
  ch -= 0x20; // Fix offset.

  for (int j = 0; j < FONT_H; j++) {
    for (int i = 0; i < FONT_W; i++) {
      const RGBA& src_px = font_->d[ch * FONT_W + i + j * font_->w];
      RGBA& dst_px = dst->d[x + i + (y + j) * dst->w];

      if (src_px.a < 200) {
        continue;
      }

      dst_px = color;
    }
  }
}

void TextRenderer::render_rune(
    Canvas *dst, int x, int y, unsigned char rune) {
  if (rune < 0x40 || rune >= 0x80) {
    // Only runes between 0x40 and 0x7F are rendered. The rest are secret!
    return;
  }

  rune -= 0x40; // Fix offset.
  unsigned char bg = rune / 8;
  unsigned char fg = rune % 8;

  for (int j = 0; j < FONT_H; j++) {
    for (int i = 0; i < FONT_W * 2; i++) {
      const RGBA& bg_px = rune_bg_->d[bg * FONT_W * 2 + i + j * rune_bg_->w];
      RGBA& dst_px = dst->d[x + i + (y + j) * dst->w];

      if (bg_px.a < 200) {
        continue;
      }

      dst_px = bg_px;
    }
  }

  for (int j = 0; j < FONT_H; j++) {
    for (int i = 0; i < FONT_W; i++) {
      const RGBA& fg_px = runes_->d[fg * FONT_W + i + j * runes_->w];
      RGBA& dst_px = dst->d[x + i + 3 + (y + j) * dst->w];

      if (fg_px.a < 200) {
        continue;
      }

      dst_px = fg_px;
    }
  }
}

void TextRenderer::render_outline(
    Canvas *dst, int x, int y, int w, int h, RGBA outline) {
  for (int j = 1; j < h - 1; j++) {
    for (int i = 1; i < w - 1; i++) {

      RGBA& px = dst->d[x + i + (y + j) * dst->w];
      if (px.a < 200) {
        continue;
      }

      for (int n = -1; n <= 1; n++) {
        for (int m = -1; m <= 1; m++) {
          RGBA& px = dst->d[x + i + m + (y + j + n) * dst->w];
          if (px.a >= 200) {
            continue;
          }

          px = outline;
        }
      }
    }
  }
}


std::string TextRenderer::cache_key(const std::string& text, int w, int h) {
  std::string s;
  s.resize(text.size() + 4);
  text.copy(s.data() + 4, text.size(), 0);

  uint16_t enc_w = uint16_t(w);
  uint16_t enc_h = uint16_t(h);
  memcpy(s.data() + 0, &enc_w, sizeof(uint16_t));
  memcpy(s.data() + 2, &enc_h, sizeof(uint16_t));

  return s;
}

TextRenderer::CachedText *TextRenderer::cache_get(const std::string& key) {
  std::unordered_map<std::string, CachedText*>& current = cache[cache_current];
  std::unordered_map<std::string, CachedText*>& next = cache[cache_current ^ 1];

  auto iter = current.find(key);
  if (iter == current.end()) {
    // Not is current cache. Check next cache.
    iter = next.find(key);
    if (iter == next.end()) {
      // Not found either.
      return nullptr;
    }

    // Found in next cache, return it.
    return iter->second;
  }

  // Found in current cache. Move to the next cache and return.
  CachedText *cached_text = iter->second;
  next[key] = cached_text;
  current.erase(iter);

  return cached_text;
}

void TextRenderer::cache_store(const std::string& key, CachedText *rendered_text) {
  std::unordered_map<std::string, CachedText*>& next = cache[cache_current ^ 1];
  next[key] = rendered_text;
}

void Console::set_text_renderer(TextRenderer *txt) {
  this->txt = txt;
}

void Console::render_to(Canvas *dst, int x, int y) {
  // Fade out scanlines if needed.
  if (fadeout_time > 0.0f) {
    auto time_now = std::chrono::steady_clock::now();
    std::chrono::duration<float> diff = time_now - time_start;
    float now = diff.count();

    // Go form the end - this way the loop can stop early when it reaches the
    // lines that are already on float::max.
    for (int j = int(line_time.size()) - 1; j >= 0; j--) {

      // Line still active or already in inactive state.
      const float deadline = line_time[j] + fadeout_time;
      if (now < deadline) {
        // OK, this check is kinda crazy due to the addition a few lines back.
        // However, due to the way float works, float::max + a decently small
        // number (well, actually it can be really big) gives float::max. So
        // it's fine. A hack, but still fine.
        if (deadline == std::numeric_limits<float>::max()) {
          break;
        }

        continue;
      }

      // Fadeout or set inactive.
      float diff = now - deadline;
      uint8_t a = 0;

      if (diff > 1.0f) {
        line_time[j] = std::numeric_limits<float>::max();
      } else {
        a = (127 - uint8_t(diff * 127.0f)) & 0xf0;
      }

      for (unsigned int i = 0; i < c.w; i++) {
        RGBA& px = c.d[i + j * c.w];
        if (px.a == 0) {
          continue;
        }
        px.a = a;
      }
    }
  }

  // Render.
  if (show_input_prompt) {
    const int input_height = FONT_H + 2;
    dst->blit(&c, 0, input_height, x, y, c.w, c.h - input_height);
    txt->printf(dst, x, y + c.h - input_height, c.w, input_height,
                "\x13" "%s", prompt.c_str());

    // Let's hope prompt never has rune markers, nor does input.
    // TODO: Something to fix.
    const int prompt_width = prompt.size() * FONT_W + 2;
    const int space_left = c.w - prompt_width;
    const int space_left_chars = (space_left - 2) / FONT_W - 1;
    std::string cursor{"\x13" "|"};  // TODO: Blink cursor.
    // TODO: Allow the color to be set.

    std::string to_show;  // TODO: Maybe some string stream?
    to_show += "\x12";  // TODO: Add some function to allow color change.

    if (input_text_.size() <= size_t(space_left_chars)) {
      to_show += input_text_;
    } else {
      // Show only the last space_left_chars-3 letters.
      to_show += "...";
      to_show += input_text_.substr(input_text_.size() - (space_left_chars - 3));
    }

    to_show += cursor;
    txt->render(dst, x + prompt_width, y + c.h - input_height,
                c.w - prompt_width, input_height,
                to_show);
  } else {
    dst->blit(&c, x, y);
  }
}

void Console::puts(const std::string& s) {
  // Render to temporary canvas.
  int line_no = txt->render(&tmp, 0, 0, tmp.w, tmp.h, s);

  // In case not everything rendered. Oh well, too bad.
  if (line_no < 0) {
    line_no = -line_no;
  }

  // Move the current console up by that amount and clear/prefill the new part.
  int height = line_no * FONT_H + 2;
  if (height > int(c.h)) {
    height = int(c.h);
  }

  int dst_y = c.h - height;
  if (dst_y < 0) {
    dst_y = 0;
  }

  //printf("'%s' --> %i lines, %i pixels\n", s.c_str(), line_no, height);

  memmove(c.d.data(), c.d.data() + height * c.w,
          (c.d.size() - height * c.w) * sizeof(RGBA));

  memmove(line_time.data(), line_time.data() + height,
          (line_time.size() - height) * sizeof(float));

  const auto time_now = std::chrono::steady_clock::now();
  const std::chrono::duration<float> diff = time_now - time_start;
  const float now = diff.count();
  std::fill(line_time.begin() + dst_y, line_time.end(), now);

  // Copy the pixel data.
  c.copy(&tmp, 0, 0, 0, dst_y, tmp.w, height);

  // Clear the temporary canvas.
  std::fill(tmp.d.begin(), tmp.d.begin() + height * tmp.w, RGBA{0, 0, 0, 0});
}

void Console::set_prompt(const std::string& new_prompt, bool show) {
  prompt = new_prompt;
  show_input_prompt = show;
}

std::string& Console::input_text() {
  return input_text_;
}

bool Engine::load_textures() {
  struct ImagesToLoad {
    const char *id;
    const char *fname;
  };

  const ImagesToLoad IMAGES_TO_LOAD[] =
    #include "texture_list.h"

  bool res = true;

  for (const ImagesToLoad *el = IMAGES_TO_LOAD; el->id; el++) {
    //printf("Loading %s...", el->fname);
    //fflush(stdout);
    if (!img.load(el->id, el->fname)) {
      printf("Loading %s...", el->fname);
      puts("ERROR!");
      res = false;
    } /*else {
      puts("");
    }*/
  }

  return res;
}

void Engine::pregenerate_texture(const std::string& gfx_id) {
  auto item = item_to_sprite_info(gfx_id);
  if (!item.is_sprite) {
    return;  // Not needed.
  }

  auto c = std::make_unique<Canvas>(item.w, item.h);
  c->copy(img.get(item.img),
          item.x, item.y,
          0, 0,
          item.w, item.h);
  img.add(gfx_id, c.release());
}

bool Engine::initialize() {
  if (!load_textures()) {
    puts("error: texture load failed");
    return false;
  }

  init_sprite_map();  // Note: This must happen after load_textures.
  auto keys = item_sprite_map_keys();
  for (const auto& k : keys) {
    pregenerate_texture(k);
  }

  txt.reset(new TextRenderer(
      img.get("ui_font"),
      img.get("ui_rune_bg"),
      img.get("ui_runes")
  ));

  in_game_text.set_text_renderer(txt.get());
  debug_con.set_text_renderer(txt.get());

  if (!world.load("map.data")) {
    puts("error: world map load failed");
    return false;
  }

  return true;
}

void Engine::tile_grassland(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 1) return;

  r3d.tile(
      Coords3D{x, 0.0f, z},  // Tile position.
      Coords3D{TILE_SZ * 1.25f, 0.0f, TILE_SZ * 1.25f},  // Tile size.
      (const char*[4]){
          "3d_grassA", "3d_grassB", "3d_grassC", "3d_grassD"}[t.variant & 3]);
}

void Engine::tile_water(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 0) return;

  (void)t;
  r3d.tile(
      Coords3D{x, 0.0f, z},  // Tile position.
      Coords3D{TILE_SZ, 0.0f, TILE_SZ},  // Tile size.
      (const char*[4]){
          "3d_waterA", "3d_waterB", "3d_waterC", "3d_waterD"}[t.variant & 3]);
}

void Engine::tile_mountains(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 4) return;

  float height = -2.0f;
  if ((t.variant & 0x80) == 0) {
    height -= (float(t.variant) / 127.0f) * 12.7f * 3.0f;
  }

  const Coords3D points[] = {
    // Top (clockwise).
    { x - TILE_SZ * 0.5f, height, z - TILE_SZ * 0.5f },
    { x - TILE_SZ * 0.5f, height, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, height, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, height, z - TILE_SZ * 0.5f },

    // Bottom (clockwise).
    { x - TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f },
    { x - TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f }
  };

  if (x > 0) {
    r3d.vquad(points[0], points[5], "3d_rockD");
  }

  if (x < 0) {
    r3d.vquad(points[3], points[6], "3d_rockL");
  }

  r3d.vquad(points[0], points[7], "3d_rock");
}

void Engine::tile_sand(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 2) return;

  (void)t;
  r3d.tile(
      Coords3D{x, 0.0f, z},  // Tile position.
      Coords3D{TILE_SZ * 1.25f, 0.0f, TILE_SZ * 1.25f},  // Tile size.
      (const char*[4]){
          "3d_sandA", "3d_sandB", "3d_sandC", "3d_sandD"}[t.variant & 3]);
}

void Engine::tile_forest(int stage, float x, float z, WorldMap::Tile t) {
  (void)t;

  if (stage == 1) {
    tile_grassland(stage, x, z, WorldMap::Tile{1, uint8_t(t.variant >> 3)});
    return;
  }

  if (stage != 4) return;

  // Do not render trees at coordinates z=0 (they would block the view).
  if (z < TILE_SZ / 2.0f) {
    return;
  }

  static const struct tree_type_t {
    float w;
    float h;
    float y_offset;
    const char *texture;
  } types[] = {
    { 2.15f * 2.0f, 4.30f * 2.0f, 0.1f, "3d_pinetree1" }, // 0
    { 2.31f * 2.0f, 4.70f * 2.0f, 0.1f, "3d_pinetree2" }, // 1
    { 2.23f * 2.0f, 5.93f * 2.0f, 0.1f, "3d_pinetree3" }, // 2
    { 1.92f * 2.0f, 3.12f * 2.0f, 0.1f, "3d_pinetree4" }, // 3
    { 1.92f * 2.0f, 3.12f * 2.0f, 0.1f, "3d_pinetree5" }, // 4

    { 4.00f * 2.0f, 6.00f * 2.0f, 0.1f, "3d_tree_leaf_huge" },      // 5
    { 2.53f * 2.0f, 3.45f * 2.0f, 0.1f, "3d_tree_leaf_large" },     // 6
    { 2.89f * 2.0f, 4.00f * 2.0f, 0.1f, "3d_tree_leaf_medium" },    // 7
    { 2.50f * 2.0f, 4.10f * 2.0f, 0.1f, "3d_tree_leaf_medium2" },   // 8
    { 3.75f * 2.0f, 5.07f * 2.0f, 0.1f, "3d_tree_leaf_verylarge" }, // 9

    { 1.82f * 2.0f, 0.80f * 2.0f, 0.1f, "3d_bush1" }, // 10
    { 1.43f * 2.0f, 0.81f * 2.0f, 0.1f, "3d_bush2" }, // 11
    { 1.40f * 2.0f, 1.14f * 2.0f, 0.1f, "3d_bush3" }, // 12

    { 1.42f * 2.0f, 1.70f * 2.0f, 0.1f, "3d_deadtree1" }, // 13
    { 1.63f * 2.0f, 2.64f * 2.0f, 0.1f, "3d_deadtree2" }, // 14
    { 2.12f * 2.0f, 3.00f * 2.0f, 0.1f, "3d_deadtree3" }, // 15
  };

  static const struct tree_set_t {
    int type[10];
  } sets[] = {
    { { 0, 1, 2, 3, 4, 0, 1, 2, 3, 4 } }, // Pine trees.
    { { 5, 6, 7, 8, 9,10,11,12, 6, 7 } }, // Leaf trees.
    { {13,14,15,13,14,15,13,14,15,13 } }, // Dead trees.
    { { 0, 1, 2,13,10,11, 6, 7, 8, 9 } }, // Mix trees.
  };

  static const struct tree_slot_t {
    float x;
    float z;
  } slots[] = {  // 8 sets of 5 slots, generated with gen_slots.py
    { 0.3960f, 0.5005f }, { 0.6172f, 0.8147f }, { 0.8184f, 0.2686f }, { 0.1968f, 0.7952f }, { 0.1790f, 0.2080f },
    { 0.5286f, 0.2043f }, { 0.5618f, 0.7385f }, { 0.2434f, 0.5247f }, { 0.1775f, 0.1780f }, { 0.7988f, 0.4739f },
    { 0.6057f, 0.5127f }, { 0.2505f, 0.5869f }, { 0.2788f, 0.2036f }, { 0.7847f, 0.1907f }, { 0.7690f, 0.8245f },
    { 0.5356f, 0.5317f }, { 0.3245f, 0.1890f }, { 0.1860f, 0.6885f }, { 0.7078f, 0.1755f }, { 0.8184f, 0.7908f },
    { 0.5156f, 0.5735f }, { 0.4287f, 0.1865f }, { 0.2285f, 0.7920f }, { 0.8042f, 0.1848f }, { 0.7964f, 0.7839f },
    { 0.5789f, 0.4573f }, { 0.2397f, 0.2056f }, { 0.2280f, 0.7839f }, { 0.7776f, 0.8071f }, { 0.7905f, 0.1768f },
    { 0.4561f, 0.4377f }, { 0.7317f, 0.7502f }, { 0.2183f, 0.7646f }, { 0.7385f, 0.1870f }, { 0.1807f, 0.2031f },
    { 0.7832f, 0.2500f }, { 0.1943f, 0.4800f }, { 0.4683f, 0.7346f }, { 0.3958f, 0.1863f }, { 0.8054f, 0.6384f }
  };

  uint8_t tree_count = (t.variant & 7) % 5 + 1;
  uint8_t tree_slot = ((t.variant >> 3) & 7);
  uint8_t tree_set = ((t.variant >> 6) & 3);

  const tree_slot_t *slot_set = &slots[tree_slot * 5];
  const tree_set_t& set = sets[tree_set];
  const uint8_t set_offset = t.variant % 10;

  for (uint8_t i = 0; i < tree_count; i++) {
    const int tree_type_idx = set.type[(i + set_offset) % 10];
    const struct tree_type_t& tree = types[tree_type_idx];
    const struct tree_slot_t& slot = slot_set[i];

    const Coords3D tree_pos{
      slot.x * TILE_SZ - TILE_SZ * 0.5f,
      tree.y_offset,
      slot.z * TILE_SZ - TILE_SZ * 0.5f
    };

    /*printf("x,z=%f, %f, tree=%f, %f, slot=%f, %f\n",
            x, z, tree_pos.x, tree_pos.z, slot.x, slot.z);*/

    Coords3D bottom_left{
        tree_pos.x + x - tree.w * 0.5f,
        tree_pos.y,
        tree_pos.z + z
    };

    Coords3D top_right{
        tree_pos.x + x + tree.w * 0.5f,
        tree_pos.y - tree.h,
        tree_pos.z + z
    };

    r3d.vquad(bottom_left, top_right, tree.texture);
  }
}

void Engine::tile_rocky_road(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 2) return;

  r3d.tile(
      Coords3D{x, 0.0f, z},  // Tile position.
      Coords3D{TILE_SZ * 1.25f, 0.0f, TILE_SZ * 1.25f},  // Tile size.
      (const char*[4]){"3d_rocky_roadA", "3d_rocky_roadB",
                       "3d_rocky_roadC", "3d_rocky_roadD"}[t.variant & 3]);

  if ((t.variant & 0x80)) {
    r3d.tile(
        Coords3D{x, -3.0f, z},  // Tile position.
        Coords3D{TILE_SZ * 1.25f, -3.0f, TILE_SZ * 1.25f},  // Tile size.
        (const char*[4]){"3d_rocky_roadA", "3d_rocky_roadB",
                         "3d_rocky_roadC", "3d_rocky_roadD"}[(t.variant+1) & 3]);
  }
}

void Engine::tile_dirt_road(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 3) return;

  (void)t;
  r3d.tile(
      Coords3D{x, 0.00f, z},  // Tile position.
      Coords3D{TILE_SZ * 1.35f, 0.0f, TILE_SZ * 1.35f},  // Tile size.
      (const char*[4]){
          "3d_dirtA", "3d_dirtB", "3d_dirtC", "3d_dirtD"}[t.variant & 3]);
}

void Engine::tile_stone_wall(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 4) return;

  const Coords3D points[] = {
    // Top (clockwise).
    { x - TILE_SZ * 0.5f, -3.0f, z - TILE_SZ * 0.5f },
    { x - TILE_SZ * 0.5f, -3.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, -3.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, -3.0f, z - TILE_SZ * 0.5f },

    // Bottom (clockwise).
    { x - TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f },
    { x - TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z + TILE_SZ * 0.5f },
    { x + TILE_SZ * 0.5f, 0.0f, z - TILE_SZ * 0.5f }
  };

  r3d.vquad(points[0], points[5], t.variant == 0xff ? "3d_wall_door" : "3d_wall");
  r3d.vquad(points[3], points[6], t.variant == 0xff ? "3d_wall_door" : "3d_wall");
  r3d.vquad(points[0], points[7], t.variant == 0xff ? "3d_wall_door" : "3d_wall");
}

void Engine::tile_wood_floor(int stage, float x, float z, WorldMap::Tile t) {
  if (stage != 1) return;

  (void)t;
  r3d.tile(
      Coords3D{x, 0.0f, z},  // Tile position.
      Coords3D{TILE_SZ, 0.0f, TILE_SZ},  // Tile size.
      "3d_wood");
}

void Engine::render_mobs_at(
    GameState *state,
    int map_x, int map_y, float x, float z, bool standing_on) {
  const auto iter = state->ground_mobs.find(GameState::coords_to_key(map_x, map_y));
  if (iter == state->ground_mobs.end()) {
    return;
  }

  const auto& mobs = iter->second;
  if (mobs.empty()) {
    return;
  }

  const size_t mobs_count = mobs.size();
  float slots_width = TILE_SZ - 2.0f;
  if (mobs_count > 4) {
    slots_width = TILE_SZ - 0.5f;
  }

  float space = (slots_width) / float(mobs_count);
  float offset_x = -slots_width * 0.5f;

  r3d.itembuffer_enable = true;
  for (const auto& mob : mobs) {
    r3d.itemid = mob.id | MOB_MASK;

    float x3d = (0.0f + offset_x);
    float y3d = (-0.01f);
    float z3d = standing_on ? TILE_SZ * 0.5f - 0.3f : 0.0f;

    Canvas *c = img.get(mob.gfx_id);
    float w3d = float(c->w) * 0.015f;
    float h3d = float(c->h) * 0.015f;

    Coords3D bottom_left{
        x - w3d * 0.5f + x3d,
        y3d,
        z + z3d
    };

    Coords3D top_right{
        x + w3d * 0.5f + x3d,
        y3d - h3d,
        z + z3d
    };

    r3d.vquad(bottom_left, top_right, mob.gfx_id);

    offset_x += space;
  }

  r3d.itembuffer_enable = false;
}

void Engine::render_items_at(
    GameState *state,
    int map_x, int map_y, float x, float z, bool standing_on) {
  const auto iter = state->ground_items.find(GameState::coords_to_key(map_x, map_y));
  if (iter == state->ground_items.end()) {
    return;
  }

  const auto& items = iter->second;
  if (items.empty()) {
    return;
  }

  const size_t item_count = items.size();
  float slots_width = TILE_SZ - 3.0f;
  if (item_count > 6) {
    slots_width = TILE_SZ - 1.0f;
  }

  float space = (slots_width) / float(item_count);
  if (space > 0.3f) {
    space = 0.3f;
  }

  float offset_x = -slots_width * 0.5f;

  r3d.itembuffer_enable = true;
  for (const auto& item : items) {
    auto info = item_to_sprite_info(item.gfx_id);
    r3d.itemid = item.id;

    float x3d = (info.has_position ? info.x3d : 0.0f + offset_x);
    float y3d = (info.has_position ? info.y3d : -0.01f);
    float z3d = standing_on ? TILE_SZ * 0.5f - 0.3f : (info.has_position ? info.z3d : 0.0f);

    Coords3D bottom_left{
        x - info.w3d * 0.5f + x3d,
        y3d,
        z + z3d
    };

    Coords3D top_right{
        x + info.w3d * 0.5f + x3d,
        y3d - info.h3d,
        z + z3d
    };

    //sprintf("--> '%s' '%s'\n", item.gfx_id.c_str(), item.name.c_str());

    r3d.vquad(bottom_left, top_right, item.gfx_id);

    offset_x += space;
  }

  r3d.itembuffer_enable = false;
}

void Engine::render_at(GameState *state, int x, int y, int dir) {
  auto tm_start = clock();

  r3d.zbuffer_reset();
  r3d.zbuffer_ignore = false;

  r3d.itembuffer_reset();
  r3d.itembuffer_enable = false;
  r3d.itemid = ITEM_NON_EXISTING_ID;

  // TODO fix fog.
  if (y >= 512 && y <= 700) {
    r3d.fog_setup(true, RGBA{0, 0, 0, 0}, 5.0f);
    std::fill(c.d.begin(), c.d.end(), RGBA{0, 0, 0, 255});
  } else {
    r3d.fog_setup(true, RGBA{128, 168, 255, 255}, 1.0f);
    c.copy_fast(img.get("3d_sky"));
  }

  Coords iter_external;
  Coords iter_internal;

  if (dir == NORTH) {
    iter_external.x = 0; iter_external.y = 1;
    iter_internal.x = 1; iter_internal.y = 0;
  } else if (dir == SOUTH) {
    iter_external.x = 0; iter_external.y = -1;
    iter_internal.x = -1; iter_internal.y = 0;
  } else if (dir == WEST) {
    iter_external.x = 1; iter_external.y = 0;
    iter_internal.x = 0; iter_internal.y = -1;
  } else if (dir == EAST) {
    iter_external.x = -1; iter_external.y = 0;
    iter_internal.x = 0; iter_internal.y = 1;
  } else {
    puts("warning: wrong direction?");
    return;
  }

  // TODO: Perhaps do a display list out of this stuff? Might actually be faster.

  for (int stage = 0; stage < 5; stage++) {

    r3d.zbuffer_ignore = (stage < 4);

    // The last stage needs to be drawn in reverse order to use the Z-buffer
    // in the fullest.
    const int j_start = (stage < 4) ? VIEWING_DISTANCE : 0;
    const int j_end = (stage < 4) ? -1 : VIEWING_DISTANCE + 1;
    const int j_vector = (stage < 4) ? -1 : +1;

    for (int j = j_start; j != j_end; j += j_vector) {

      // Calculate the view cone.
      const int bx = (int)(((float)j * 3.0f + 3.0f) / 2.0f);

      for (int i = -bx; i <= bx; i++) {
        const float x3D = (float)i * TILE_SZ;
        const float z3D = (float)j * TILE_SZ - PLAYER_Z;

        int map_x = x + iter_internal.x * i - iter_external.x * j;
        int map_y = y + iter_internal.y * i - iter_external.y * j;

        uint8_t default_tile_type = (stage == 0) ? 2 /* water */ : 0 /* nothing */;
        WorldMap::Tile t{default_tile_type, 0};
        if (map_x >= 0 && map_x < WORLD_W &&
            map_y >= 0 && map_y < WORLD_H) {
          const size_t idx = map_x + map_y * WORLD_W;
          t = world.tiles[idx];
        }

        switch (t.type) {
          case 0: break;  // Nothing.
          case 1: tile_grassland(stage, x3D, z3D, t); break;
          case 2: tile_water(stage, x3D, z3D, t); break;
          case 3: tile_mountains(stage, x3D, z3D, t); break;
          case 4: tile_sand(stage, x3D, z3D, t); break;
          case 5: tile_forest(stage, x3D, z3D, t); break;
          case 6: {
            t.variant &= 0x7f;
            if (map_y >= 512) {
              t.variant |= 0x80;
            }
            tile_rocky_road(stage, x3D, z3D, t);
          } break;
          case 7: tile_dirt_road(stage, x3D, z3D, t); break;
          case 8: {
            auto pos = GameState::coords_to_key(map_x, map_y);
            t.variant = 0;
            if (state->ground_items.find(pos) != state->ground_items.end()) {
              t.variant = 0xff;
            }

            tile_stone_wall(stage, x3D, z3D, t);
          } break;
          case 9: tile_wood_floor(stage, x3D, z3D, t); break;
        }

        // At the end do show items, but only nearby.
        if (stage == 4 && j < VIEWING_DISTANCE_ITEMS) {
          render_mobs_at(state, map_x, map_y, x3D, z3D, i == 0 && j == 0);
          render_items_at(state, map_x, map_y, x3D, z3D, i == 0 && j == 0);
        }
      }
    }
  }

  auto tm_end = clock();

  auto tm_diff = tm_end - tm_start;
  float spf = float(tm_diff) / float(CLOCKS_PER_SEC);
  float fps = 1.0f / spf;
  (void)fps;
  //printf("%f sec (%.1f FPS)\n", spf, fps);

  /*
  for (int i = 0; i < WIDTH_3D * HEIGHT_3D; i++) {
    memcpy(&c.d[i], &r3d.itembuffer[i], 4);
    c.d[i].a = 255;
  }
  */
}


void Engine::map_at(int x, int y, int dir) {

  // Cache this.
  c.blit(img.get("ui_map_bg"), WIDTH_UI - 60, 0);

  // TODO: Don't redraw map if game state didn't change too much.
  auto art_icons = img.get("ui_map_icons");
  auto art_dir = img.get("ui_map_dir");

  map_c.reset();

  for (int j = -4; j <= 4; j++) {
    for (int i = -4; i <= 4; i++) {

      int map_x = x + i;
      int map_y = y + j;

      WorldMap::Tile t{2, 0};
      if (map_x >= 0 && map_x < WORLD_W &&
          map_y >= 0 && map_y < WORLD_H) {
        const size_t idx = map_x + map_y * WORLD_W;
        t = world.tiles[idx];
      }

      map_c.copy(art_icons,
             t.type * 5, 0,
             29 + i * 5, 26 + j * 5,
             5, 5);
    }
  }

  // Do a nice anti-aliasing border.
  for (size_t i = 0; i < map_c.w; i++) {
    map_c.d[i + 6 * map_c.w].a /= 8;
    map_c.d[i + 7 * map_c.w].a /= 4;
    map_c.d[i + 8 * map_c.w].a /= 2;

    map_c.d[9 + i * map_c.w].a /= 8;
    map_c.d[10 + i * map_c.w].a /= 4;
    map_c.d[11 + i * map_c.w].a /= 2;

    map_c.d[51 + i * map_c.w].a /= 2;
    map_c.d[52 + i * map_c.w].a /= 4;
    map_c.d[53 + i * map_c.w].a /= 8;

    map_c.d[i + 48 * map_c.w].a /= 2;
    map_c.d[i + 49 * map_c.w].a /= 4;
    map_c.d[i + 50 * map_c.w].a /= 8;
  }

  c.blit(&map_c, WIDTH_UI - 60, 0);

  switch (dir) {
    case NORTH:
      c.blit(art_dir, 0 * 11, 0, WIDTH_UI - 60 + 30 - 4, 1, 11, 11);
      break;

    case SOUTH:
      c.blit(art_dir, 1 * 11, 0, WIDTH_UI - 60 + 30 - 4, 60 - 11 - 4, 11, 11);
      break;

    case WEST:
      c.blit(art_dir, 2 * 11, 0, WIDTH_UI - 60 + 4, 30 - 7, 11, 11);
      break;

    case EAST:
      c.blit(art_dir, 3 * 11, 0, WIDTH_UI - 11 - 1, 30 - 7, 11, 11);
      break;
  }


  c.blit(art_dir, 4 * 11, 0, WIDTH_UI - 60 + 30 - 4, 30 - 7, 11, 11);
}

void Engine::blit_item(const std::string& name, Canvas *dst, int x, int y) {
  /* Given that we have carve out sprites and put the result in "global" texture
     namespace this code is no longer needed, but keeping it here for "just in
     case" reasons.
  auto item = item_to_sprite_info(name);  // Always returns something.
  auto src = img.get(item.img);
  if (item.is_sprite) {
    dst->blit(src, item.x, item.y, x, y, item.w, item.h);
  } else {
    dst->blit(src, x, y);
  }*/
  dst->blit(img.get(name), x, y);
}

void Engine::draw_ui(GameState *state) {
  // Handle mouse events and tooltips.
  std::string tooltip;
  int px = 0, py = 0; // Position of the top middle of what the mouse is pointing at.
  bool lclick = !state->mleft && state->mleft_previous &&
                mouse_close(state->mx, state->my,
                state->mx_down_left, state->my_down_left);
  bool rclick = !state->mright && state->mright_previous &&
                mouse_close(state->mx, state->my,
                state->mx_down_right, state->my_down_right);
  bool start_holding = !state->mleft_previous && state->mleft &&
                       state->holding.id == ITEM_NON_EXISTING_ID &&
                       !state->selecting;
  bool stop_holding = state->holding.id != ITEM_NON_EXISTING_ID &&
                      state->mleft_previous && !state->mleft;
  bool selected = lclick && state->selecting;

  if (selected) {
    // Override this to make sure no hold happens by accident.
    start_holding = false;
  }

  if (stop_holding) {
    state->wants_to_drop_item = true;
    state->item_drop_dst = 255;  // Ground by default.
  }

  std::string button_inv_spell("ui_button_off");
  std::string button_inv_spell_icon(
      (state->game_hud == GameState::HUD_SPELL) ?
      "ui_icon_inv_idle" : "ui_icon_magic_idle");
  std::string button_spell_cast("ui_button_off");
  std::string button_spell_cast_icon("ui_icon_cast_idle");

  // Properly this should be done using a quad tree or sth, but actually
  // there are only a couple of things too check, so...
  if (mouse_is_over(state, 12, 208, 24, 24)) {
    // Toggle Inventory / Spellcasting.
    px = 12 + 24 / 2; py = 208;
    tooltip = (state->game_hud == GameState::HUD_SPELL) ?
       "Spellcasting (TAB)" : "Inventory (TAB)";

    if (lclick) {
      if (state->game_hud == GameState::HUD_SPELL) {
        state->game_hud = GameState::HUD_INVENTORY;
      } else {
        state->game_hud = GameState::HUD_SPELL;
      }
    }

    if (state->mleft) {
      button_inv_spell = "ui_button_on";
      button_inv_spell_icon = (state->game_hud == GameState::HUD_SPELL) ?
          "ui_icon_inv_pressed" : "ui_icon_magic_pressed";
    } else {
      button_inv_spell_icon = (state->game_hud == GameState::HUD_SPELL) ?
          "ui_icon_inv_hover" : "ui_icon_magic_hover";
    }
  }

  else if (state->game_hud == GameState::HUD_SPELL &&
           mouse_is_over(state, 392, 208, 24, 24)) {
    // Fire a spell.
    px = 392 + 24 / 2; py = 208;
    tooltip = "Cast Spell (SPACE)";

    if (lclick) {
      state->clicked_spell_cast = true;
    }

    if (state->mleft) {
      button_spell_cast = "ui_button_on";
      button_spell_cast_icon = "ui_icon_cast_pressed";
    } else {
      button_spell_cast_icon = "ui_icon_cast_hover";
    }
  }

  else if (state->game_hud == GameState::HUD_SPELL &&
           lclick &&
           mouse_is_over(state, (428 - 192) / 2, 204 - 1, 192, 9 * 4) &&
           state->spell_length < 8) {
    // Type in spell.
    int rune_x = std::clamp((state->mx - ((428 - 192) / 2)) / 12, 0, 15);
    int rune_y = std::clamp((state->my - (204 - 1)) / 9, 0, 3);
    uint8_t rune = 0x40 + rune_x + rune_y * 0x10;
    state->spell[state->spell_length++] = rune;
  }

  else if (mouse_is_over(state, 5, 181, 90, 14)) {
    // HP bar.
    px = 5 + 90 / 2; py = 181;
    char buf[64];
    sprintf(buf, "HP: %i / %i", state->player_hp, state->player_hp_max);
    tooltip = buf;
  }

  else if (mouse_is_over(state, 333, 181, 90, 14)) {
    // Mana bar.
    px = 333 + 90 / 2; py = 181;
    char buf[64];
    sprintf(buf, "Mana: %i / %i", state->player_mana, state->player_mana_max);
    tooltip = buf;
  }

  else if (state->game_hud == GameState::HUD_INVENTORY &&
           mouse_is_over(state, 93, 203, 330, 32)) {
    // Mouse over inventory slots.
    py = 208;

    if (mouse_is_over(state, 96, 208, 24, 24)) {
      // Left hand.
      px = 96 + 24 / 2;
      if (state->equiped[HAND_LEFT].id == ITEM_NON_EXISTING_ID) {
        tooltip = "Empty hand (left)";

        if (selected) {
          state->wants_to_select = true;
          state->selection_target_type = 3; // Empty slot.
          state->selection_target = 8;
        }
      } else {
        tooltip = state->equiped[HAND_LEFT].name;

        if (rclick) {
          state->wants_to_use_item = state->equiped[HAND_LEFT].id;
        }

        if (start_holding) {
          state->wants_to_hold_item = state->equiped[HAND_LEFT].id;
        }

        if (selected) {
          state->wants_to_select = true;
          state->selection_target_type = 0; // Item.
          state->selection_target = state->equiped[HAND_LEFT].id;
        }
      }

      if (stop_holding) {
        state->item_drop_dst = 8;
      }
    } else if (mouse_is_over(state, 128, 208, 24, 24)) {
      // Right hand.
      px = 128 + 24 / 2;
      if (state->equiped[HAND_RIGHT].id == ITEM_NON_EXISTING_ID) {
        tooltip = "Empty hand (right)";

        if (selected) {
          state->wants_to_select = true;
          state->selection_target_type = 3; // Empty slot.
          state->selection_target = 9;
        }
      } else {
        tooltip = state->equiped[HAND_RIGHT].name;

        if (rclick) {
          state->wants_to_use_item = state->equiped[HAND_RIGHT].id;
        }

        if (start_holding) {
          state->wants_to_hold_item = state->equiped[HAND_RIGHT].id;
        }

        if (selected) {
          state->wants_to_select = true;
          state->selection_target_type = 0; // Item.
          state->selection_target = state->equiped[HAND_RIGHT].id;
        }
      }

      if (stop_holding) {
        state->item_drop_dst = 9;
      }
    } else {
      // Inventory slots.
      for (int i = 0; i < 8; i++) {
        if (mouse_is_over(state, 168 + 32 * i, 208, 24, 24)) {
          px = 168 + 32 * i + 24 / 2;
          if (state->inventory[i].id == ITEM_NON_EXISTING_ID) {
            tooltip = "Empty slot";

            if (selected) {
              state->wants_to_select = true;
              state->selection_target_type = 3; // Empty slot.
              state->selection_target = uint64_t(i);
            }

          } else {
            tooltip = state->inventory[i].name;

            if (rclick) {
              state->wants_to_use_item = state->inventory[i].id;
            }

            if (start_holding) {
              state->wants_to_hold_item = state->inventory[i].id;
            }

            if (selected) {
              state->wants_to_select = true;
              state->selection_target_type = 0; // Item.
              state->selection_target = state->inventory[i].id;
            }
          }

          if (stop_holding) {
            state->item_drop_dst = i;
          }

          break;
        }
      }
    }
  }

  else if (mouse_is_over(state, 0, 0, WIDTH_3D, HEIGHT_3D - SCENE_3D_OFFSET_Y)) {
    // 3D world view.
    int idx = state->mx + (state->my + SCENE_3D_OFFSET_Y) * WIDTH_3D;
    uint64_t itemid = r3d.itembuffer[idx];
    if (itemid != ITEM_NON_EXISTING_ID) {
      px = state->mx;
      py = state->my;

      if ((itemid & MOB_MASK) == 0) {
        // Item.
        const auto iter = state->item_id_to_item.find(itemid);

        if (iter == state->item_id_to_item.end()) {
          tooltip = "Unknown";
        } else {
          tooltip = iter->second.name;
        }

        if (rclick) {
          state->wants_to_use_item = itemid;
        }

        if (start_holding) {
          state->wants_to_hold_item = itemid;
        }

        if (selected) {
          state->wants_to_select = true;
          state->selection_target_type = 0; // Item.
          state->selection_target = itemid;
        }
      } else {
        // Mob.
        uint64_t realid = itemid ^ MOB_MASK;

        const auto iter = state->mob_id_to_mob.find(realid);
        if (iter == state->mob_id_to_mob.end()) {
          tooltip = "Unknown";
        } else {
          tooltip = iter->second.name;
        }

        if (selected) {
          state->wants_to_select = true;
          state->selection_target_type = 1; // Mob.
          state->selection_target = realid;
        }
      }

    } else {
      if (selected) {
        state->wants_to_select = true;
        state->selection_target_type = 2; // Ground.
      }
    }
  }

  // Actual drawing starts here.
  c.blit(img.get("ui_border_B"), 0, 0);

  int hp = std::clamp(state->player_hp, 0, state->player_hp_max);
  int mana = std::clamp(state->player_mana, 0, state->player_mana_max);

  int hp_px = (hp * 84) / state->player_hp_max; // 0 to 84 (inclusive)
  int mana_px = (mana * 84) / state->player_mana_max; // 0 to 84 (inclusive)
  c.blit(img.get("ui_bar_health_bg"), 0, 0, 8, 183, 84, 10);
  c.blit(img.get("ui_bar_health"),
         84 - hp_px, 0, 8 + 84 - hp_px, 183, hp_px, 10);

  c.blit(img.get("ui_bar_mana_bg"), 0, 0, 336, 183, 84, 10);  // 84 max
  c.blit(img.get("ui_bar_mana"), 0, 0, 336, 183, mana_px, 10);  // 84 max

  //state->game_hud = GameState::HUD_SPELL;
  if (state->game_hud == GameState::HUD_INVENTORY) {
    auto hud = img.get("ui_hud_inv");
    c.blit(hud, 0, 240 - hud->h);  // TODO agressive cut hud.
    c.blit(img.get("ui_hand_left"), 96, 208);
    c.blit(img.get("ui_hand_right"), 128, 208);

    c.blit(img.get(state->player_gender ? "ui_portrait_m" : "ui_portrait_f"),
           48, 204);

    if (state->equiped[HAND_LEFT].id != ITEM_NON_EXISTING_ID) {
      blit_item(state->equiped[HAND_LEFT].gfx_id, &c, 96 + 4, 208 + 4);
    }

    if (state->equiped[HAND_RIGHT].id != ITEM_NON_EXISTING_ID) {
      blit_item(state->equiped[HAND_RIGHT].gfx_id, &c, 128 + 4, 208 + 4);
    }

    for (int i = 0; i < 8; i++) {
      if (state->inventory[i].id == ITEM_NON_EXISTING_ID) {
        continue;
      }

      blit_item(state->inventory[i].gfx_id, &c, 172 + 32 * i, 208 + 4);
    }

    c.blit(img.get(button_inv_spell), 12, 208);
    c.blit(img.get(button_inv_spell_icon), 12, 208);

  } else if (state->game_hud == GameState::HUD_SPELL) {
    auto hud = img.get("ui_hud_spell");
    c.blit(hud, 0, 240 - hud->h);  // TODO agressive cut hud.

    for (int i = 0; i < state->spell_length; i++) {
      uint8_t buf[3] = { 0xff, state->spell[i], 0 };
      txt->render(&c, 151 + 16 * i, 187, 15, 15, (char*)buf);
    }

    // This is somewhat funny, but probably it's the simplest way to do it.
    txt->render(&c, (428 - 192) / 2 - 1, 204 - 2, 192 + 2 + 12, 10,
        "\xff\x40\xff\x41\xff\x42\xff\x43\xff\x44\xff\x45\xff\x46\xff\x47"
        "\xff\x48\xff\x49\xff\x4a\xff\x4b\xff\x4c\xff\x4d\xff\x4e\xff\x4f");

    txt->render(&c, (428 - 192) / 2 - 1, 204 - 2 + 9, 192 + 2 + 12, 10,
        "\xff\x50\xff\x51\xff\x52\xff\x53\xff\x54\xff\x55\xff\x56\xff\x57"
        "\xff\x58\xff\x59\xff\x5a\xff\x5b\xff\x5c\xff\x5d\xff\x5e\xff\x5f");

    txt->render(&c, (428 - 192) / 2 - 1, 204 - 2 + 18, 192 + 2 + 12, 10,
        "\xff\x60\xff\x61\xff\x62\xff\x63\xff\x64\xff\x65\xff\x66\xff\x67"
        "\xff\x68\xff\x69\xff\x6a\xff\x6b\xff\x6c\xff\x6d\xff\x6e\xff\x6f");

    txt->render(&c, (428 - 192) / 2 - 1, 204 - 2 + 27, 192 + 2 + 12, 10,
        "\xff\x70\xff\x71\xff\x72\xff\x73\xff\x74\xff\x75\xff\x76\xff\x77"
        "\xff\x78\xff\x79\xff\x7a\xff\x7b\xff\x7c\xff\x7d\xff\x7e\xff\x7f");

    c.blit(img.get(button_inv_spell), 12, 208);
    c.blit(img.get(button_inv_spell_icon), 12, 208);
    c.blit(img.get(button_spell_cast), 392, 208);
    c.blit(img.get(button_spell_cast_icon), 392, 208);
  }

  in_game_text.render_to(&c, 10, 10);
  c.blit(img.get("ui_dir_frame"), 172, 0);

  auto dir = img.get(
      state->player_dir == NORTH ? "ui_dir_N" :
      state->player_dir == SOUTH ? "ui_dir_S" :
      state->player_dir == WEST  ? "ui_dir_W" :
                                   "ui_dir_E");
  c.blit(dir, (428 - 32) / 2, 4);

  map_at(state->player_x, state->player_y, state->player_dir);

  // Render the tooltip.
  if (!tooltip.empty()) {
    // Assume only one line of tooltip and no colors.
    if (tooltip.size() > 40) {
      tooltip.resize(40);
      tooltip += "...";
    }

    int w = int(tooltip.size()) * 6 + 2;
    int x = px - w / 2;
    if (x + w >= WIDTH_UI) {
      x = WIDTH_UI - w;  // Will not go negative.
    } else if (x < 0) {
      x = 0;
    }

    int y = py - 14;
    if (y < 0) {
      y = 0;
    }

    tooltip = "\x13" + tooltip;

    txt->render(&c, x, y, w, 10, tooltip);
  }

}

bool Engine::mouse_is_over(GameState *state, int x, int y, int w, int h) {
  return (state->mx >= x && state->mx < x + w) &&
         (state->my >= y && state->my < y + h);
}

void Engine::draw_console(GameState * /*state*/) {
  c.copy_fast(img.get("con_bg"));
  debug_con.render_to(&c, 10, 10);
  c.blit(img.get("ui_border_A"), 0, 0);
}

void Engine::render_frame(GameState *state) {
  txt->hint_frame_change();

  if (state->game_scene == GameState::SCENE_SPLASHSCREEN) {
    // TODO: hook up the splash screen while the game is connecting...
  }

  if (state->game_scene == GameState::SCENE_GAME) {
    // Render the 3D scene first.
    render_at(state, state->player_x, state->player_y, state->player_dir);

    // Render the UI on top of the 3D scene.
    draw_ui(state);
  }

  // Render the console on top of it all if needed.
  if (state->game_scene == GameState::SCENE_CONSOLE) {
    draw_console(state);
  }

  // Update mouse states.
  state->mleft_previous = state->mleft;
  state->mright_previous = state->mright;
}

