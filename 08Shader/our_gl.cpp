#include <cmath>
#include <limits>
#include <cstdlib>
#include "our_gl.h"
#include <algorithm>
Matrix ModelView;
Matrix Viewport;
Matrix Projection;

IShader::~IShader() {}

//视角矩阵
void viewport(int x, int y, int w, int h) {
    Viewport = Matrix::identity();
    Viewport[0][3] = x+w/2.f;
    Viewport[1][3] = y+h/2.f;
    Viewport[2][3] = 255.f/2.f;
    Viewport[0][0] = w/2.f;
    Viewport[1][1] = h/2.f;
    Viewport[2][2] = 255.f/2.f;
}

//投影矩阵
//param:coeff = -1.f/(eye-center)
void projection(float coeff) {
    Projection = Matrix::identity();
    Projection[3][2] = coeff;
}

//变换矩阵
void lookat(Vec3f eye, Vec3f center, Vec3f up) {
    Vec3f z = (eye-center).normalize();
    Vec3f x = cross(up,z).normalize();
    Vec3f y = cross(z,x).normalize();
    ModelView = Matrix::identity();
    Matrix translaition = Matrix::identity();
	Matrix rotation     = Matrix::identity();
	for (int i = 0; i < 3; i++) {
        translaition[i][3] = -center[i];
	}
    for (int i=0; i<3; i++) {
        rotation[0][i] = x[i];
        rotation[1][i] = y[i];
        rotation[2][i] = z[i];
    }
    ModelView = rotation* translaition;
}

//计算质心坐标
//只是计算，并没有判断是否在三角形里
Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P) {
    Vec3f s[2];
    for (int i=2; i--; ) {
        s[i][0] = B[i] - A[i];
        s[i][1] = C[i] - A[i];
        s[i][2] = A[i]-P[i];
    }
    Vec3f res = cross(s[0], s[1]);
    ////res[2]=0代表三点共线
    if (std::abs(res[2])>1e-2) 
        return Vec3f(1.f-(res.x+res.y)/res.z, res.x/res.z, res.y/res.z);
    return Vec3f(-1,1,1);
}

//绘制三角形
void triangle(Vec4f *pts, IShader &shader, TGAImage &image, TGAImage &zbuffer) {
    //初始化三角形边界框
    Vec2f bboxmin( std::numeric_limits<float>::max(),  std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            //这里pts除以了最后一个分量，实现了透视中的缩放，所以作为边界框
            //pts[i]为三角形的三个顶点
            //pts[i][2]为三角形的z信息(0~255)
            //pts[i][3]为三角形的投影系数(1-z/c)
            bboxmin[j] = std::min(bboxmin[j], pts[i][j]/pts[i][3]);
            bboxmax[j] = std::max(bboxmax[j], pts[i][j]/pts[i][3]);
        }
    }
    //当前像素坐标P，颜色color
    Vec2i P;
    TGAColor color;
    //遍历边界框中的每一个像素
    for (P.x=bboxmin.x; P.x<=bboxmax.x; P.x++) {
        for (P.y=bboxmin.y; P.y<=bboxmax.y; P.y++) {
            //c为当前P对应的质心坐标
            //这里pts除以了最后一个分量，实现了透视中的缩放，所以用于判断P是否在三角形内
            Vec3f c = barycentric(proj<2>(pts[0]/pts[0][3]), proj<2>(pts[1]/pts[1][3]), proj<2>(pts[2]/pts[2][3]), proj<2>(P));
            //插值计算P的zbuffer
            float z_P = (pts[0][2]/ pts[0][3])*c.x + (pts[0][2] / pts[1][3]) *c.y + (pts[0][2] / pts[2][3]) *c.z;
            int frag_depth = std::max(0, std::min(255, int(z_P+.5)));
            //P的任一质心分量小于0或者zbuffer小于已有zbuffer，不渲染
            //正常来说如果是深度测试，应该是值越小代表越在前面，但这里用z的大小代表的深度，也就是越大越在前面
            if (c.x<0 || c.y<0 || c.z<0 || zbuffer.get(P.x, P.y)[0]>frag_depth) continue;
            //调用片元着色器计算当前像素颜色，第二个参数是个地址，fragment会给这个赋值
            bool discard = shader.fragment(c, color);
            if (!discard) {
                //zbuffer
                zbuffer.set(P.x, P.y, TGAColor(frag_depth));
                //为像素设置颜色
                image.set(P.x, P.y, color);
            }
        }
    }
}

