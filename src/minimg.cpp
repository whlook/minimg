#include "minimg.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "jpeglib.h"
#include "png.h"
#include <dirent.h>
#include <setjmp.h>
#include <vector>

int min_image_allocate(int w, int h, int s, min_pixel_format_e p, min_image_t **img)
{
        *img = new min_image_t;
        (*img)->pixel_format = p;
        (*img)->width = w;
        (*img)->height = h;
        (*img)->stride = s;
        (*img)->data = new unsigned char[h * s];
        return 0;
}
int min_image_release(min_image_t *img)
{
        if (img) {
                if (img->data) {
                        delete[] img->data;
                }
                delete img;
        }
}

// JPG HEADER LENGTH
#define JPG_BYTES_TO_CHECK 3
#define JPG_SIGNATURE "\xFF\xD8\xFF"

// PNG HEADER LENGTH
#define PNG_BYTES_TO_CHECK 8

// BMP HEADER LENGTH
#define BMP_BYTES_TO_CHECK 2
#define BMP_SIGNATURE "BM"

// TIFF HEADER LENGTH
#define TIFF_BYTES_TO_CHECK 4
#define TIFF_SIGNATURE_II "II\x2A\x00"
#define TIFF_SIGNATURE_MM "MM\x00\x2A"

static bool check_if_jpg(const char *file_name)
{
        if (!file_name) {
                return false;
        }
        FILE *fp = fopen(file_name, "rb");
        if (!fp) {
                return false;
        }

        char buf[JPG_BYTES_TO_CHECK];
        int read_count = fread(buf, 1, JPG_BYTES_TO_CHECK, fp);
        fclose(fp);

        if (read_count != JPG_BYTES_TO_CHECK) {
                return false;
        }

        return (0 == memcmp(buf, JPG_SIGNATURE, JPG_BYTES_TO_CHECK));
}

static bool check_if_png(const char *file_name)
{
        if (!file_name) {
                return false;
        }
        FILE *fp = fopen(file_name, "rb");
        if (!fp) {
                return false;
        }

        char buf[PNG_BYTES_TO_CHECK];
        int read_count = fread(buf, 1, PNG_BYTES_TO_CHECK, fp);
        fclose(fp);

        if (read_count != PNG_BYTES_TO_CHECK) {
                return false;
        }

        return (0 == png_sig_cmp((png_const_bytep)buf, 0, PNG_BYTES_TO_CHECK));
}

static bool check_if_bmp(const char *file_name)
{
        if (!file_name) {
                return false;
        }
        FILE *fp = fopen(file_name, "rb");
        if (!fp) {
                return false;
        }

        char buf[BMP_BYTES_TO_CHECK];
        int read_count = fread(buf, 1, BMP_BYTES_TO_CHECK, fp);
        fclose(fp);

        if (read_count != BMP_BYTES_TO_CHECK) {
                return false;
        }

        return (0 == memcmp(buf, BMP_SIGNATURE, BMP_BYTES_TO_CHECK));
}

struct my_error_mgr {
        struct jpeg_error_mgr pub; /* "public" fields */
        jmp_buf setjmp_buffer; /* for return to caller */
};
typedef struct my_error_mgr *my_error_ptr;

static void my_error_exit(j_common_ptr cinfo)
{
        my_error_ptr myerr = (my_error_ptr)cinfo->err;
        (*cinfo->err->output_message)(cinfo);
        longjmp(myerr->setjmp_buffer, 1);
}

static void cmyk_to_rgb(JSAMPLE c, JSAMPLE m, JSAMPLE y, JSAMPLE k, JSAMPLE *r, JSAMPLE *g,
                        JSAMPLE *b)
{
        *r = (JSAMPLE)((double)c * (double)k / 255.0 + 0.5);
        *g = (JSAMPLE)((double)m * (double)k / 255.0 + 0.5);
        *b = (JSAMPLE)((double)y * (double)k / 255.0 + 0.5);
}

