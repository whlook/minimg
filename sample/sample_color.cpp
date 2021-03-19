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
int main(int argc, char **argv)
{
        if (argc < 3) {
                printf("[USAGE] %s R G B\n", argv[0]);
                printf("[OR] %s -s COLOR \n", argv[0]);
                return 0;
        }
        string opt = argv[1];
        if (opt == "-s") {
                int c = stoi(argv[2]);
                unsigned char b = (c >> 16) & 0xff;
                unsigned char g = (c >> 8) & 0xff;
                unsigned char r = (c >> 0) & 0xff;
                printf("R:%d G:%d B:%d\n", r, g, b);
                return 0;
        }
        int r = stoi(argv[1]);
        int g = stoi(argv[2]);
        int b = stoi(argv[3]);
        int color = MAKE_COLOR(b, g, r);
        printf("%d\n", color);
        return 0;
}
