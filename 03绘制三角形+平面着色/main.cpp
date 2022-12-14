#include <vector>
#include <cmath>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
using namespace std;
const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red   = TGAColor(255, 0,   0,   255);
const TGAColor green = TGAColor(0,   255, 0,   255);
Model *model = NULL;
const int width  = 800;
const int height = 800;

//画线算法(坐标1，坐标2，tga指针，颜色)
void line(Vec2i p0, Vec2i p1, TGAImage &image, TGAColor color) {
    bool steep = false;
    if (std::abs(p0.x-p1.x)<std::abs(p0.y-p1.y)) {
        std::swap(p0.x, p0.y);
        std::swap(p1.x, p1.y);
        steep = true;
    }
    if (p0.x>p1.x) {
        std::swap(p0, p1);
    }

    for (int x=p0.x; x<=p1.x; x++) {
        float t = (x-p0.x)/(float)(p1.x-p0.x);
        int y = p0.y*(1.-t) + p1.y*t;
        if (steep) {
            image.set(y, x, color);
        } else {
            image.set(x, y, color);
        }
    }
}

//绘制三角形(坐标1，坐标2，坐标3，tga指针，颜色)
//用横线填色
void triangle(Vec2i t0, Vec2i t1, Vec2i t2, TGAImage &image, TGAColor color) {
    //三角形面积为0
    if (t0.y==t1.y && t0.y==t2.y) return;
    //根据y的大小对坐标进行排序
    if (t0.y>t1.y) swap(t0, t1);
    if (t0.y>t2.y) swap(t0, t2);
    if (t1.y>t2.y) swap(t1, t2);
    int total_height = t2.y-t0.y;
    //以高度差作为循环控制变量，此时不需要考虑斜率，因为着色完后每行都会被填充
    for (int i=0; i<total_height; i++) {
        //根据t1将三角形分割为上下两部分
        bool second_half = i>t1.y-t0.y || t1.y==t0.y;//是否是上半部分
        int segment_height = second_half ? t2.y-t1.y : t1.y-t0.y;
        float alpha = (float)i/total_height;
        float beta  = (float)(i-(second_half ? t1.y-t0.y : 0))/segment_height; 
        //计算A,B两点的坐标
        Vec2i A =               t0 + (t2-t0)*alpha;//对三角形横着切一刀左边那个点，用t2的坐标
        Vec2i B = second_half ? t1 + (t2-t1)*beta : t0 + (t1-t0)*beta;//对三角形横着切一刀右边那个点
        if (A.x>B.x) swap(A, B);
        //根据A,B和当前高度对tga着色（画线）
        for (int j=A.x; j<=B.x; j++) {
            image.set(j, t0.y+i, color);
        }
    }
}

int main(int argc, char** argv) {
    //读取model文件
    if (2==argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/african_head.obj");
    }
    //构造tgaimage
    TGAImage image(width, height, TGAImage::RGB);
    //高洛德着色Gouraud shading
    //指定光照方向
    Vec3f light_dir(0,0,-1);
    //模型的面作为循环控制变量
    for (int i=0; i<model->nfaces(); i++) {
        //face是一个数组，用于存储一个面的三个顶点
        vector<int> face = model->face(i);
        Vec2i screen_coords[3];
        Vec3f world_coords[3];
        for (int j=0; j<3; j++) {
            Vec3f v = model->vert(face[j]);
            //屏幕坐标    (-1,-1)映射为(0,0)  （1,1）映射为(width,height)[这里的映射直接扔掉z轴的，所以大概属于正交投影而不是透视投影]
            screen_coords[j] = Vec2i((v.x+1.)*width/2., (v.y+1.)*height/2.);
            //世界坐标    即模型坐标
            world_coords[j]  = v;
        }
        //用世界坐标计算法向量【三角形两条边的叉乘（y2y0 X y1y0）】
        //在模型的制作中，会规定好顶点的顺序，使得模型的每一个面的法向量朝向模型外部
        Vec3f n = (world_coords[2]-world_coords[0])^(world_coords[1]-world_coords[0]);
        n.normalize(); 
        // diffuse漫反射的公式：强度=法向量*光照方向   即法向量和光照方向重合时，亮度最高
        //[括号里面的小于0，说明平面朝向为内  即背面裁剪]
        float intensity = fmax(0., n * light_dir);
        //强度小于0，说明平面朝向为内  即背面裁剪
        if (intensity > 0){//不加判断的话背面会被渲染成黑色而挡住前面的面，因为这一课还没有深度缓冲，如果加上深度缓冲就不用判断了
        //话来不加判断的话会得到非常恐怖的图图，恐怖效果可以用
            triangle(screen_coords[0], screen_coords[1], screen_coords[2], image, TGAColor(intensity * 255, intensity * 255, intensity * 255, 255));
        }
        
    }

    image.flip_vertically(); // i want to have the origin at the left bottom corner of the image
    image.write_tga_file("output.tga");
    delete model;
    return 0;
}

