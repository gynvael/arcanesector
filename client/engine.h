#pragma once
#include <algorithm>
#include <stdint.h>
#include <vector>
#include <string>
#include <utility>
#include <SDL2/SDL_image.h>
#include <unordered_map>
#include <memory>
#include <limits>
#include <chrono>

#include "world_map.h"
#include "gamestate.h"

// Output frame has exactly 428 x 240 x 32bpp, R, G, B, A byte order.
// Canvases have arbitrary size, but remain 32bpp, R, G, B, A byte order.

struct RGBA {
  uint8_t r, g, b, a;
};

struct Coords {
  int x, y;
};

struct Coordsf {
  float x, y;
};

struct Coords3D {
  float x, y, z;
};

const float PERSPECTIVE_CORRECTION = 0.25f;
const float TILE_SZ = 5.0f;
const float PLAYER_Z = -0.5f;
const float EYE_LEVEL = -1.8;// -1.8f;

// Width/Height of the UI canvas.
const int WIDTH_UI = 428;
const int HEIGHT_UI = 240;

const int SCENE_3D_OFFSET_Y = 30;

// Width/Height of the canvas for the 3D engine.
const int WIDTH_3D = 428;//356;
const int HEIGHT_3D = 240;

const uint8_t PIXEL_MASK = 0xff;

const float WIDTH_3DF = (float)WIDTH_3D;
const float HEIGHT_3DF = (float)HEIGHT_3D;

const float SCALE_3D = WIDTH_3DF / 10.0f;

const int VIEWING_DISTANCE = 25;
const int VIEWING_DISTANCE_ITEMS = 4;

const float FOG_NONE_DISTANCE = 2.0f * TILE_SZ;
const float FOG_MAX_DISTANCE = ((float)VIEWING_DISTANCE * 0.80f) * TILE_SZ;

const int DEFAULT_TEXT_COLOR = 7;  // Gray.
const int FONT_W = 6;  // simple_6x8.png
const int FONT_H = 8;  // simple_6x8.png

// A 2D canvas with some software 3D capabilities.
class Canvas {
 public:
  unsigned int w, h;
  std::vector<RGBA> d;

  // From scratch.
  Canvas(unsigned int width, unsigned int height)
      : w{width}, h{height}, d{w * h}, texel_cache{w * h} {}

  // From SDL surface.
  Canvas(SDL_Surface *s);

  // To SDL surface.
  SDL_Surface* to_surface() const;

  // Drawing functions.
  void line(Coords s, Coords e, RGBA color);

  // Copies bytes from canvas of identical width.
  void copy_fast(Canvas *src);
  void copy_fast(Canvas *src, int dst_y);

  void copy(Canvas *src,
            int src_x, int src_y,
            int dst_x, int dst_y,
            int src_w, int src_h);

  void blit(Canvas *src,
            int src_x, int src_y,
            int dst_x, int dst_y,
            int src_w, int src_h);

  void blit(Canvas *src, int dst_x, int dst_y);

  void reset();

  // For textures.
  RGBA get_texel(Coordsf p, Coordsf sz);  // Pixel coords and size.
  std::vector<std::unordered_map<uint32_t, RGBA>> texel_cache;  // Mipmap cheat.

  // For debugging.
  void dump(const std::string& fname);  // Will write PNG
};

class TextRenderer {
 public:
  TextRenderer(Canvas *font, Canvas *rune_bg, Canvas *runes)
      : font_(font), rune_bg_(rune_bg), runes_(runes) { }

  void hint_frame_change();

  // These return the number of lines rendered. If the number is negative, it
  // means the canvas was too small to render all the lines (absolute value is
  // still the number of lines rendered though).
  int printf(Canvas *dst, int x, int y, int w, int h, const char *fmt, ...);
  int render(Canvas *dst, int x, int y, int w, int h, const std::string& text);

 private:
  struct CachedText {
    int lines_rendered;  // Can be negative.
    Canvas *text;
  };

  Canvas *render_worker(const std::string& text, int w, int h, int *lines_rendered);
  void render_char(Canvas *dst, int x, int y, unsigned char ch, RGBA color);
  void render_rune(Canvas *dst, int x, int y, unsigned char rune);
  void render_outline(Canvas *dst, int x, int y, int w, int h, RGBA outline);

