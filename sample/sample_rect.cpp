#include "minimg.h"
#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/input.h>
#include <linux/uinput.h>
#include <list>
#include <map>
#include <pthread.h>
#include <queue>
#include <regex>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

static std::vector<min_rect_t> get_rects(std::string file)
{
        std::vector<min_rect_t> out;
        std::ifstream infile(file, std::ios::in);
        if (!infile.is_open()) {
                printf("file not open\n");
                return out;
        }
        while (!infile.eof()) {
                std::string line_str;
                infile >> line_str;
                if (line_str.empty()) {
                        break;
                }
                size_t pos = line_str.find(",");
                std::string left = line_str.substr(0, pos);
                std::string temp = line_str.substr(pos + 1, line_str.size());
                pos = temp.find(",");
                std::string top = temp.substr(0, pos);
                temp = temp.substr(pos + 1, line_str.size());
                pos = temp.find(",");
                std::string right = temp.substr(0, pos);
                std::string bottom = temp.substr(pos + 1, line_str.size());

                min_rect_t rect;
                rect.left = std::stoi(left);
                rect.top = std::stoi(top);
                rect.right = std::stoi(right);
                rect.bottom = std::stoi(bottom);
                out.push_back(rect);
        }
        infile.close();
        return out;
}

int main(int argc, char **argv)
{
        if (argc < 3) {
                printf("[USAGE] %s image_file rect_file [color]\n", argv[0]);
                printf("-------------------------\n");
                printf("| left1,top1,right1,bottom1\n");
                printf("| left2,top2,right2,bottom2\n");
                printf("| .........................\n");
                printf("| leftn,topn,rightn,bottomn\n");
                printf("-------------------------\n");
                return 0;
        }
        std::string img_file = argv[1];
        std::string rect_file = argv[2];
        int color = MAKE_COLOR(255, 0, 0);
        if (argc > 3) {
                color = stoi(argv[3]);
        }
        int ret = 0;
        min_image_t *image = NULL;
        ret = min_image_read(img_file.c_str(), &image);
        if (ret != 0) {
                printf("image read err:%d\n", ret);
                return ret;
        }

        std::vector<min_rect_t> rects = get_rects(rect_file);
        for (size_t i = 0; i < rects.size(); ++i) {
                min_draw_rect(image, rects[i], 2, color);
        }
        min_image_write(img_file.c_str(), image);
        min_image_release(image);
        return 0;
}
