#include <vector>
#include <cmath>
#include <cstdlib>
#include <limits>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include <algorithm>
const int width  = 800;
const int height = 800;
const int depth  = 255;

Model *model = NULL;
int *zbuffer = NULL;
Vec3f light_dir(0.2,0.15,-1);
Vec3f camera(0,0,3);

//4d-->3d
//除以最后一个分量。（当最后一个分量为0，表示向量）
//不为0，表示坐标
Vec3f m2v(Matrix m) {
    return Vec3f(m[0][0]/m[3][0], m[1][0]/m[3][0], m[2][0]/m[3][0]);
}

//3d-->4d
//添加一个1表示坐标
Matrix v2m(Vec3f v) {
    Matrix m(4, 1);
    m[0][0] = v.x;
    m[1][0] = v.y;
    m[2][0] = v.z;
    m[3][0] = 1.f;
    return m;
}

//视角矩阵
//将物体x，y坐标从(0,0)（1，1）转换到屏幕坐标(100,100) (700,700)    1/8width~7/8width
//这里不是转移到（0，800）所以平移并不是直接加1
//zbuffer(-1,1)转换到0~255
//这里input的xy是屏幕左下角的坐标wh是屏幕右下角的坐标
Matrix viewport(int x, int y, int w, int h) {
    Matrix m = Matrix::identity(4);
    //第4列表示平移信息
    m[0][3] = x+w/2.f;
    m[1][3] = y+h/2.f;
    m[2][3] = depth/2.f;
    //对角线表示缩放信息
    m[0][0] = w/2.f;
    m[1][1] = h/2.f;
    m[2][2] = depth/2.f;
    return m;
}



//绘制三角形(顶点坐标，uv坐标，tga指针，亮度，zbuffer)
void triangle(Vec3i t0, Vec3i t1, Vec3i t2, Vec2i uv0, Vec2i uv1, Vec2i uv2, TGAImage &image, float intensity, int *zbuffer) {
    if (t0.y==t1.y && t0.y==t2.y) return;
    //分割成两个三角形
    if (t0.y>t1.y) { std::swap(t0, t1); std::swap(uv0, uv1); }
    if (t0.y>t2.y) { std::swap(t0, t2); std::swap(uv0, uv2); }
    if (t1.y>t2.y) { std::swap(t1, t2); std::swap(uv1, uv2); }
    //用高度做循环控制
    int total_height = t2.y-t0.y;
    for (int i=0; i<total_height; i++) {
        //判断属于哪一部分以确定高度
        bool second_half = i>t1.y-t0.y || t1.y==t0.y;
        int segment_height = second_half ? t2.y-t1.y : t1.y-t0.y;
        //计算当前的比例
        float alpha = (float)i/total_height;
        float beta  = (float)(i-(second_half ? t1.y-t0.y : 0))/segment_height; // be careful: with above conditions no division by zero here
        //A表示t0与t2之间的点
        //B表示t0与t1之间的点或t1与t2之间的点
        Vec3i A   =               t0  + Vec3f(t2-t0  )*alpha;
        Vec3i B   = second_half ? t1  + Vec3f(t2-t1  )*beta : t0  + Vec3f(t1-t0  )*beta;
        //计算UV，和上面的公式基本一样
        Vec2i uvA =               uv0 +      (uv2-uv0)*alpha;
        Vec2i uvB = second_half ? uv1 +      (uv2-uv1)*beta : uv0 +      (uv1-uv0)*beta;
        //保证B在A的右边
        if (A.x > B.x) { std::swap(A, B); }// std::swap(uvA, uvB);}
        //用横坐标作为循环控制，对这一行进行着色
        /*之前只需要根据漫反射结果画的时候：
        for (int j = A.x; j <= B.x; j++) {
            image.set(j, t0.y + i, color);
        }
        */
        //现在是横着涂色的，看看可不可以用质点坐标着色
        for (int j=A.x; j<=B.x; j++) {
            //计算当前点在AB之间的比例
            float phi = B.x==A.x ? 1. : (float)(j-A.x)/(float)(B.x-A.x);
            //计算出当前点的坐标,A，B保存了z轴信息
            Vec3i   P = Vec3f(A) + Vec3f(B-A)*phi;
            Vec2i uvP =     uvA +   (uvB-uvA)*phi;
            if (P.x < width && P.y < height)
            {
                //计算当前zbuffer下标=P.x+P.y*width
                int idx = P.x+P.y*width;
                //当前点的z大于zbuffer信息，覆盖掉，并更新zbuffer
                if (zbuffer[idx]<P.z) {
                    zbuffer[idx] = P.z;
                    TGAColor color = model->diffuse(uvP);
                    image.set(P.x, P.y, TGAColor(color.r*intensity, color.g*intensity, color.b*intensity));
                }
            }
        }
    }
}