static int jpeg_image_write(const char *path, min_image_t *image_in)
{
        if (!image_in) {
                return -1;
        }
        min_image_t *image = image_in;
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        FILE *outfile; /* target file */
        JSAMPROW row_pointer[1]; /* pointer to JSAMPLE row[s] */
        int row_stride; /* physical row width in image buffer */

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        if ((outfile = fopen(path, "wb")) == NULL) {
                fprintf(stderr, "[ERR] Can't open %s\n", path);
                return -1;
        }
        jpeg_stdio_dest(&cinfo, outfile);

        cinfo.image_width = image->width; /* image width and height, in pixels */
        cinfo.image_height = image->height;
        cinfo.input_components = 3; /* # of color components per pixel */
        cinfo.in_color_space = JCS_RGB; /* colorspace of input image */
        if (image->pixel_format == MIN_PIX_FMT_GRAY8) {
                cinfo.input_components = 1;
                cinfo.in_color_space = JCS_GRAYSCALE;
        }
        if (image->pixel_format == MIN_PIX_FMT_NV12 || image->pixel_format == MIN_PIX_FMT_NV21) {
                cinfo.input_components = 1;
                cinfo.in_color_space = JCS_GRAYSCALE;
                // return -1;
        }

        jpeg_set_defaults(&cinfo);

        jpeg_set_quality(&cinfo, 100, TRUE /* limit to baseline-jpeg values */);
        jpeg_start_compress(&cinfo, TRUE);

        row_stride = image->stride; /* JSAMPLEs per row in image_buffer */

        row_pointer[0] = (JSAMPROW)malloc(row_stride);
        while (cinfo.next_scanline < cinfo.image_height) {
                memcpy(row_pointer[0], &image->data[cinfo.next_scanline * row_stride],
                       row_stride);
                for (int i = 0; i < row_stride - 2; i += 3) {
                        unsigned char tmp;
                        tmp = row_pointer[0][i];
                        row_pointer[0][i] = row_pointer[0][i + 2];
                        row_pointer[0][i + 2] = tmp;
                }
                (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }
        free(row_pointer[0]);
        jpeg_finish_compress(&cinfo);
        fclose(outfile);
        jpeg_destroy_compress(&cinfo);
        if (image != image_in) {
                min_image_release(image);
        }
        return 0;
}

// jpeg
static int jpeg_image_read(const char *path, min_image_t **image)
{
        if (check_if_jpg(path) == false) {
                fprintf(stderr, "[ERR] Not jpg file:%s\n", path);
                return -1;
        }
        struct jpeg_decompress_struct cinfo;
        struct my_error_mgr jerr;
        FILE *infile; /* source file */
        int row_stride; /* physical row width in output buffer */

        if ((infile = fopen(path, "rb")) == NULL) {
                fprintf(stderr, "[ERR] Can't open %s\n", path);
                return -1;
        }

        cinfo.err = jpeg_std_error(&jerr.pub);
        jerr.pub.error_exit = my_error_exit;
        if (setjmp(jerr.setjmp_buffer)) {
                jpeg_destroy_decompress(&cinfo);
                fclose(infile);
                return -1;
        }
        jpeg_create_decompress(&cinfo);

        jpeg_stdio_src(&cinfo, infile);

        (void)jpeg_read_header(&cinfo, TRUE);

        if (JCS_GRAYSCALE != cinfo.out_color_space && JCS_RGB != cinfo.out_color_space) {
                //&& JCS_CMYK != cinfo.out_color_space) {
                fprintf(stderr, "[ERR], non-supported color format, %d\n", cinfo.out_color_space);
                jpeg_destroy_decompress(&cinfo);
                fclose(infile);
                return -1;
        }

        (void)jpeg_start_decompress(&cinfo);

        row_stride = cinfo.output_width * cinfo.output_components;

        min_pixel_format_e pixel_format = MIN_PIX_FMT_BGR888;
        int output_stride = cinfo.output_width * 3;
        if (cinfo.out_color_space == JCS_GRAYSCALE) {
                pixel_format = MIN_PIX_FMT_GRAY8;
                output_stride = cinfo.output_width;
        }

        int ret = min_image_allocate(cinfo.output_width, cinfo.output_height, output_stride,
                                     pixel_format, image);
        if (ret != 0) {
                jpeg_destroy_decompress(&cinfo);
                fclose(infile);
                return ret;
        }

        unsigned char *line_buf = NULL;
        if (JCS_CMYK == cinfo.out_color_space) {
                line_buf = new unsigned char[row_stride];
        }

        unsigned char *p, *src = NULL;
        while (cinfo.output_scanline < cinfo.output_height) {
                if (cinfo.out_color_space == JCS_CMYK) {
                        p = (unsigned char *)((*image)->data +
                                              output_stride * cinfo.output_scanline);
                        jpeg_read_scanlines(&cinfo, &line_buf, 1);
                        src = line_buf;
                        for (unsigned int i = 0; i < cinfo.output_width; ++i) {
                                JSAMPLE c = *src++, m = *src++, y = *src++, k = *src++;
                                cmyk_to_rgb(c, m, y, k, p + 2, p + 1, p);
                                p += 3;
                        }
                } else if (cinfo.out_color_space == JCS_RGB) {
                        p = (unsigned char *)(*image)->data + row_stride * cinfo.output_scanline;
                        (void)jpeg_read_scanlines(&cinfo, &p, 1);
                        int i = 0;
                        for (i = 0; i < row_stride - 2; i += 3) {
                                unsigned char tmp;
                                tmp = p[i];
                                p[i] = p[i + 2];
                                p[i + 2] = tmp;
                        }
                } else if (cinfo.out_color_space == JCS_GRAYSCALE) {
                        p = (unsigned char *)(*image)->data + row_stride * cinfo.output_scanline;
                        (void)jpeg_read_scanlines(&cinfo, &p, 1);
                }
        }
        if (line_buf) {
                delete[] line_buf;
                line_buf = NULL;
        }

        (void)jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
}

// png
static int png_image_read(const char *path, min_image_t **image)
{
        if (check_if_png(path) == false) {
                fprintf(stderr, "[ERR] Not png file:%s\n", path);
                return -1;
        }
        png_structp png_ptr;
        png_infop info_ptr;
        FILE *fp;
        png_uint_32 width;
        png_uint_32 height;
        int bit_depth;
        int color_type;
        int interlace_method;
        int compression_method;
        int filter_method;
        png_bytepp rows;
        fp = fopen(path, "rb");
        if (!fp) {
                fprintf(stderr, "[ERR] Cannot open '%s': %s\n", path, strerror(errno));
                return -1;
        }
        png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!png_ptr) {
                fprintf(stderr, "[ERR] Cannot create PNG read structure");
                fclose(fp);
                return -1;
        }
        info_ptr = png_create_info_struct(png_ptr);
        if (!png_ptr) {
                fprintf(stderr, "[ERR] Cannot create PNG info structure");
                fclose(fp);
                return -1;
        }
        png_init_io(png_ptr, fp);
        png_read_png(png_ptr, info_ptr, 0, 0);
        png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                     &interlace_method, &compression_method, &filter_method);
        rows = png_get_rows(png_ptr, info_ptr);
        int rowbytes;

        int w = (int)width;
        int h = (int)height;
        rowbytes = png_get_rowbytes(png_ptr, info_ptr);
        fclose(fp);

        min_pixel_format_e pixel_format = MIN_PIX_FMT_BGR888;
        int output_stride = rowbytes;
        int ret = min_image_allocate(w, h, output_stride, pixel_format, image);
        for (int i = 0; i < h; ++i) {
                png_bytep row;
                row = rows[i];
                for (int j = 0; j < rowbytes; ++j) {
                        (*image)->data[i * rowbytes + j] = (unsigned char)row[j];
                }
        }
        return 0;
}

