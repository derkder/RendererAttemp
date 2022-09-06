#ifndef __MODEL_H__
#define __MODEL_H__

#include <vector>
#include "geometry.h"

class Model {
private:
	std::vector<Vec3f> verts_;
	std::vector<std::vector<int> > faces_;
public:
	Model(const char *filename);
	~Model();
	int nverts();
	int nfaces();
	Vec3f vert(int i);
	std::vector<int> face(int idx);
};

#endif //__MODEL_H__


///���ı��ĵ���.obj�ļ�
///[v 0.1 0.2 0.3] ��ʾ��һ���������������(x,y,z)
/// [f 24/1/24 25/2/25 26/3/26 ]��ʾ��ģ���е�һ�������棬���е�f�����������ݣ�
///								ÿһ��ĵ�һ�����ݱ���ľ��Ƕ������Ϣ���������������ֱ������Ϊ24��25��26���������㣨���������Ϣ�Ѿ����������v���ˣ