//计算质心坐标，具体分析在文档判断点是否在三角形内部里
Vec3f barycentric(Vec3f A, Vec3f B, Vec3f C, Vec3f P) {
    Vec3f s[2];
    //计算[AB,AC,PA]的x和y分量
    for (int i = 2; i--; ) {
        s[i][0] = C[i] - A[i];
        s[i][1] = B[i] - A[i];
        s[i][2] = A[i] - P[i];
    }
    //[u,v,1]和[AB,AC,PA]对应的x和y向量都垂直
    Vec3f u = cross(s[0], s[1]);
    //三点共线时，会导致u[2]为0，此时返回(-1,1,1)
    if (std::abs(u[2]) > 1e-2)
        //若1-u-v，u，v全为大于0的数，表示点在三角形内部
        //这里u.y/u.z是u，u.x/u.z是v，return的是（P点对于三角形ABC的质心坐标(1-u-v, u, v)）
        return Vec3f(1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z);
    return Vec3f(-1, 1, 1);
}

//用质心坐标进行着色
void triangleCentroid(Vec3i t0, Vec3i t1, Vec3i t2, Vec2i uv0, Vec2i uv1, Vec2i uv2, TGAImage& image, float intensity, int* zbuffer) {
    Vec2f bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    Vec2f clamp(image.get_width() - 1, image.get_height() - 1);
    //确定三角形的边框（2d版的三角形AABB包围盒）

}



int main(int argc, char** argv) {
    //读取模型
    if (2==argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/african_head.obj");
    }
    //构造zbuffer
    zbuffer = new int[width*height];
    for (int i=0; i<width*height; i++) {
        //初始化zbuffer
        zbuffer[i] = std::numeric_limits<int>::min();
    }

    //绘制
    {
        //初始化投影矩阵
        Matrix Projection = Matrix::identity(4);
        //初始化视角矩阵
        //后面两个3/4是【7/8（右上角的坐标为700）-1/8（左下角的坐标为）】也就是实际显示区域的长宽
        Matrix ViewPort   = viewport(width/8, height/8, width*3/4, height*3/4);
        //投影矩阵[3][2]=-1/c，c为相机z坐标，推导在文档里
        Projection[3][2] = -1.f/camera.z;
        TGAImage image(width, height, TGAImage::RGB);
        //以模型面为循环控制变量
        for (int i=0; i<model->nfaces(); i++) {
            std::vector<int> face = model->face(i);
            Vec3i screen_coords[3];
            Vec3f world_coords[3];
            for (int j=0; j<3; j++) {
                Vec3f v = model->vert(face[j]);
                //视角矩阵*投影矩阵*坐标
                //v2m()是用来将向量(vector)变成矩阵(matrix)，也就是我们提到的齐次坐标
                //这里的m2v可能相当于unityMVP变换中的M矩阵变换
                screen_coords[j] = m2v(ViewPort * Projection * v2m(v));
                world_coords[j]  = v;
            }
            //计算法向量
            Vec3f n = (world_coords[2]-world_coords[0])^(world_coords[1]-world_coords[0]);
            n.normalize();
            //计算漫反射强度
            float intensity = n*light_dir;
            intensity = std::min(std::abs(intensity),1.f);
            if (intensity>0) {
                Vec2i uv[3];
                for (int k=0; k<3; k++) {
                    uv[k] = model->uv(i, k);
                }
                //绘制三角形
                triangle(screen_coords[0], screen_coords[1], screen_coords[2], uv[0], uv[1], uv[2], image, intensity, zbuffer);
                //triangleCentroid(screen_coords[0], screen_coords[1], screen_coords[2], uv[0], uv[1], uv[2], image, intensity, zbuffer);
            }
        }
        image.flip_vertically();
        image.write_tga_file("output.tga");
    }
    //输出zbuffer
    {
        TGAImage zbimage(width, height, TGAImage::GRAYSCALE);
        for (int i=0; i<width; i++) {
            for (int j=0; j<height; j++) {
                zbimage.set(i, j, TGAColor(zbuffer[i+j*width], 1));
            }
        }
        zbimage.flip_vertically();
        zbimage.write_tga_file("zbuffer.tga");
    }
    delete model;
    delete [] zbuffer;
    return 0;
}