// bmp
typedef enum bit_data_type_e {
        BIT32 = 1, //读取或存储成32位
        BIT24 = 2, //读取或存储成24位
} bit_data_type_e;

typedef struct bmp_head_t {
        unsigned char type[2]; //存储 'B' 'M'                    2字节
        unsigned int size; //位图文件大小                    4字节
        unsigned short reserved1; //保留字                          2字节
        unsigned short reserved2; //保留字                          2字节
        unsigned int offBits; //位图数据起始位置                4字节
} bmp_head_t;
typedef struct bmp_info_head_t {
        unsigned int selfSize; //位图信息头的大小                 4字节
        long bitWidth; //位图的宽度,以像素为单位          4字节
        long bitHeight; //位图的高度,以像素为单位          4字节
        unsigned short bitPlanes; //目标设备的级别,必须为1           2字节
        unsigned short pixelBitCount; //每个像素所需的位数               2字节
        unsigned int compression; //位图压缩类型,0(不压缩)           4字节
        unsigned int sizeImage; //位图的大小,以字节为单位          4字节
        long pixelXPerMeter; //位图的水平分辨率,每米像素数      4字节
        long pixelYPerMeter; //位图的垂直分辨率,每米像素数      4字节
        unsigned int colorUsed; //位图实际使用的颜色表中的颜色数   4字节
        unsigned int colorImportant; //位图显示过程中重要的颜色数       4字节
} bmp_info_head_t;

