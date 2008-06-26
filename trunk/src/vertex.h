//
//
//
// A model is a collection of meshes with an optional skeleton aka bone collection.
// Each mesh is associated with a single material.  So something has to be split into 
// multiple meshes to use different materials or textures.


#ifndef SM_VERTEX_H
#define SM_VERTEX_H

#include "vecmath.h"
#include "array.h"
#include "object.h" // so bones can be objects and I can test them directly - this should eventually be removed.
#include "wingmesh.h"

class Vertex
{
  public:
	float3  position;
	float3  normal;
	float2 texcoord;	
	float3  tangent;
	float3  binormal;
	Vertex(){}
	Vertex(const float3& _pos,const float3 &_nrm, const float2 &_tex, const float3 &_tng, const float3 &_bnm)
		:position(_pos),normal(_nrm),texcoord(_tex),tangent(_tng),binormal(_bnm){}
	static char *semantic() { return "position normal texcoord tangent binormal";}
};



class VertexS
{
  public:
	float3  position;
	float4  weights;
	byte4   bones;
	float3  normal;
	byte4   color;
	float2  texcoord;	
	float3  tangent;
	float3  binormal;
	VertexS(){}
	static char *semantic() { return "position blendweights blendindices normal color texcoord tangent binormal";}
};


class VertexPN  // position and normal
{
  public:
	float3  position;
	float3  normal; 
	VertexPN(){}
	VertexPN(const float3& _position,const float3 &_normal)
		:position(_position),normal(_normal){}
	static char *semantic() { return "position normal";}
};


class VertexPC  // position and color
{
  public:
	float3  position;
	unsigned long color;  // ARGB order, consistent with DWORD,D3DCOLOR,D3DFMT_A8R8G8B8 
	float2 texcoord;
	VertexPC(){}
	VertexPC(const float3& _pos,unsigned long _color,float2 _texcoord=float2(0,0))
		:position(_pos),color (_color),texcoord(_texcoord){}
	static char *semantic() { return "position color texcoord";}
};

//
class MeshBase
{
public:
	virtual int GetMat()=0;
	virtual const float3 &Bmin()=0; // assumed to be local to the mesh i.e. in model space, not world
	virtual const float3 &Bmax()=0;
	virtual const float4x4 &GetWorld()=0;  // plan to make this unnecessary
};


class Model;

class DataMesh : public MeshBase
{
public:
	int matid;
	Array<Vertex> vertices;
	Array<short3> tris;
	int   vertexnum() { return vertices.count; }
	int   stride()    {return sizeof(Vertex);}
	void *vertexdata(){ return vertices.element; }
	void *ripvertexdata() { void *v=vertices.element;vertices.element=NULL;vertices.count=0;return v;}
	char *semantic()  { return vertices.element->semantic(); }
	Model *model;
	int GetMat() {return matid;}
	const float4x4 &GetWorld();
	virtual const float3 &Bmin();  // assumed to be local to the mesh i.e. in model space, not world
	virtual const float3 &Bmax();
	DataMesh():matid(0),model(NULL){}
	~DataMesh(){}
};




class MeshHW;
class Model;

MeshHW *DynamicCastMeshHW(MeshBase*);


class KeyFrame
{
public:
	float time;
	Pose  pose;
};

class Bone : public Object, public Pose
{
public:
	Pose basepose;
	Pose modelpose;
	Array<KeyFrame> animation;
	Bone *parent;
	Model *model;
	WingMesh* geometry;
	Bone(const char *_name,Model *_model,Bone *_parent=NULL);
	~Bone();
};

class Model
{
  public:
	Array<MeshHW *> meshes;
	Array<DataMesh*> datameshes;
	float3 bmin,bmax; // local
	float4x4 modelmatrix;  // also known as "world" matrix in direct3d-speak - converts object local to world space
	Array<float4x4> currentpose;  // 
	Array<Bone*> skeleton;
	Model();
	~Model();
};


class Kobbelt
{
public:
	const float3 *pos;
	const int w,h;
	int subdiv;
	int wireframe;
	float3 color;
	float  texscale; // for tiling textures
	Model *model;
	DataMesh  *mesh;
	int &matid;
	Kobbelt(const float3 *_pos,int _w,int _h,int _s);
	~Kobbelt(){delete model;}
	void Update();
	void Draw();
  private:
	void BuildMesh();
};

inline 	const float4x4 &DataMesh::GetWorld()
{
	assert(model);
	return model->modelmatrix;
}
inline const float3 &DataMesh::Bmin(){assert(model);return model->bmin;}
inline const float3 &DataMesh::Bmax(){assert(model);return model->bmax;}


#endif

