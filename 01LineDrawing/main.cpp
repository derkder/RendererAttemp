#include "tgaimage.h"
using namespace std;
const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);

void line(int x0, int y0, int x1, int y1, TGAImage& image, TGAColor color) {
    bool steep = false; //标记当前斜率的绝对值是否大于1
    if (abs(x0 - x1) < abs(y0 - y1)) {
        //斜率绝对值>1了，此时将线段端点各自的x,y坐标对调。
        swap(x0, y0);
        swap(x1, y1);
        steep = true;
    }
    if (x0 > x1) { //当x0>x1时，调换两个点的坐标
        swap(x0, x1);
        swap(y0, y1);
    }
    for (int x = x0; x <= x1; x++) {
        float t = (x - x0) / (float)(x1 - x0);
        int y = y0 * (1. - t) + y1 * t;// = y0 + t (y1 - y0) 
        if (steep) {
            //如果线段是斜率大于1的，那么线段上的点原本坐标应该是(y,x)
            image.set(y, x, color);
        }
        else {
            image.set(x, y, color);
        }
    }
}

int main(int argc, char** argv) {
    TGAImage image(100, 100, TGAImage::RGB);
    line(13, 20, 80, 40, image, white); //线段A
    line(20, 13, 40, 80, image, red); //线段B
    line(80, 40, 13, 20, image, red);//线段C
    image.flip_vertically();
    image.write_tga_file("output.tga");
    return 0;
}