#define BMP_SIZE_FILEHEADER 14
#define BMP_SIZE_INFOHEADER 40

#define BMP_COLOR_BITS_24 24
#define BMP_COLOR_BITS_32 32

static unsigned int uInt16Number(unsigned char buf[2])
{
        return (buf[1] << 8) | buf[0];
}

static unsigned int uInt32Number(unsigned char buf[4])
{
        unsigned numb = buf[3];
        numb = (numb << 8) | buf[2];
        numb = (numb << 8) | buf[1];
        numb = (numb << 8) | buf[0];
        return numb;
}

static int read_header(FILE *f, int *bitmap_pos)
{

        unsigned char header[BMP_SIZE_FILEHEADER];
        bmp_head_t bmp_head;

        int numb = 0;
        int offset = 0;

        if (fseek(f, 0, SEEK_SET))
                return -1;
        //读取bmp head信息
        numb = fread(header, BMP_SIZE_FILEHEADER, 1, f);
        if (numb != 1)
                return -2;
        // 0 - 1
        if (header[0] != 'B' || header[1] != 'M')
                return -3;
        bmp_head.type[0] = header[0];
        bmp_head.type[1] = header[1];

        // 2 - 5
        bmp_head.size = uInt32Number(header + 2);
        // 6 - 7
        // 8 - 9

        // 10 - 13
        offset = uInt32Number(header + 10);
        if (offset != 54)
                return -4;

        *bitmap_pos = offset;
        return 0;
}

static int read_info_head(FILE *f, int &w, int &h)
{

        unsigned char header[BMP_SIZE_INFOHEADER];
        bmp_info_head_t bmp_info_head;

        int numb = 0;

        if (fseek(f, BMP_SIZE_FILEHEADER, SEEK_SET))
                return -1;
        //读取bmp info head信息
        numb = fread(header, BMP_SIZE_INFOHEADER, 1, f);
        if (numb != 1)
                return -1;

        // 14 - 17
        bmp_info_head.selfSize = uInt32Number(header);
        if (bmp_info_head.selfSize != 40)
                return -1;

        // 18 - 21
        bmp_info_head.bitWidth = (long)uInt32Number(header + 4);
        // 22 - 25
        bmp_info_head.bitHeight = (long)uInt32Number(header + 8);
        // 26 - 27
        bmp_info_head.bitPlanes = (unsigned short)uInt16Number(header + 12);
        // 28 - 29
        bmp_info_head.pixelBitCount = (unsigned short)uInt16Number(header + 14);
        // 30 - 33
        bmp_info_head.compression = uInt32Number(header + 16);
        // 34 - 37
        bmp_info_head.sizeImage = uInt32Number(header + 20);
        // 38 - 41
        bmp_info_head.pixelXPerMeter = (long)uInt32Number(header + 24);
        // 42 - 45
        bmp_info_head.pixelYPerMeter = (long)uInt32Number(header + 28);
        // 46 - 49
        bmp_info_head.colorUsed = uInt32Number(header + 32);
        // 50 - 53
        bmp_info_head.colorImportant = uInt32Number(header + 36);

        w = bmp_info_head.bitWidth;
        h = bmp_info_head.bitHeight;

        return (int)(bmp_info_head.pixelBitCount);
}

