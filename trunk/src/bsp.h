//
//      BSP
//  (c) Stan Melax 1998-2007
//  
// BSPs? really? are they still useful?  
// Clearly, Spatial structures are still necessary to implement anything useful in a 3D 
// interactive application.  
// Admittedly overlapping hierarchies and polygon soup approaches are easier to implement.
// An advantage with BSP is the ability to do interesting things  
// from just knowing if a point xyz is in empty space or inside a solid volume all the way to 
// operations such as computational solid geometry (intersections and unions).
// 
// 

#ifndef SMBSP_H
#define SMBSP_H


#include "vecmath.h"
#include "array.h"

#define COPLANAR   (0)
#define UNDER      (1)
#define OVER       (2)
#define SPLIT      (OVER|UNDER)
#define PAPERWIDTH (0.0001f)

class Face;
class Convex;
class WingMesh;
class Collidable;


class BSPNode :public Plane 
{
  public:
	BSPNode *		under;
	BSPNode *		over;
	int				isleaf;
	int				flag;  // using this for GC
	WingMesh *		convex;
	Array<Face *>	brep;
					BSPNode(const Plane &p);
					BSPNode(const float3 &n=float3(0,0,0),float d=0);
					~BSPNode();
};


class Face : public Plane 
{
  public:
	int				matid;
	Array<float3>	vertex;
	float3			gu;
	float3			gv;
	float3			ot;
	Face() {matid=0;}
};




class BBox
{
public:
	float3 bmin;
	float3 bmax;
};


#define CREASE_ANGLE_DEFAULT (30)

int      PlaneTest(float3 normal,float dist,float3 v,float epsilon=PAPERWIDTH);
int      PlaneTest(const Plane &p, const float3 &v,float epsilon=PAPERWIDTH);

BBox     VertexExtremes(const Array<Face*> &faces);
Face *   FaceDup(Face *face);
Face *   FaceClip(Face *face,const Plane &clip);
float     FaceArea(Face *face);
float3    FaceCenter(Face *face);
int	     FaceSplitTest(Face *face,float3 splitnormal,float splitdist,float epsilon=PAPERWIDTH);
void     FaceSliceEdge(Face *face,int edge,BSPNode *n);
void     FaceEmbed(BSPNode *node,Face *face);
void     FaceExtractMatVals(Face *face,const float3 &v0,const float3 &v1,const float3 &v2,const float2 &t0,const float2 &t1,const float2 &t2);
void     FaceTranslate(Array<Face *> &faces,const float3 &offset);
void     FaceTranslate(Face *face,const float3 &offset);
void     FaceRotate(Face *face,const Quaternion &r);
int      FaceClosestEdge(Face *face,const float3 &sample_point);
Face *   FaceNewQuad(const float3 &v0,const float3 &v1,const float3 &v2,const float3 &v3);
Face *   FaceNewTri(const float3 &v0,const float3 &v1,const float3 &v2);
Face *   FaceNewTriTex(const float3 &v0,const float3 &v1,const float3 &v2,const float2 &t0,const float2 &t1,const float2 &t2);
float2   FaceTexCoord(Face *f,int i);  // uv texture coords of i'th vertex
float2   FaceTexCoord(Face *f,const float3 &v); // uv texture coord of point v on face
int      FaceSplitifyEdges(BSPNode *root);

void     AssignTex(BSPNode *node,int matid=0);
void     AssignTex(Face* face);



BSPNode *BSPCompile(Array<Face *> &inputfaces,WingMesh *convex_space,int side=0); 
void     BSPMakeBrep(BSPNode *r,Array<Face*> &faces);
void     BSPDeriveConvex(BSPNode *node,WingMesh *convex);
BSPNode* BSPDup(BSPNode *n);
void     BSPTranslate(BSPNode *n,const float3 &offset);
void     BSPRotate(BSPNode *n,const Quaternion &r);
void     BSPScale(BSPNode *n,float s);
void     BSPFreeBrep(BSPNode *);
void     BSPRipBrep(BSPNode *r,Array<Face*> &faces);
void     BSPGetBrep(BSPNode *r,Array<Face*> &faces);
void     BSPMakeBrep(BSPNode *);
BSPNode *BSPUnion(BSPNode *a,BSPNode *b);
BSPNode *BSPIntersect(BSPNode *a,BSPNode *b);
void     NegateTree(BSPNode *n);
int      HitCheck(BSPNode *node,int solid,float3 v0,float3 v1,float3 *impact);
int      HitCheckSolidReEnter(BSPNode *node,float3 v0,float3 v1,float3 *impact); // wont just return v0 if you happen to start in solid
int      HitCheckCylinder(float r,float h,BSPNode *node,int solid,float3 v0,float3 v1,float3 *impact,const float3 &nv0); 
int      HitCheckConvex(Array<float3> &verts,BSPNode *node,int solid,
				   float3 v0,float3 v1,float3 *impact,
				   const Quaternion &q0,const Quaternion &q1,Quaternion *impactq,const float3 &nv0,const int vrtv0);
int	     HitCheckConvexGJK(const Collidable *collidable, BSPNode *bsp);
int      HitCheckSphere(float r,BSPNode *node,int solid,float3 v0,float3 v1,float3 *impact,const float3 &nv0);
int	     ProximityCells(const Collidable *collidable, BSPNode *bsp,Array<WingMesh*> &cells_out,float padding=0.0f);
int      ConvexHitCheck(WingMesh *convex,float3 v0,float3 v1,float3 *impact); 
void     BSPGetSolids(BSPNode *n,Array<WingMesh*> &meshes);
void     BSPPartition(BSPNode *n,const Plane &p,BSPNode * &nodeunder, BSPNode * &nodeover);
BSPNode *BSPClean(BSPNode *n); 
int      BSPCount(BSPNode *n);
int      BSPFinite(BSPNode *bsp);
inline int maxdir(const Array<float3> &array,const float3 &dir) {return maxdir(array.element,array.count,dir);}


#endif