//
//   WingMesh class 
//  (c) stan melax 2007
//
//
// A mesh class designed to support many general mesh operations and be efficient.
// This class uses halfedge structures and is designed to work with manifold meshes.
// The eventual goal is to consolidate all the different structures and functions currently in use.
//
// Many winged mesh implementations make excessive use of new and delete resulting
// in a rat's nest of pointers and bad memory fragmentation.
// The idea is to use dynamic arrays for storing vertices, edges and faces.
// Obviously, indices are used instead of pointers.  
// Pointers to faces,edges,or vertices are discouraged since these can become invalid as
// elements are added to the array which might cause a reallocation.
//
// The wingmesh structure can be repacked/compressed to fill in space within the arrays.
// Care must be taken since any references (by index) to elements of the winged mesh from
// places outside of the class may become incorrect or invalid after packing.
// Therefore, depending on the application, it may be better to repacked the mesh 
// in constant time immediately after each operation, or wait and repack the
// entire mesh in linear time after a mesh algorithm has completed.
//
// Programming can be especially tricky using integer indices instead of pointers which 
// offer greater safety due to type checking.  For example, there's nothing to safeguard 
// the programmer against using the id of an edge when indexing the vertex list.
//
//
//  polymesh
//
// A basic polymesh class that matches the semantic used by maya, xsi, and collada. 
// Whereas vrml uses -1 to delimit faces within the index list, the preferred standard
// in the DCC community is to use another list with number-of-face elements that indicate
// the number of sides of each face.
//
// The Polymesh data structure isn't very useful for mesh computation and is
// only meant to provide a simple standard means to exchange polymesh information.
// Passing PolyMesh or returning it from a function only does a shallow copy.
// That is to make it convenient for returning a polymesh from a function.
// That design decision could be revisited.
//
#ifndef WINGMESH_H
#define WINGMESH_H

#include <assert.h>
#include "vecmath.h"
#include "array.h"

class WingMesh;
class Face; // see bsp.h 

class Collidable
{
 public:
	virtual int    Support(const float3& dir) const =0;  // returns index of support map vertex furthest in the supplied direction
	virtual float3 GetVert(int index) const =0;
};

class LimitLinear;
class Contact
{
public:
	Collidable*		C[2];
	int				type; //  v[t] on c[0] for t<type on c[1] for t>=type so 2 is edge-edge collision
	int				v[4];
	float3			normal; // worldspace
	float			dist;  // plane
	float3			impact; 
	float			separation;
	float3 n0,n1,p0,p1;
	float3 n0w,n1w,p0w,p1w;
	float time;
	float totalimpulse;
	float tangentimpulse;
	float binormalimpulse;
	float targvel;
	float bouncevel;
	LimitLinear * limits[3];
	Contact():normal(0,0,0),impact(0,0,0) {C[0]=C[1]=NULL;limits[0]=limits[1]=limits[2]=NULL;type=-1;totalimpulse=tangentimpulse=binormalimpulse=targvel=0.0f;}
};

int Separated(const Collidable *A, const Collidable *B,Contact &hitinfo,int findclosest);


class WingMesh: public Collidable
{
  public:
	class HalfEdge
	{
	  public:
		short id;
		short v;
		short adj;
		short next;
		short prev;
		short face;
		HalfEdge(){id=v=adj=next=prev=face=-1;}
		HalfEdge(short _id,short _v,short _adj,short _next,short _prev,short _face):id(_id),v(_v),adj(_adj),next(_next),prev(_prev),face(_face){}

		HalfEdge *Next(){assert(next>=0 && id>=0); return this+(next-id);}  // convenience for getting next halfedge 
		HalfEdge *Prev(){assert(prev>=0 && id>=0); return this+(prev-id);}
		HalfEdge *Adj (){assert(adj >=0 && id>=0); return this+(adj -id);}
	};
	Array<HalfEdge> edges;
	Array<float3> verts;
	Array<Plane>  faces;
	Array<short>  vback;
	Array<short>  fback;
	int unpacked; // flag indicating if any unused elements within arrays
	WingMesh():unpacked(0){}
	virtual int		Support(const float3& dir)const {return maxdir(verts.element,verts.count,dir);}  
	virtual float3	GetVert(int v)const{return verts[v];}
};


WingMesh* Dual(WingMesh *m,float r=1.0f,const float3 &p=float3(0,0,0));
WingMesh* WingMeshCreate(float3 *verts,int3 *tris,int n);
WingMesh* WingMeshCreate(float3 *verts,int3 *tris,int n,int *hidden_edges,int hidden_edges_count);
void      WingMeshDelete(WingMesh *m); 
WingMesh *WingMeshDup(WingMesh *src);
WingMesh *WingMeshCube(const float3 &bmin,const float3 &bmax);
WingMesh *WingMeshCrop(WingMesh *_m,const Plane &slice);
int       WingMeshSplitTest(const WingMesh *m,const Plane &plane);
void      WingMeshTranslate(WingMesh *m,const float3 &offset);
void      WingMeshRotate(WingMesh *m,const Quaternion &rot);
float     WingMeshVolume(WingMesh *m);

int       WingMeshTris(const WingMesh *m,Array<int3> &tris_out);  // generates a list of indexed triangles from the wingmesh's faces

float3x3  Inertia(const Array<WingMesh*> &meshes, const float3& com=float3(0,0,0));
float3    CenterOfMass(const Array<WingMesh*> &meshes);
float     Volume(const Array<WingMesh*> &meshes);

int       WingMeshToFaces(WingMesh *m,Array<Face*> &faces);



class PolyMesh
{
public:
	float3 *verts;
	int verts_count;
	int *indices;
	int indices_count;
	int *sides;
	int sides_count;
};

void PolyMeshFree(PolyMesh &pm); // frees space allocated
void PolyMeshAlloc(PolyMesh &pm,int _verts_count,int _indices_count, int _sides_count); // just allocates space 

PolyMesh  WingMeshToPolyData(WingMesh *m);



#endif