int bmp_image_read(const char *file, min_image_t **out_img, bit_data_type_e bit_data_type = BIT24)
{
        if (check_if_bmp(file) == false) {
                fprintf(stderr, "[ERR] Not bmp file:%s\n", file);
                return -1;
        }
        FILE *f;
        int bitmap_pos;
        int n_bits;

        int flErr = 0;

        f = fopen(file, "rb");

        if (!f) {
                return -1;
        }

        if (0 > (flErr = read_header(f, &bitmap_pos))) {
                fclose(f);
                return -1;
        }

        int w, h;
        n_bits = read_info_head(f, w, h);

        if (n_bits == BMP_COLOR_BITS_24) {
                int rgb_size;
                int rgba_size;
                unsigned char *rgb;
                int y;
                unsigned char *line;
                int rest_4;

                if (fseek(f, bitmap_pos, SEEK_SET)) {
                        fclose(f);
                        return -1;
                }

                rgb_size = 3 * w;
                rgba_size = 4 * w;

                rest_4 = rgb_size % 4;
                if (rest_4 > 0)
                        rgb_size += 4 - rest_4;

                if (bit_data_type == 1) {
                        min_image_allocate(w, h, w * 4, MIN_PIX_FMT_BGRA8888, out_img);

                        rgb = (unsigned char *)malloc(rgb_size);

                        if (NULL == rgb) {
                                return -1;
                        }

                        for (y = (*out_img)->height - 1; y >= 0; y--) {
                                int numb = 0;
                                int x = 0;

                                numb = fread(rgb, rgb_size, 1, f);
                                if (numb != 1) {
                                        fclose(f);
                                        free(rgb);
                                        return -1;
                                }

                                numb = 0;
                                line = (*out_img)->data + (*out_img)->width * 4 * y;
                                for (x = 0; x < (*out_img)->width; x++) {
                                        line[3] = 255;
                                        line[2] = rgb[numb++];
                                        line[1] = rgb[numb++];
                                        line[0] = rgb[numb++];
                                        line += 4;
                                }
                        }
                } else if (bit_data_type == 2) {
                        min_image_allocate(w, h, w * 3, MIN_PIX_FMT_BGR888, out_img);

                        rgb = (unsigned char *)malloc(rgb_size);

                        if (NULL == rgb)
                                return -1;

                        for (y = (*out_img)->height - 1; y >= 0; y--) {
                                int numb = 0;
                                int x = 0;

                                numb = fread(rgb, rgb_size, 1, f);
                                if (numb != 1) {
                                        fclose(f);
                                        free(rgb);
                                        return -1;
                                }

                                numb = 0;
                                line = (*out_img)->data + (*out_img)->width * 3 * y;
                                for (x = 0; x < (*out_img)->width; x++) {
                                        line[2] = rgb[numb++];
                                        line[1] = rgb[numb++];
                                        line[0] = rgb[numb++];
                                        line += 3;
                                }
                        }
                }
                fclose(f);
                free(rgb);
        } else if (n_bits == BMP_COLOR_BITS_32) {
                int rgba_size;
                unsigned char *rgba;
                int y;
                unsigned char *line;

                if (fseek(f, bitmap_pos, SEEK_SET)) {
                        fclose(f);
                        return -1;
                }

                rgba_size = 4 * (*out_img)->width;
                if (bit_data_type == 1) {
                        min_image_allocate(w, h, w * 4, MIN_PIX_FMT_BGRA8888, out_img);

                        rgba = (unsigned char *)malloc(rgba_size);

                        if (NULL == rgba)
                                return -1;

                        for (y = (*out_img)->height - 1; y >= 0; y--) {
                                int numb = 0;
                                int x = 0;

                                numb = fread(rgba, rgba_size, 1, f);
                                if (numb != 1) {
                                        fclose(f);
                                        free(rgba);
                                        return -1;
                                }

                                numb = 0;
                                line = (*out_img)->data + (*out_img)->width * 4 * y;
                                for (x = 0; x < (*out_img)->width; x++) {
                                        line[2] = rgba[numb++]; // B
                                        line[1] = rgba[numb++]; // G
                                        line[0] = rgba[numb++]; // R
                                        line[3] = rgba[numb++]; // A
                                        line += 4;
                                }
                        }
                } else if (bit_data_type == 2) {
                        min_image_allocate(w, h, w * 3, MIN_PIX_FMT_BGR888, out_img);

                        rgba = (unsigned char *)malloc(rgba_size);

                        if (NULL == rgba)
                                return -1;

                        for (y = (*out_img)->height - 1; y >= 0; y--) {
                                int numb = 0;
                                int x = 0;

                                numb = fread(rgba, rgba_size, 1, f);
                                if (numb != 1) {
                                        fclose(f);
                                        free(rgba);
                                        return -1;
                                }

                                numb = 0;
                                line = (*out_img)->data + (*out_img)->width * 3 * y;
                                for (x = 0; x < (*out_img)->width; x++) {
                                        line[2] = rgba[numb++]; // B
                                        line[1] = rgba[numb++]; // G
                                        line[0] = rgba[numb++]; // R
                                        line += 3;
                                        numb++;
                                }
                        }
                }
                fclose(f);
                free(rgba);
        } else {
                return -1;
        }
        return 0;
}

