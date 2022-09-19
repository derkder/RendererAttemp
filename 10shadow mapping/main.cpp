#include <vector>
#include <iostream>
#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "our_gl.h"

Model *model     = NULL;
const int width  = 800;
const int height = 800;

Vec3f light_dir(0,1,1);
Vec3f       eye(0.5,1,2);
Vec3f    center(0,0,1);
Vec3f        up(0,1,0);

//�������ɫ��
//������ķ�����ɫ����ͬ��������varying_intensity��¼��ÿ��vertex�Ĺ���ǿ�ȣ�������ƬԪ��ɫ���Ϳ���������vertex�Ĳ�ֵ���м�����
//������ɫ���Ὣ����д��varying_intensity
//ƬԪ��ɫ����varying_intensity�ж�ȡ����
struct GouraudShader : public IShader {
    Vec3f varying_intensity;
    mat<2, 3, float> varying_uv;
    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));//�Ȱ����������uv����ֵȷ����
        varying_intensity[nthvert] = std::max(0.f, model->normal(iface, nthvert) *light_dir); //�������������������ǿ����������Ϳ��Բ�ֵ��
        return Viewport * Projection * ModelView * gl_Vertex;
    }
    //���ݴ������������bar����ɫ���Լ�varying_intensity�������ǰ���ص���ɫ
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        Vec2f uv = varying_uv * bar;
        TGAColor uv2c = model->diffuse(uv);//��uv����ͨ�������uv���ݼ�ת������ɫ;_diffuse.tga�洢����һ��������ͼ��
        float intensity = varying_intensity*bar;
        color = uv2c *intensity;
        return false;                              
    }
};


//Phong����ɫ
struct PhongShader : public IShader {
    mat<2, 3, float> varying_uv;  // same as above
    mat<4, 4, float> uniform_M = Projection * ModelView;
    mat<4, 4, float> uniform_MIT = ModelView.invert_transpose();
    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
        return Viewport * Projection * ModelView * gl_Vertex; // transform it to screen coordinates
    }
    virtual bool fragment(Vec3f bar, TGAColor& color) {
        Vec2f uv = varying_uv * bar;//uvֻ���ò�ֵ����ʹ�����ƬԪ�����phong����ɫ
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize();
        Vec3f r = (n * (n * l * 2.f) - l).normalize();   // reflected light
        //Ϊɶ���ﲻ�ó���v����Ϊ��ȡ.z�ˣ��൱�ڶ�Ӧ�����ߵ�v����Ϊ��0��0��1�����൱�ڳ��Թ��ˣ�model->specular(uv)����ÿ�����Ƿ��й���(�������blingphong�иĳɰ��չ�ʽ����)
        float spec = pow(std::max(r.z, 0.0f), model->specular(uv)) ;
        //float spec = pow(std::max(v*r, 0.0f), model->specular(uv));//phong
        float diff = std::max(0.f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;
        for (int i = 0; i < 3; i++) color[i] = std::min<float>(5 + c[i] * (diff + .6 * spec), 255);//����ѭ��������ɫ��alpha��beta��gamma
        return false;
    }
};

struct BlingPhongShader : public IShader {
    mat<2, 3, float> varying_uv;  // same as above
    mat<4, 4, float> uniform_M = Projection * ModelView;
    mat<4, 4, float> uniform_MIT = ModelView.invert_transpose();
    virtual Vec4f vertex(int iface, int nthvert) {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert)); // read the vertex from .obj file
        return Viewport * Projection * ModelView * gl_Vertex; // transform it to screen coordinates
    }
    virtual bool fragment(Vec3f bar, TGAColor& color) {
        Vec2f uv = varying_uv * bar;//uvֻ���ò�ֵ����ʹ�����ƬԪ�����phong����ɫ
        Vec3f n = proj<3>(uniform_MIT * embed<4>(model->normal(uv))).normalize();
        Vec3f l = proj<3>(uniform_M * embed<4>(light_dir)).normalize();
        //Vec3f r = (n * (n * l * 2.f) - l).normalize();   // reflected light
        Vec3f v= proj<3>(uniform_M * embed<4>(eye)).normalize();
        Vec3f h= proj<3>(embed<4>(l+eye)).normalize();
        //float spec = pow(std::max(v*r, 0.0f), model->specular(uv));//phong
        float spec = pow(std::max(v * h, 0.0f), model->specular(uv));//bling-phong
        float diff = std::max(0.f, n * l);
        TGAColor c = model->diffuse(uv);
        color = c;
        //for (int i = 0; i < 3; i++) color[i] = std::min<float>(5 + c[i] * (diff + .9 * spec), 255);//����ѭ��������ɫ��alpha��beta��gamma
        for (int i = 0; i < 3; i++) color[i] = std::min<float>(5 + c[i] * (diff + 1.2 * spec), 255);//����ĸ߹�ò���е�����
        return false;
    }
};

