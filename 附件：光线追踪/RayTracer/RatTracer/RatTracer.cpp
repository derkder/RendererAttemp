#include "geometry.h"
#include <limits>
#include <fstream>
#include <vector>
#define M_PI       3.14159265358979323846   // pi

struct Light
{
    Light(const vec3& p, const float& i) : position(p), intensity(i) {}
    vec3 position;
    float intensity;
};

struct Material
{
    Material(const vec3& a, const vec3& color, const float& spec) : albedo(a), diffuse_color(color), specular_exponent(spec) {}
    Material() : albedo(), diffuse_color(), specular_exponent() {}
    vec3 albedo;//用于控制漫反射、高光、折射产生的颜色强度
    vec3 diffuse_color;
    float specular_exponent;
};

struct Sphere
{
    vec3 center;
    float radius;
    Material material;
    Sphere(const vec3& c, const float& r, const Material& m) : center(c), radius(r), material(m) {}
    bool ray_intersect(const vec3& orig, const vec3& dir, float& t0) const
    {
        vec3 L = center - orig;
        float tca = L * dir;
        float d2 = L * L - tca * tca;
        if (d2 > radius * radius)
            return false;
        float thc = sqrtf(radius * radius - d2);
        t0 = tca - thc;
        float t1 = tca + thc;
        if (t0 < 0) t0 = t1;//说明光源的出发点在球体内部
        if (t0 < 0) return false;
        return true;
    }
};

bool scene_intersect(const vec3& orig, const vec3& dir, const std::vector<Sphere>& spheres, vec3& hit, vec3& N, Material& material)//每个屏幕上的像素点跑一个
{
    float spheres_dist = std::numeric_limits<float>::max();//用于存储当前光线与场景中的球体最早相交的时间（这里时间等同于距离）
    for (size_t i = 0; i < spheres.size(); i++)//逐个球看有没有相交
    {
        float dist_i;
        if (spheres[i].ray_intersect(orig, dir, dist_i) && dist_i < spheres_dist)//有离摄像头更近的球了
        {
            spheres_dist = dist_i;
            //因为传入进来的是地址
            hit = orig + dir * dist_i;
            N = (hit - spheres[i].center).normalize();
            material = spheres[i].material;
        }
    }
    return spheres_dist < 1000;
}

vec3 reflect(const vec3& I, const vec3& N) {
    return  I - N * 2.f * (I * N);;
}

//每个屏幕上的像素点跑一个,加了depth的概念后，每个点要递归四层嵌套跑或者直到没有再反射到物体身上为止
//【整个函数的目的是求实现到屏幕上的一个点连接后，碰到的第一个球体上的点应该显示的颜色】
vec3 cast_ray(const vec3& orig, const vec3& dir, const std::vector<Sphere>& spheres, const std::vector<Light>& lights, size_t depth = 0)
{
    vec3 point, N;
    Material material;
    if (depth > 4 || !scene_intersect(orig, dir, spheres, point, N, material)) {
        return vec3{ 0.2, 0.7, 0.8 };
    }
    vec3 reflect_dir = reflect(dir, N).normalize();
    vec3 reflect_orig = point + N * 1e-3;
    vec3 reflect_color = cast_ray(reflect_orig, reflect_dir, spheres, lights, depth + 1);//递归，后面反射的点会贡献值给前面反射的点
    float diffuse_light_intensity = 0;
    float specular_light_intensity = 0;
    for (size_t i = 0; i < lights.size(); i++)
    {
        vec3 light_dir = (lights[i].position - point).normalize();
        float light_distance = (lights[i].position - point).norm(); // 光源与交点之间的距离
        vec3 shadow_orig = point + N * 1e-3; // 偏移交点防止与自己相交
        vec3 shadow_pt, shadow_N;
        Material tmpmaterial;
        // 阴影的实现就靠这一个判断：如果被遮挡则不进行该光源的光照计算，直接进行下一轮光源的计算
        if (scene_intersect(shadow_orig, light_dir, spheres, shadow_pt, shadow_N, tmpmaterial) && (shadow_pt - shadow_orig).norm() < light_distance)
            continue;
        diffuse_light_intensity += lights[i].intensity * std::max(0.f, light_dir * N);
        specular_light_intensity += powf(std::max(0.f, reflect(light_dir, N) * dir), material.specular_exponent) * lights[i].intensity;
    }
    return material.diffuse_color * diffuse_light_intensity * material.albedo[0] + vec3{ 1., 1., 1. } *specular_light_intensity * material.albedo[1] + reflect_color * material.albedo[2];
}


void render(const std::vector<Sphere>& spheres, const std::vector<Light>& lights)
{
    const int width = 1024;
    const int height = 768;
    const float fov = M_PI / 3.0;//说明视野是1/3个180度（pai）
    std::vector<vec3> framebuffer(width * height);
    for (size_t j = 0; j < height; j++)
    {
        for (size_t i = 0; i < width; i++)
        {
            //获得像素【一个平面上的】的世界坐标
            float x = (i + 0.5) - width / 2.;//0.5是因为取的是像素点的中心
            //同时，这里的0.5代表了忽略了像素是有面积的，相当于只对于当前像素的中心点进行了采样，这就导致了噪点。
            //路径追踪就是在这里的基础上，不止采样一个点，而是加蒙特卡洛找妥协点
            float y = -(j + 0.5) + height / 2.;
            float z = -height / (2. * tan(fov / 2.));//固定的，因为是在一个屏幕上
            vec3 dir = vec3{ x, y, z }.normalize();
            framebuffer[i + j * width] = cast_ray(vec3{ 0, 0, 0 }, dir, spheres, lights);
        }
    }
    std::ofstream ofs;
    ofs.open("./outPureSphereImage.ppm", std::ios::binary);
    ofs << "P6\n"
        << width << " " << height << "\n255\n";
    for (size_t i = 0; i < height * width; ++i) {
        vec3& c = framebuffer[i];
        float max = std::max(c[0], std::max(c[1], c[2]));
        if (max > 1) c = c * (1. / max); // 如果超过最大值需要缩小颜色
        for (size_t j = 0; j < 3; j++) {
            ofs << (char)(255 * std::max(0.f, std::min(1.f, framebuffer[i][j])));
        }
    }
}


int main() {
    Material purpel_material(vec3{ 0.4, 0.3, 0.3 }, vec3{ 0.58, 0.44, 0.86 }, 50);
    Material red_material(vec3{ 0.3, 0.1, 0.1 }, vec3{ 1.0, 0.42, 0.42 }, 10);
    Material mirror(vec3{ 0.0, 10.0, 0.8 }, vec3{ 1.0, 1.0, 1.0 }, 1425);
    std::vector<Sphere> spheres;
    spheres.push_back(Sphere(vec3{ -3,    0,   -16 }, 2, purpel_material));
    spheres.push_back(Sphere(vec3{ -1.0, -1.5, -12 }, 2, mirror));
    spheres.push_back(Sphere(vec3{ 1.5, -0.5, -18 }, 3, red_material));
    spheres.push_back(Sphere(vec3{ 7,    5,   -18 }, 4, mirror));

    std::vector<Light>  lights;
    lights.push_back(Light(vec3{ -20, 20,  20 }, 1.5));
    lights.push_back(Light(vec3{ 30, 50, -25 }, 1.8));
    lights.push_back(Light(vec3{ 30, 20,  30 }, 1.7));

    render(spheres, lights);
    return 0;
}