static bool check_substr(std::string str, std::string substr)
{
        std::size_t found = str.find(substr);
        if (found != std::string::npos) {
                return true;
        }
        return false;
}

int min_image_read(const char *path, min_image_t **image)
{
        std::string pathstr = path;
        int ret = 0;
        if (check_substr(pathstr, ".jpg") || check_substr(pathstr, ".JPG") ||
            check_substr(pathstr, ".jpeg") || check_substr(pathstr, ".JPEG")) {
                ret = jpeg_image_read(path, image);
                return ret;
        }
        if (check_substr(pathstr, ".png") || check_substr(pathstr, ".PNG")) {
                ret = png_image_read(path, image);
                return ret;
        }
        if (check_substr(pathstr, ".bmp") || check_substr(pathstr, ".BMP")) {
                ret = bmp_image_read(path, image);
                return ret;
        }
        return -1;
}

int min_image_write(const char *path, min_image_t *image)
{
        int ret = jpeg_image_write(path, image);
        return ret;
}

int min_draw_rect(min_image_t *image, min_rect_t rect, int line_width, int color)
{
        if (!image) {
                return -1;
        }
        if (image->pixel_format != MIN_PIX_FMT_BGR888) {
                fprintf(stderr, "[draw_rect] only bgr888 support\n");
                return -1;
        }
        unsigned char b = (color >> 16) & 0xff;
        unsigned char g = (color >> 8) & 0xff;
        unsigned char r = (color >> 0) & 0xff;
        for (int i = 0; i < line_width; ++i) {
                ++rect.left;
                ++rect.top;
                --rect.right;
                --rect.bottom;
                for (int start = rect.left; start <= rect.right; ++start) {
                        image->data[rect.top * image->stride + start * 3 + 0] = b;
                        image->data[rect.top * image->stride + start * 3 + 1] = g;
                        image->data[rect.top * image->stride + start * 3 + 2] = r;

                        image->data[rect.bottom * image->stride + start * 3 + 0] = b;
                        image->data[rect.bottom * image->stride + start * 3 + 1] = g;
                        image->data[rect.bottom * image->stride + start * 3 + 2] = r;
                }
                for (int start = rect.top; start <= rect.bottom; ++start) {
                        image->data[start * image->stride + rect.left * 3 + 0] = b;
                        image->data[start * image->stride + rect.left * 3 + 1] = g;
                        image->data[start * image->stride + rect.left * 3 + 2] = r;

                        image->data[start * image->stride + rect.right * 3 + 0] = b;
                        image->data[start * image->stride + rect.right * 3 + 1] = g;
                        image->data[start * image->stride + rect.right * 3 + 2] = r;
                }
        }
        return 0;
}

int min_draw_circle(min_image_t *image, int x, int y, int color)
{
        if (!image) {
                return -1;
        }
        if (image->pixel_format != MIN_PIX_FMT_BGR888) {
                fprintf(stderr, "[draw_circle] only bgr888 support\n");
                return -1;
        }
        x = x < 1 ? 1 : x;
        x = x > (image->width - 2) ? (image->width - 2) : x;
        y = y < 1 ? 1 : y;
        y = y > (image->height - 2) ? (image->height - 2) : y;
        int xs[9] = { x - 1, x - 1, x - 1, x, x, x, x + 1, x + 1, x + 1 };
        int ys[9] = { y + 1, y, y - 1, y - 1, y, y + 1, y - 1, y, y + 1 };

        unsigned char b = (color >> 16) & 0xff;
        unsigned char g = (color >> 8) & 0xff;
        unsigned char r = (color >> 0) & 0xff;

        for (int i = 0; i < 9; ++i) {
                image->data[ys[i] * image->stride + xs[i] * 3 + 0] = b;
                image->data[ys[i] * image->stride + xs[i] * 3 + 1] = g;
                image->data[ys[i] * image->stride + xs[i] * 3 + 2] = r;
        }
        return 0;
}