  std::string cache_key(const std::string& text, int w, int h);
  CachedText *cache_get(const std::string& key);
  void cache_store(const std::string& key, CachedText *rendered_text);
  // Note: Cache takes ownership of the rendered_text Canvas.

  // TextRenderer is NOT the owner of these canvases.
  Canvas *font_ = nullptr;
  Canvas *rune_bg_ = nullptr;
  Canvas *runes_ = nullptr;

  // TextRenderer is the owner of the cached rendered canvases.
  // Note: the key is a concatenation of uint16_t width and height of the text
  // field, and the actual text.
  std::unordered_map<std::string, CachedText*> cache[2];
  int cache_current = 0;

};

class ImageManager {
 public:
  // ImageManager is the owner of all loaded images.
  bool load(const std::string& id,
            const std::string& fname);
  void add(const std::string& id,
           Canvas *c);  // ImageManager takes ownership of *c.

  Canvas* get(std::string id);

 private:
  std::unordered_map<std::string, std::unique_ptr<Canvas>> images;
};

class Render3D {
 public:
  Render3D(Canvas *canvas, ImageManager *images)
      : c{canvas}, img{images}, fog_enable(false) {
        zbuffer.resize(c->w * c->h);
        zbuffer_reset();
        itembuffer.resize(c->w * c->h);
        itembuffer_reset();
      }

  Coords point3D_to_2D(Coords3D p);

  // Returns fixed_z.
  float first_z_on_screen(float z, float y);

  float y_scanline_to_z(int y_scanline, float y);
  float x_scanline_to_z(int x_scanline, Coords3D near, Coords3D far);

  // Two most important figures in the game are:
  // vquad - an Y-axis aligned textured vertical quad.
  // tile - an axis-aligned textured floor tile.
  // In both cases I'm cheating here and there to ease up the math for
  // texturing etc, i.e. in vquad the scanlines are calculated vertically
  // as it's super simple to do, and in tile they are calculated horizontally.

  // "s" is supposed to be bottom left corner of the quad, and "e" is
  // the top right corner (a little weird, but it makes most sense to me.
  void vquad(Coords3D s, Coords3D e, std::string texture_id);

  // "m" is middle of the tile, and sz is it's size (y coord is ignored).
  void tile(Coords3D m, Coords3D sz, std::string texture_id);

  // Place a pixel with alpha, z-buffer.
  inline RGBA merge_colors(RGBA a, RGBA b) {
    return RGBA{
      (uint8_t)(((uint32_t)a.r * a.a + (uint32_t)b.r * (255 - a.a)) >> 8),
      (uint8_t)(((uint32_t)a.g * a.a + (uint32_t)b.g * (255 - a.a)) >> 8),
      (uint8_t)(((uint32_t)a.b * a.a + (uint32_t)b.b * (255 - a.a)) >> 8),
      255
    };
  }

  inline uint8_t mask_pixel(uint32_t px) {
    return (uint8_t)px & (uint8_t)PIXEL_MASK;
  }

  inline void pixel3D(Coords p, float z, RGBA color) {
    if (p.x < 0 || p.y < 0 || p.x >= (int)c->w || p.y >= (int)c->h) {
      return;
    }

    const size_t idx = p.x + p.y * c->w;

    if (color.a == 0) {
      return;  // Would not be visible anyway.
    }

    if (!zbuffer_ignore && z >= zbuffer[idx]) {
      return;
    }

    zbuffer[idx] = z;
    if (itembuffer_enable) {
      itembuffer[idx] = itemid;
    }

    RGBA final = color.a == 255 ? color : merge_colors(color, c->d[idx]);
    if (this->fog_enable) {
      final = merge_colors(fog_get_color(z), final);
    }

    // Y offset.
    const int offset_y = p.y - SCENE_3D_OFFSET_Y;
    if (offset_y < 0) {
      return;
    }
    const size_t offset_idx = p.x + offset_y * c->w;

    c->d[offset_idx] = RGBA{
        mask_pixel(final.r),
        mask_pixel(final.g),
        mask_pixel(final.b),
        final.a
    };
  }

  void line3D(Coords3D s, Coords3D e, RGBA color);
  void helper_3D_block(float x, float z);
  void helper_3D_floor(float x, float z);

