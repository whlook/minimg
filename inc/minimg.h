#ifndef _MINIMG_H_
#define _MINIMG_H_

#include <vector>
#include <string>

#define MAKE_COLOR(b, g, r) (((b) << 16) | ((g) << 8) | r)

/// @brief pixel format definition
typedef enum min_pixel_format {
  MIN_PIX_FMT_GRAY8,          ///< Y    1       8bpp
  MIN_PIX_FMT_YUV420P,        ///< YUV  4:2:0   12bpp
  MIN_PIX_FMT_NV12,           ///< YUV  4:2:0   12bpp
  MIN_PIX_FMT_NV21,           ///< YUV  4:2:0   12bpp
  MIN_PIX_FMT_BGRA8888,       ///< BGRA 8:8:8:8 32bpp
  MIN_PIX_FMT_BGR888,         ///< BGR  8:8:8   24bpp
  MIN_PIX_FMT_NONE
} min_pixel_format_e;

/// @brief timestamp definition
typedef struct min_time_t {
  long int tv_sec;
  long int tv_usec;
} min_time_t;

/// @brief image definition
typedef struct min_image {
  unsigned char *data;
  int width;
  int height;
  int stride;
  min_pixel_format_e pixel_format;
  min_time_t time_stamp;
} min_image_t;

/// @brief rect definition
typedef struct min_rect {
	int left;
	int top;
	int right;
	int bottom;
}min_rect_t;

/// @brief point definition
typedef struct min_pointf {
        float x;
        float y;
} min_pointf_t;

int min_image_read(const char *path, min_image_t **image);
int min_image_write(const char *path, min_image_t *image);
int min_image_allocate(int w, int h, int s, min_pixel_format_e p, min_image_t **img);
int min_image_release(min_image_t *image);
int min_draw_circle(min_image_t *image, int x, int y, int color);
int min_draw_rect(min_image_t *image, min_rect_t rect, int line_width, int color);

#endif // _MINIMG_H_
