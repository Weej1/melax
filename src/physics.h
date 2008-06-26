#ifndef PHYSICS_H
#define PHYSICS_H


#include "vecmath.h"
#include "array.h"
#include "bsp.h"
#include "wingmesh.h" 

class State;
class Contact;
class Shape;
class RigidBody;
class Spring;

class State : public Pose
{
  public:
					State(const float3 &p,const Quaternion &q,const float3 &v,const float3 &r):Pose(p,q),momentum(v),rotation(r){}
					State(){};
	float3			momentum;  
	float3			rotation;  // angular momentum
	State&          state(){return *this;}
	const State&    state() const {return *this;}
};

class BodyContact
{
public:
	Collidable *a;
	Collidable *b;
	Contact c[5];
	int count;
	float3 tangent;
	float3 binormal;
	BodyContact():a(NULL),b(NULL),count(0){}
	BodyContact(Collidable *_b):a(NULL),b(_b),count(0){}
	BodyContact(Collidable *_a,Collidable *_b):a(_a),b(_b),count(0){}
};




class RigidBody : public State , public Collidable
{
  public:
	float			mass;
	float			massinv;  
	float3x3		tensor;  // inertia tensor
	float3x3		tensorinv;  
	float3x3		Iinv;    // Inverse Inertia Tensor rotated into world space 
	float3			spin;     // often called Omega in most references
	float			radius;
	float3          bmin,bmax; // in local space
	float3			position_next;
	Quaternion		orientation_next;
	float3			position_old;  // used by penetration rollback
	Quaternion		orientation_old;  // used by penetration rollback
	float           damping;
	float			friction;  // friction multiplier
					RigidBody(WingMesh *_geometry,const Array<Face *> &_faces,const float3 &_position);
					RigidBody(BSPNode *_bsp,const Array<Face *> &_faces,const float3 &_position);
					~RigidBody();
	WingMesh *		local_geometry;
	WingMesh *		world_geometry;
	Array<Face *>	faces;
	float			hittime;
	int				rest;
	State			old;
	int				collide;  // collide&1 body to world  collide&2 body to body
	int				resolve;
	int				usesound;
	float3          com; // computed in constructor, but geometry gets translated initially 
	float3          PositionUser() {return pose()* -com; }  // based on original origin
	Array<Spring*>	springs;
	Array<BodyContact>	contacts;
	virtual int		Support(const float3& dir)const{return local_geometry->Support(rotate(Inverse(orientation),dir)); } // return world_geometry->Support(dir); }
	virtual float3	GetVert(int v)const {return position + rotate(orientation,local_geometry->GetVert(v));} // world_geometry->GetVert(v);}
	Array<Shape*>   shapes;
	Array<RigidBody*> ignore; // things to ignore during collision checks
};


class Shape : public Collidable
{
  public:
	// idea is for rigidbody to have an array of these.
	// all meshes are stored in the coord system of the rigidbody, no additional shape transform.
	RigidBody *rb;
	const float3 &Position() const {return rb->position;}
	const Quaternion &Orientation() const {return rb->orientation;}
	const Pose& pose() const {return rb->pose();}
	WingMesh *local_geometry;
	Array<BodyContact>	contacts;
	virtual int		Support(const float3& dir)const{return local_geometry->Support(rotate(Inverse(rb->orientation),dir)); } // return world_geometry->Support(dir); }
	virtual float3	GetVert(int v)const {return rb->position + rotate(rb->orientation, local_geometry->GetVert(v));} // world_geometry->GetVert(v);}
	Shape(RigidBody *_rb,WingMesh *_geometry);
	~Shape();
};

 
class Spring 
{
  public:
	RigidBody *		bodyA;
	float3			anchorA;
	RigidBody *		bodyB;
	float3			anchorB;
	float			k; // spring constant
};

void rbscalemass(RigidBody *rb,float s);  // scales the mass and all relevant inertial properties by multiplier s
Spring *   CreateSpring(RigidBody *a,float3 av,RigidBody *b,float3 bv,float k);
void       DeleteSpring(Spring *s);
RigidBody *FindBody(char *name);
void PutBodyToRest(RigidBody *rb);




#endif