//��һ����ֵ�ڵĹ���ǿ�ȸ��滻Ϊһ��
struct ToonShader : public IShader {
    mat<3, 3, float> varying_tri;
    Vec3f          varying_ity;

    virtual ~ToonShader() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));

        varying_ity[nthvert] = model->normal(iface, nthvert) * light_dir;

        gl_Vertex = Viewport * gl_Vertex;
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color) {
        float intensity = varying_ity * bar;
        if (intensity > .85) intensity = 1;
        else if (intensity > .60) intensity = .80;
        else if (intensity > .45) intensity = .60;
        else if (intensity > .30) intensity = .45;
        else if (intensity > .15) intensity = .30;
        color = TGAColor(255, 155, 0) * intensity;
        return false;
    }
};

//���Է��������в�ֵ����������Դ�������αߵĲ��
struct FlatShader : public IShader {
    //���������Ϣ
    mat<3, 3, float> varying_tri;

    virtual ~FlatShader() {}

    virtual Vec4f vertex(int iface, int nthvert) {
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));
        gl_Vertex = Projection * ModelView * gl_Vertex;
        varying_tri.set_col(nthvert, proj<3>(gl_Vertex / gl_Vertex[3]));
        gl_Vertex = Viewport * gl_Vertex;
        return gl_Vertex;
    }

    virtual bool fragment(Vec3f bar, TGAColor& color) {

        Vec3f n = cross(varying_tri.col(1) - varying_tri.col(0), varying_tri.col(2) - varying_tri.col(0)).normalize();
        float intensity = n * light_dir;
        color = TGAColor(255, 255, 255) * intensity;
        return false;
    }
};


int main(int argc, char** argv) {
    //����ģ��
    if (2==argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("obj/floor.obj");

    }
    //��ʼ���任����ͶӰ�����ӽǾ���
    lookat(eye, center, up);
    projection(-1.f/(eye-center).norm());
    viewport(width / 8, height / 8, width * 3 / 4, height * 3 / 4);
    light_dir.normalize();
    //��ʼ��image��zbuffer
    TGAImage image  (width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);
    //ʵ����flat��ɫ
    //FlatShader shader;
    //ʵ�����������ɫ
    //GouraudShader shader;
    //ʵ����Phong��ɫ
    PhongShader shader;
    //ʵ����Toon��ɫ
	//ToonShader shader;
    //BlingPhongShader shader;
    //��ģ������Ϊѭ��������
    for (int i=0; i<model->nfaces(); i++) {
        Vec4f screen_coords[3];
        for (int j=0; j<3; j++) {
            //ͨ��������ɫ����ȡģ�Ͷ���
            //�任�������굽��Ļ���꣨�ӽǾ���*ͶӰ����*�任����*v�� ***��ʵ��������������Ļ���꣬��Ϊû�г������һ������
            //�������ǿ��
            screen_coords[j] = shader.vertex(i, j);
        }
        //������3�����㣬һ�������ι�դ����ɣ����������Σ�triangle�ڲ�ͨ��ƬԪ��ɫ������������ɫ
        triangle(screen_coords, shader, image, zbuffer);
    }

    image.  flip_vertically();
    zbuffer.flip_vertically();
    image.  write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}