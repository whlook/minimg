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

static std::vector<min_pointf_t> get_points(std::string file)
{
        std::vector<min_pointf_t> out;
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
                std::string x = line_str.substr(0, pos);
                std::string y = line_str.substr(pos + 1, line_str.size());
                min_pointf_t point;
                point.x = std::stof(x);
                point.y = std::stof(y);
                out.push_back(point);
        }
        infile.close();
        return out;
}

int main(int argc, char **argv)
{
        if (argc < 3) {
                printf("[USAGE] %s image_file point_file [color]\n", argv[0]);
                printf("-------------------------\n");
                printf("| point1.x,point1.y\n");
                printf("| point2.x,point2.y\n");
                printf("| .................\n");
                printf("| pointn.x,pointn.y\n");
                printf("-------------------------\n");
                return 0;
        }
        std::string img_file = argv[1];
        std::string point_file = argv[2];
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

        std::vector<min_pointf_t> points = get_points(point_file);
        for (size_t i = 0; i < points.size(); ++i) {
                min_draw_circle(image, points[i].x, points[i].y, color);
        }
        min_image_write(img_file.c_str(), image);
        min_image_release(image);
        return 0;
}