  // Fog functions.
  void fog_setup(bool enable, RGBA color, float intensity);
  RGBA fog_get_color(float z);

  // Helper routines for Z-buffer.
  inline void zbuffer_reset() {
    std::fill(this->zbuffer.begin(), this->zbuffer.end(),
              std::numeric_limits<float>::infinity());
  }

  Canvas *c;  // Render3D is not the owner of this object.
              // The canvas MUST be WIDTH_3D x HEIGHT_3D size.
  std::vector<float> zbuffer;  // Z-buffer used here and there.
  bool zbuffer_ignore;  // If true, the Z-buffer will be filled, but ignored
                        // while drawing (useful for transparency).

  std::vector<uint64_t> itembuffer;  // Item IDs per pixel.
  inline void itembuffer_reset() {
    std::fill(this->itembuffer.begin(), this->itembuffer.end(),
              ITEM_NON_EXISTING_ID);
  }

  bool itembuffer_enable{false};  // If true, pixel3D paints with itemid.
  uint64_t itemid;  // Item "color".

  ImageManager *img;

  bool fog_enable;
  RGBA fog_color;
  float fog_intensity;
};

// In game console (i.e. text field + input box).
class Console {
 public:
  Console(unsigned int w, unsigned int h, float fadeout) :
    line_time(size_t(h), std::numeric_limits<float>::max()),
    fadeout_time{fadeout},
    c{w, h}, tmp{w, h},
    time_start{std::chrono::steady_clock::now()} { }

  void set_text_renderer(TextRenderer *txt);

  void render_to(Canvas *dst, int x, int y);
  void puts(const std::string& s);

  void set_prompt(const std::string& new_prompt, bool show);
  std::string& input_text();  // Caller can directly change input text.

 private:
  std::vector<float> line_time;
  float fadeout_time;
  Canvas c;
  Canvas tmp;

  std::string prompt;
  bool show_input_prompt = false;
  std::string input_text_;

  std::chrono::time_point<std::chrono::steady_clock> time_start;
  TextRenderer *txt;  // Console is NOT the owner of this.
};

class Engine {
 public:
  Engine()
      : c{WIDTH_UI, HEIGHT_UI},
        r3d{&c, &img},
        in_game_text{WIDTH_UI - 20, HEIGHT_UI - 75, 10.0f},
        debug_con{WIDTH_UI - 20, HEIGHT_UI - 20, -1.0f},
        map_c{60, 60} {
    IMG_Init(IMG_INIT_PNG);
  };

  ~Engine() {
    IMG_Quit();
  };

  bool initialize();
  void render_frame(GameState *state);

  void blit_item(const std::string& name, Canvas *dst, int x, int y);

  ImageManager img;
  WorldMap world;
  Canvas c;
  Render3D r3d;
  Console in_game_text;
  Console debug_con;
  std::unique_ptr<TextRenderer> txt;

 private:
  void render_at(GameState *state, int x, int y, int dir);
  void render_items_at(
      GameState *state,
      int map_x, int map_y,
      float x, float z, bool standing_on);
  void render_mobs_at(
      GameState *state,
      int map_x, int map_y,
      float x, float z, bool standing_on);
  void draw_ui(GameState *state);
  void draw_console(GameState *state);
  void map_at(int x, int y, int dir);

  bool load_textures();
  void pregenerate_texture(const std::string& gfx_id);

  bool mouse_is_over(GameState *state, int x, int y, int w, int h);
  inline bool mouse_close(int x1, int y1, int x2, int y2) {
    int dx = x1 - x2;
    int dy = y1 - y2;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy < 8;
  }

  void tile_grassland(int stage, float x, float z, WorldMap::Tile t);
  void tile_water(int stage, float x, float z, WorldMap::Tile t);
  void tile_mountains(int stage, float x, float z, WorldMap::Tile t);
  void tile_sand(int stage, float x, float z, WorldMap::Tile t);
  void tile_forest(int stage, float x, float z, WorldMap::Tile t);
  void tile_rocky_road(int stage, float x, float z, WorldMap::Tile t);
  void tile_dirt_road(int stage, float x, float z, WorldMap::Tile t);
  void tile_stone_wall(int stage, float x, float z, WorldMap::Tile t);
  void tile_wood_floor(int stage, float x, float z, WorldMap::Tile t);

  Canvas map_c;
};


