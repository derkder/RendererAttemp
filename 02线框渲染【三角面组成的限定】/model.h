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


///用文本文档打开.obj文件
///[v 0.1 0.2 0.3] 表示了一个顶点的世界坐标(x,y,z)
/// [f 24/1/24 25/2/25 26/3/26 ]表示了模型中的一个三角面，所有的f都有三组数据，
///								每一组的第一个数据保存的就是顶点的信息，即该面的三个点分别是序号为24，25和26的这三个点（点的坐标信息已经存在上面的v里了）