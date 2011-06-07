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
	float4  orientation;  // tangent space basis in quat form
	float2  texcoord;	
	float3  tangent;
	float3  binormal;
	float3  normal;
	unsigned long color;  // ARGB order, consistent with DWORD,D3DCOLOR,D3DFMT_A8R8G8B8 
	Vertex(){}
	Vertex(const float3& _pos,const float4 &_orn, const float2 &_tex)
		:position(_pos),orientation(_orn),texcoord(_tex),color(0xFFFFFFFF){}
	static char *semantic() { return "position orientation texcoord";}
};


class VertexS
{
  public:
	float3  position;
	float4  orientation;  // tangent space basis in quat form
	float2  texcoord;	
	byte4   bones;
	float4  weights;
	VertexS(){}
	static char *semantic() { return "position orientation texcoord bones weights";}
};


struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct IDirect3DVertexDeclaration9;

class Model;

class DataMesh 
{
public:
	int matid;
	Array<Vertex> vertices;
	VertexS *sverts;
	Array<short3> tris;
	int   vertexnum() { return vertices.count; }
	int   stride()    {return sizeof(Vertex);}
	void *vertexdata(){ return vertices.element; }
	void *ripvertexdata() { void *v=vertices.element;vertices.element=NULL;vertices.count=0;return v;}
	char *semantic()  { return vertices.element->semantic(); }
	Model *model;
	int GetMat() {return matid;}
	const Pose &GetWorld();
	virtual const float3 &Bmin();  // assumed to be local to the mesh i.e. in model space, not world
	virtual const float3 &Bmax();
	int manifold;
	IDirect3DVertexBuffer9 *vb;  // for d3d gpu cacheing
	IDirect3DIndexBuffer9  *ib;
	IDirect3DVertexDeclaration9 *vdecl;

	DataMesh():matid(0),model(NULL),sverts(NULL),manifold(0),vb(NULL),ib(NULL),vdecl(NULL){}
	~DataMesh();
};




class Model;

class KeyFrame
{
public:
	float time;
	Pose  pose;
};

class Bone :  public Pose
{
public:
	Pose basepose;
	Pose modelpose;
	Array<KeyFrame> animation;
	Bone *parent;
	Model *model;
	WingMesh* geometry;
	String id;
	Bone(const char *_name,Model *_model,Bone *_parent=NULL);
	~Bone();
};

class Model : public Pose
{
  public:
	Array<DataMesh*> datameshes;
	float3 bmin,bmax; // local
	Array<float4x4> currentpose;  // 
	Array<float3> currentposep;
	Array<Quaternion> currentposeq;
	Array<Bone*> skeleton;
	Model();
	~Model();
};


class Kobbelt
{
public:
	const float4 *pos4;
	const float3 *pos3;
	const float* pos;
	int stride_inner,stride_outer;
	const int w,h;
	int subdiv;
	int wireframe;
	float3 color;
	float  texscale; // for tiling textures
	Model *model;
	DataMesh  *mesh;
	DataMesh  *shadow;
	int &matid;
	Kobbelt(const float4 *_pos,int _w,int _h,int _s);
	Kobbelt(const float3 *_pos,int _w,int _h,int _s);
	Kobbelt(const float  *_pos,int _stride_inner, int _stride_outer,int _w,int _h,int _s);
	~Kobbelt(){delete model;}
	void Update();
	void Draw();
  private:
	void BuildMesh();
};

inline 	const Pose &DataMesh::GetWorld()
{
	assert(model);
	return (Pose&) (*model) ;
}
inline const float3 &DataMesh::Bmin(){assert(model);return model->bmin;}
inline const float3 &DataMesh::Bmax(){assert(model);return model->bmax;}

void BoxLimits(const Vertex *verticies,int count,float3 &bmin,float3 &bmax,int reset_prior_limits=1);
template<class T> inline void BoxLimits(Array<T> &a,float3 &bmin,float3 &bmax,int reset_prior_limits=1){return BoxLimits(a.element,a.count,bmin,bmax,reset_prior_limits);}

#endif

