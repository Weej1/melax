
#ifndef SM_CHUCKABLE_H
#define SM_CHUCKABLE_H

#include "object.h"
#include "bsp.h"
// need model;
#include "mesh.h"
#include "physics.h"

class Chuckable : public Entity , public RigidBody
{
public:
	Chuckable(const char *name,WingMesh *_geometry,const Array<Face *> &_faces,const float3 &_position);
	Chuckable(const char *name,BSPNode  *_bsp,const Array<Face *> &_faces,const float3 &_position,int _shadowcast);
	String message; // can be used like a tooltip on what to do with this object
	int				render;
	//float3 position;
	//Quaternion orientation;
	~Chuckable();
	int HitCheck(const float3 &_v0,const float3 &_v1,float3 *impact);
	Model *model;
	int submerged;
};
extern Array<Chuckable*> Chuckables;





#endif
