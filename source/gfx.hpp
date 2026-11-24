#pragma once
#include <string>
#include <cstdint>

struct Color { uint8_t r, g, b, a; };

namespace gfx {

enum Align { Left = 0, Center = 1, Right = 2 };

bool init();
void exit();
void clear(Color c);
void present();

void rect(int x, int y, int w, int h, Color c);
void roundRect(int x, int y, int w, int h, int r, Color c);
void clip(int x, int y, int w, int h);
void noClip();

// Texto con la fuente del sistema. maxW > 0 recorta con "…". Devuelve el ancho dibujado.
int text(const std::string& s, int x, int y, int size, Color c, int align = Left, int maxW = 0);
int measure(const std::string& s, int size);
// Iconos de botones (fuente NintendoExt), p. ej. GLYPH_A.
int glyph(const char* utf8, int x, int y, int size, Color c);
int glyphWidth(const char* utf8, int size);
int wrappedHeight(const std::string& s, int size, Color c, int wrapW);
int textWrapped(const std::string& s, int x, int y, int size, Color c, int wrapW);

} // namespace gfx

#define GLYPH_A "\xEE\x83\xA0"
#define GLYPH_B "\xEE\x83\xA1"
#define GLYPH_X "\xEE\x83\xA2"
#define GLYPH_Y "\xEE\x83\xA3"
#define GLYPH_L "\xEE\x83\xA4"
#define GLYPH_R "\xEE\x83\xA5"
#define GLYPH_PLUS "\xEE\x83\xAF"
