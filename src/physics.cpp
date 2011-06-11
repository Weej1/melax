//
// Impulse Physically Based Modelling
//
// attempted to play with positioncorrection
// still needs tweaking

#include <stdio.h>
#include <float.h>
#include "vecmath.h"
#include "bsp.h"
#include "physics.h"
#include "wingmesh.h"
#include "console.h"
#include "camera.h"


float   physics_deltaT=0.015f;
EXPORTVAR(physics_deltaT);
int physics_positioncorrection =0;
EXPORTVAR(physics_positioncorrection);

//extern float   GlobalTime;
//extern float3  MouseVector;

//extern int HitCheckVertexHit;
//extern BSPNode *area_bsp;
static Array<BSPNode*> area_bsps;
int GetEnvBSPs(Array<BSPNode*>& bsps);
extern float3 HitCheckImpactNormal;



float  physics_restitution=0.4f;  // coefficient of restitution
extern float3 gravity;
float  physics_coloumb=0.6f; // used as default friction value and for static geometry    
EXPORTVAR(physics_restitution);
EXPORTVAR(physics_coloumb);  // between 0 and 1



int physics_warmstart=1;
EXPORTVAR(physics_warmstart);
int physics_warmstartjoint=0;
EXPORTVAR(physics_warmstartjoint);

float physics_biasfactorjoint = 0.3f;
float physics_biasfactorpositive = 0.3f;
float physics_biasfactornegative = 0.3f;
EXPORTVAR(physics_biasfactorjoint);
EXPORTVAR(physics_biasfactorpositive);
EXPORTVAR(physics_biasfactornegative);
float physics_falltime_to_ballistic=0.2f;
EXPORTVAR(physics_falltime_to_ballistic);

// global list of physics objects
Array<RigidBody *> rigidbodies;





Quaternion DiffQ(const Quaternion &orientation,const float3x3 &tensorinv,const float3 &angular)
{
	Quaternion s_orientation_normed = normalize(orientation);
	float3x3 s_orientation_matrix = s_orientation_normed.getmatrix();
	float3x3 Iinv = Transpose(s_orientation_matrix) * tensorinv * s_orientation_matrix;
	float3 halfspin = (Iinv * angular) * 0.5f;
	return Quaternion(halfspin.x,halfspin.y,halfspin.z,0)*s_orientation_normed;
}

Quaternion rkupdateq(const Quaternion &s,const float3x3 &tensorinv,const float3 &angular,float dt)
{
	Quaternion d1 = DiffQ(s ,tensorinv,angular);
	Quaternion s2(s+d1*(dt/2));

	Quaternion d2 = DiffQ(s2,tensorinv,angular);
	Quaternion s3(s+d2*(dt/2));

	Quaternion d3 = DiffQ(s3,tensorinv,angular);
	Quaternion s4(s+d3*(dt));

	Quaternion d4 = DiffQ(s4,tensorinv,angular);
	return normalize( s + d1*(dt/6) + d2*(dt/3) + d3*(dt/3) + d4*(dt/6)) ;  
}


Spring *CreateSpring(RigidBody *a,float3 av,RigidBody *b,float3 bv,float k) {
	Spring *s  = new Spring();
	s->bodyA   = a;
	s->anchorA = av;
	s->bodyB   = b;
	s->anchorB = bv;
	s->k       = k;
	if(a)a->springs.Add(s);
	if(b)b->springs.Add(s);
	return s;
}
void DeleteSpring(Spring *s) {
	if(s->bodyA && s->bodyA->springs.Contains(s)) {
		s->bodyA->springs.Remove(s);
	}
	if(s->bodyB && s->bodyB->springs.Contains(s)) {
		s->bodyB->springs.Remove(s);
	}
	delete s;
}
class Limit
{
public:
	RigidBody *rb0;
	RigidBody *rb1;
	int    active;
	int    positioncorrection;
	//float3 position0;
	//float3 position1;
	Limit(RigidBody *_rb0,RigidBody *_rb1);
	virtual ~Limit();
	virtual void PreIter()=0;
	virtual void WarmStart(){}
	virtual void Iter()=0;
	virtual void PostIter()=0;
	virtual void Finalize(){}
	virtual void PositionCorrection(){}
};
class LimitAngular : public Limit
{
public:
	float  limitangle;
	float3 axis;  // world-space
	float  torque;  // rotational impulse
	float  targetspin;
	float  maxtorque;
	int    equality;  // flag for whether <= or == i.e. two-sided instead of just one-sided
	LimitAngular(RigidBody *_rb0,RigidBody *_rb1=NULL,const float3 &_axis=float3(0,0,1),float _targetspin=0.0f,float _maxtorque=FLT_MAX);
	virtual void PreIter(){};
	virtual void Iter();
	virtual void PostIter(){targetspin=(equality)?0:Min(targetspin,0.0f);}  // not zero since its ok to let it fall to the limit;
};
class LimitLinear : public Limit
{
 public:
	float3 position0;  // position of contact on rb0 in rb0 local space
	float3 position1;  // position of contact on rb1 in rb1 local space
	float3 normal;     // direction in world in which limit is applied
	float  targetspeed; 
	float  targetspeednobias;
	float  maxforce;
	int    equality; // flag for whether <= or == i.e. two-sided instead of just one-sided
	float  impulsesum;
	float  impulseprev;
	virtual void PreIter();
	virtual void WarmStart();
	virtual void Iter();
	virtual void PostIter();
	virtual void Finalize();
	virtual void PositionCorrection();
	LimitLinear(RigidBody *_rb0,RigidBody *_rb1,const float3 &_position0,const float3 &_position1,
	            const float3 &_normal=float3(0,0,1),float _targetspeed=0.0f,float _targetspeednobias=0.0f,float _maxforce=FLT_MAX);
};

class LimitFriction : public LimitLinear
{
public:
	LimitLinear *master;
	float friction;
	LimitFriction( LimitLinear *_master, const float3 &_normal=float3(0,0,1));
	virtual void Iter();
};

LimitFriction::LimitFriction(LimitLinear *m,
                             const float3 &_normal
							)   : LimitLinear(m->rb0,m->rb1,m->position0,m->position1,_normal)
{
	master=m;
	targetspeed=0.0f;
	maxforce=0.0f;
	friction = ((rb0)?rb0->friction : physics_coloumb ) * ((rb1)? rb1->friction : physics_coloumb);
}

void LimitFriction::Iter()
{
	maxforce = friction * master->impulsesum / physics_deltaT;
	LimitLinear::Iter();
}

class LimitAxis : public Limit
{
public:
	float  limitangle;
	float3 normal0;
	float3 normal1;
	float3 axis0;
	float3 axis;  // world-space
	float  torque;  // rotational impulse
	float  targetspin;
	float  maxtorque;
	int    equality;  // flag for whether <= or == i.e. two-sided instead of just one-sided
	LimitAxis(RigidBody *_rb,float _limitangle=DegToRad(0.0f));
	virtual void PreIter();
	virtual void Iter();
	virtual void PostIter();
};
Array<Limit*>Limits;
Limit::Limit(RigidBody *_rb0,RigidBody *_rb1):rb0(_rb0),rb1(_rb1)
{
	active=1;
	positioncorrection=0;
	Limits.Add(this);
}
Limit::~Limit()
{
	Limits.Remove(this);
}
LimitAngular::LimitAngular(RigidBody *_rb0,RigidBody *_rb1,const float3 &_axis,float _targetspin,float _maxtorque):Limit(_rb0,_rb1),axis(_axis),targetspin(_targetspin),maxtorque(_maxtorque)
{
	equality = 1;
	torque=0.0f;
}
void LimitAngular::Iter()
{
	if(!active) return;
	if(targetspin==-FLT_MAX) return;
	float currentspin = ((rb1)?dot(rb1->spin,axis):0.0f) - ((rb0)?dot(rb0->spin,axis):0.0f) ;  // how we are rotating about the axis 'normal' we are dealing with
	float dspin = targetspin - currentspin;  // the amount of spin we have to add to satisfy the limit.
	float spintotorque = 1.0f / (  ((rb0)?dot(axis,rb0->Iinv*axis):0.0f) + ((rb1)?dot(axis,rb1->Iinv*axis):0.0f)  );
	float dtorque = dspin * spintotorque ;  // how we have to change the angular impulse
	if(!equality) dtorque = Max(dtorque,-torque);  // one-sided limit total torque cannot go below 0
	dtorque = Min(dtorque,maxtorque*physics_deltaT-torque); // total torque cannot exceed maxtorque
	dtorque = Max(dtorque,-maxtorque*physics_deltaT-torque); // total torque cannot fall below -maxtorque
	if(rb0)
	{
		rb0->rotation += axis*-dtorque;  // apply impulse
		rb0->spin = rb0->Iinv * rb0->rotation; // recompute auxilary variable spin
	}
	if(rb1)
	{
		rb1->rotation += axis*dtorque;  // apply impulse
		rb1->spin = rb1->Iinv * rb1->rotation; // recompute auxilary variable spin
	}
	torque += dtorque;
}

class LimitCone : public Limit
{
public:
	float  limitangle;
	float3 normal0;
	float3 normal1;
	float3 axis;  // world-space
	float  torque;  // rotational impulse
	float  targetspin;
	float  maxtorque;
	int    equality;  // flag for whether <= or == i.e. two-sided instead of just one-sided
	LimitCone(RigidBody *_rb,float _limitangle=DegToRad(10.0f));
	virtual void PreIter();
	virtual void Iter();
	virtual void PostIter();
};

LimitCone::LimitCone(RigidBody *_rb,float _limitangle):Limit(NULL,_rb),normal0(0.0f,0.0f,1.0f),normal1(0.0f,0.0f,1.0f)
{
	limitangle = _limitangle;
	equality = (limitangle==0.0f);
	torque=targetspin=0.0f;
	maxtorque = FLT_MAX;
}
void LimitCone::PreIter()
{
	if(!active) return;
	float3 a0 = (rb0)? rotate(rb0->orientation,normal0) : normal0;
	float3 a1 = (rb1)? rotate(rb1->orientation,normal1) : normal1;
	axis = safenormalize(cross(a1,a0));  // could this be an issue if a0 and a1 are collinear and we start enforcing a two sided constraint with limitangle 0 on a bogus axis
	float rbangle = acosf(clamp(dot(a0,a1),0.0f,1.0f));
	float dangle = rbangle - limitangle;
	targetspin = ((equality)?physics_biasfactorjoint:1.0f) * dangle /physics_deltaT; // enforce all the way to limit if one-sided
	torque=0.0f;  // specifically its an angular impulse aka torque times time
}
void LimitCone::Iter()
{
	if(!active) return;
	if(targetspin==-FLT_MAX) return;
	float currentspin = ((rb1)?dot(rb1->spin,axis):0.0f) - ((rb0)?dot(rb0->spin,axis):0.0f) ;  // how we are rotating about the axis 'normal' we are dealing with
	float dspin = targetspin - currentspin;  // the amount of spin we have to add to satisfy the limit.
	float spintotorque = 1.0f / (  ((rb0)?dot(axis,rb0->Iinv*axis):0.0f) + ((rb1)?dot(axis,rb1->Iinv*axis):0.0f)  );
	float dtorque = dspin * spintotorque ;  // how we have to change the angular impulse
	if(!equality) dtorque = Max(dtorque,-torque);  // one-sided limit total torque cannot go below 0
	if(rb0)
	{
		rb0->rotation += axis*-dtorque;  // apply impulse
		rb0->spin = rb0->Iinv * rb0->rotation; // recompute auxilary variable spin
	}
	if(rb1)
	{
		rb1->rotation += axis*dtorque;  // apply impulse
		rb1->spin = rb1->Iinv * rb1->rotation; // recompute auxilary variable spin
	}
	torque += dtorque;
}
void LimitCone::PostIter()
{
	if(!active) return;
	// removes bias
	targetspin = (equality)?0:Min(targetspin,0.0f);  // not zero since its ok to let it fall to the limit
}


void createlimit(RigidBody *_rb,float _limitangle)
{
	int i=Limits.count;
	while(i--)
		if(Limits[i]->rb0==_rb)
			delete(Limits[i]);
	Limit *angularlimit= new LimitCone(_rb,_limitangle);
}

Array<Limit*> tmplimits;
void createdrive(RigidBody *rb0,RigidBody *rb1,Quaternion target,float maxtorque)
{
	//target=Quaternion(0,0,0,1.0f);  // TEST
//	Quaternion r1 = rb1->orientation ;
//	Quaternion r0 = Inverse(rb0->orientation*target);
//	if(dot(r1,r0)>0.0f) r1 *= -1.0f;
//	Quaternion dq = r1*r0  ;  // quat that takes a direction in r0+target orientation into r1 orientation
	Quaternion q0 = (rb0)?rb0->orientation:Quaternion();
	Quaternion q1 = (rb1)?rb1->orientation:Quaternion();
	Quaternion dq = q1 * Inverse(q0*target) ;  // quat that takes a direction in r0+target orientation into r1 orientation
	if(dq.w<0) dq=-dq;
	float3 axis     = safenormalize(dq.xyz()); // dq.axis();
	float3 binormal = Orth(axis);
	float3 normal   = cross(axis,binormal);
	tmplimits.Add ( new LimitAngular(rb0,rb1,axis,-physics_biasfactorjoint*(acosf(clamp(dq.w,-1.0f,1.0f))*2.0f)/physics_deltaT,maxtorque) );
	tmplimits.Add ( new LimitAngular(rb0,rb1,binormal,0,maxtorque) );
	tmplimits.Add ( new LimitAngular(rb0,rb1,normal  ,0,maxtorque) );
}

void createlimits2(RigidBody *rb0,RigidBody *rb1, const float3 &axis, const float angle)
{
	LimitAngular *xa,*xb;
	//tmplimits.Add ( (xa= new  LimitAngular(rb0,rb1,rb0->orientation.zdir(),asin(-magnitude(cross(rb0->orientation.xdir(),rb1->orientation.xdir()))))));
	//xa->equality=0;
	//tmplimits.Add ( (xb= new  LimitAngular(rb0,rb1,rotate(Inverse(rb0->orientation),float3(-1,0,0)),3.14f/2.0f)));
	//xb->equality=0;
	//LimitCone *cone = new LimitCone(rb1,DegToRad(10.0f));
	LimitCone *cone = new LimitCone(rb1,angle);

	cone->rb0=rb0;
	cone->rb1=rb1;
	cone->normal0=cone->normal1=axis;

	tmplimits.Add(cone);
	{
	LimitCone *cone = new LimitCone(rb1,DegToRad(45.0f));
	cone->rb0=rb0;
	cone->rb1=rb1;
	cone->normal0=normalize(float3(0,-1,1));
	cone->normal1=float3(0,0,1);
	tmplimits.Add(cone);
	}
	//tmplimits.Add ( new LimitAngular(rb0,rb1,rb0->orientation.zdir(),0.03f/physics_deltaT*asin(-dot(rb0->orientation.zdir(),(cross(rb0->orientation.xdir(),rb1->orientation.xdir()))))));
	//tmplimits.Add ( new LimitAngular(rb0,rb1,rb0->orientation.ydir(),0.03f/physics_deltaT*asin(-dot(rb0->orientation.ydir(),(cross(rb0->orientation.xdir(),rb1->orientation.xdir()))))));
	//tmplimits.Add ( new LimitAngular(rb0,rb1,rotate(Inverse(rb0->orientation),float3(0,0,1)),0) );
}

void createlimits(RigidBody *rb0,RigidBody *rb1, const float3 &axis, float angle)
{
	// this creates an axis constraint on rb0 and rb1's x axis with no x limit.
	// easy to change to any axis by multiplying joint frame on the rhs for both rb0 and rb1's orientations
	if(angle!=0.0f) return createlimits2(rb0,rb1,axis,angle);

	Quaternion jf = rb0->orientation  * RotationArc(float3(1,0,0),axis);  
	Quaternion eq = Inverse(jf) * rb1->orientation * RotationArc(float3(1,0,0),axis);
	Quaternion dq= eq*normalize(Quaternion(-eq.x,0,0,eq.w));
	if(dq.w<0) dq=dq*-1;

	LimitAngular *az= new  LimitAngular(rb0,rb1,jf.zdir(),physics_biasfactorjoint * 2 * -dq.z /physics_deltaT); 
	LimitAngular *ay= new  LimitAngular(rb0,rb1,jf.ydir(),physics_biasfactorjoint * 2 * -dq.y /physics_deltaT); 
	tmplimits.Add ( az);
	tmplimits.Add ( ay);

	LimitCone *cone = new LimitCone(rb1,DegToRad(45.0f));
	cone->rb0=rb0;
	cone->rb1=rb1;
	cone->normal0=normalize(float3(0,-1,1));
	cone->normal1=float3(0,0,1);
	tmplimits.Add(cone);
}

void createnail(RigidBody *rb0,const float3 &p0,RigidBody *rb1,const float3 &p1)
{
	float3 d = ((rb1)?rb1->pose()*p1:p1) - ((rb0)?rb0->pose()*p0:p0);
	tmplimits.Add ( new LimitLinear(rb0,rb1,p0,p1,float3(1.0f,0.0f,0.0f),d.x / physics_deltaT ,0.0f) );
	tmplimits.Add ( new LimitLinear(rb0,rb1,p0,p1,float3(0.0f,1.0f,0.0f),d.y / physics_deltaT ,0.0f) );
	tmplimits.Add ( new LimitLinear(rb0,rb1,p0,p1,float3(0.0f,0.0f,1.0f),d.z / physics_deltaT ,0.0f) );
}

Array<Shape*> Shapes;
Shape::Shape(RigidBody *_rb,WingMesh *_geometry):rb(_rb) 
{
	local_geometry = WingMeshDup(_geometry);
	Shapes.Add(this);
}
Shape::~Shape()
{
	Shapes.Remove(this);
}



int ConstraintsInactivateInvalid(RigidBody *dead_rb)
{
	int found=0;
	int i;
	i=Limits.count;
	while(i--)
	{
		if(Limits[i]->rb0==dead_rb)
		{
			Limits[i]->rb0=NULL;
			Limits[i]->active=0;
			found++; 
			//delete Limits[i];  // not sure if deletion is the best thing here, the other guys are just invalidated
		}
		if(Limits[i]->rb1==dead_rb)
		{
			Limits[i]->rb1=NULL;
			Limits[i]->active=0;
			found++; 
		}
	}

	// remove bodytobody constraints:
	for(i=0;i<Shapes.count;i++) 
	{
		Shape *shape0 = Shapes[i];
		RigidBody *rb0 = shape0->rb; 
		int j = shape0->contacts.count;
		while(j--)
		{
			BodyContact &bc = shape0->contacts[j];
			Shape  *shape1 = dynamic_cast<Shape*>(bc.b);
			RigidBody *rb1 = (shape1)?shape1->rb:NULL; 
			if(rb1==dead_rb) 
			{
				shape0->contacts.DelIndex(j);
			}
		}
	}

	return found; // number of constraints that were attached to this rigidbody.
}

RigidBody::RigidBody(WingMesh *_geometry,const Array<Face *> &_faces,const float3 &_position)
{
	int i;
	local_geometry=world_geometry=NULL;
	position=_position;
	rest=0;   
	hittime=0.0f;
	rigidbodies.Add(this);
	resolve=0;
	collide=0;
	usesound = 0;
	mass=1;
	massinv=1.0f/mass;
	radius=0;
	friction = physics_coloumb;

	if(!_geometry) 
	{
		return; // hack??
	}

	resolve=1;

	local_geometry = WingMeshDup(_geometry);
	//world_geometry = WingMeshDup(_geometry);
	for(i=0;i<_faces.count;i++) 
	{
		faces.Add(_faces[i]);
	}
	Array<int3> tris;
	WingMeshTris(_geometry,tris);
	com = CenterOfMass(local_geometry->verts.element,tris.element,tris.count);
	position += com;
	WingMeshTranslate(local_geometry,-com);
	FaceTranslate(faces,-com);
	tensor = Inertia(local_geometry->verts.element,tris.element,tris.count);
	tensorinv=Inverse(tensor);
	for(i=0;i<local_geometry->verts.count;i++) 
	{
		radius = Max(radius,magnitude(local_geometry->verts[i]));
		bmax = VectorMax(bmax,local_geometry->verts[i]); 
		bmin = VectorMax(bmax,local_geometry->verts[i]); 
	}
	collide=3;
	shapes.Add(new Shape(this,local_geometry));

}
 

RigidBody::RigidBody(BSPNode *bsp,const Array<Face *> &_faces,const float3 &_position)
{
	int i;
	local_geometry=world_geometry=NULL;
	position=_position;
	rest=0;   
	hittime=0.0f;
	rigidbodies.Add(this);
	collide=(bsp)?3:0;
	resolve=(bsp)?1:0;
	usesound = 0;
	mass=1;
	massinv=1.0f/mass;
	//physics_damping = 0.0f;
	friction = physics_coloumb;

	Array<WingMesh*> meshes;
	BSPGetSolids(bsp,meshes);
	for(i=0;i<meshes.count;i++)
		meshes[i] = shapes.Add(new Shape(this,meshes[i]))->local_geometry;


	//local_geometry = WingMeshDup(_geometry);
	//world_geometry = WingMeshDup(_geometry);
	for(i=0;i<_faces.count;i++) 
	{
		faces.Add(_faces[i]);
	}
	com = CenterOfMass(meshes);
	position += com;
	for(i=0;i<meshes.count;i++)
		WingMeshTranslate(meshes[i],-com);
	BSPTranslate(bsp,-com);
	//FaceTranslate(faces,-com);
	tensor = Inertia(meshes);
	tensorinv=Inverse(tensor);

	radius=0;
	for(i=0;i<meshes.count;i++)
	 for(int j=0;j<meshes[i]->verts.count;j++) 
	{
		radius = Max(radius,magnitude(meshes[i]->verts[j]));
		bmax = VectorMax(bmax,meshes[i]->verts[j]); 
		bmin = VectorMax(bmax,meshes[i]->verts[j]); 
	}
}


RigidBody::~RigidBody(){
	// I still have other memory leaks here
	ConstraintsInactivateInvalid(this);
	while(springs.count) {
		DeleteSpring(springs[springs.count-1]);
	}
	rigidbodies.Remove(this);
	for(int i=0;i<shapes.count;i++)
	{
		delete shapes[i];
	}
	shapes.count=0;
}



float springk=12.56f; // spring constant position - default to about half a second for unit mass weight
float springr=1.0f;   // spring constant rotation 
float physics_damping=0.25f;  // 1 means critically damped,  0 means no damping
EXPORTVAR(springk);
EXPORTVAR(physics_damping);
float3 SpringForce(State &s,const float3 &home,float mass) {
	return (home-s.position)*springk +   // spring part
		   (s.momentum/mass)* (-sqrtf(4 * mass * springk) * physics_damping);    // physics_damping part
}




int AddSpringForces(RigidBody *rba,const State &state,float3 *force,float3 *torque)
{
	int i;
	for(i=0;i<rba->springs.count;i++) {
		Spring *s = rba->springs[i];
		RigidBody *rbb;
		float3 pa;
		float3 pb;
		if(s->bodyA==rba) {
			pa = s->anchorA;
			rbb= s->bodyB;
			pb = s->anchorB;
		}
		else{
			pa = s->anchorB;
			rbb= s->bodyA;
			pb = s->anchorA;
		}
		//float3 wa = rba->position+rba->orientation*va;
		float3 wa = state.position+rotate(state.orientation,pa);
		float3 wb = rbb->position+rotate(rbb->orientation,pb);
		float3 va = rba->momentum*rba->massinv + cross(rba->spin,rotate(rba->orientation,pa));
		float3 vb = rbb->momentum*rbb->massinv + cross(rbb->spin,rotate(rbb->orientation,pb));
		float3 u  = safenormalize(wb-wa);
		float3 dforce = u * (dot(vb-va,u)*(sqrtf(4.0f * s->k / (rba->massinv+rbb->massinv)) * physics_damping)); 
		float3 sforce = (wb-wa) * s->k;
		*force += sforce + dforce;
		*torque+= cross(wa-rba->position,sforce+ dforce);
	}
	return 1;
}


void ApplyImpulse(RigidBody *rb,float3 r,float3 n,float impulse) 
{
	rb->momentum = rb->momentum + (n*(impulse));
	float3 drot = cross(r,(n*impulse));
	rb->rotation = rb->rotation + drot ;

	rb->spin = rb->Iinv * rb->rotation; // recompute auxilary variable spin
	//assert(magnitude(rb->spin) <10000);
}

void ApplyImpulseIm(RigidBody *rb,float3 r,float3 n,float impulse) 
{
	float3 drot = cross(r,(n*impulse));
	rb->orientation = normalize(rb->orientation + DiffQ(rb->orientation,rb->tensorinv,drot)*physics_deltaT);
	rb->Iinv = Transpose(rb->orientation.getmatrix()) * rb->tensorinv * (rb->orientation.getmatrix()); // recompute Iinv
	rb->spin = rb->Iinv * rb->rotation; // recompute auxilary variable spin
	rb->position +=(n*(impulse)) * rb->massinv * physics_deltaT;
	//assert(magnitude(rb->spin) <10000);
}


 
char *energy(RigidBody *rb)
{
	if(!rb) {
		return "no such rigidbody";
	}
	static char buf[512];
	// energy equasions - potential, kinetic, and rotational
	float Ep = rb->mass*(1-dot(rb->position,gravity));
	float Ek = 0.5f*rb->mass*squared(magnitude(rb->momentum*rb->massinv));
	float3x3 T = Transpose(rb->orientation.getmatrix()) * rb->tensor * (rb->orientation.getmatrix());
	float Er = 0.5f*dot((T*rb->spin),rb->spin);
	float E  = Ep+Ek+Er;
	sprintf_s(buf,512,"E=%5.4f  Ep=%5.4f  Ek=%5.4f  Er=%5.4f  ",E,Ep,Ek,Er);
	sprintf_s(buf+strlen(buf),512-strlen(buf),"rest=%d ",rb->rest);
	return buf;
}


float physics_driftmax = 0.05f;
EXPORTVAR(physics_driftmax);
float showcontactnormal=0.1f;
EXPORTVAR(showcontactnormal);


void PhysicsPurgeContacts()
{
	for(int i=0;i<Shapes.count;i++)
		Shapes[i]->contacts.count=0;
}

static void ShapeContactUpdate(Shape *shape) 
{
	int i=shape->contacts.count;
	while(i--)
	{
		BodyContact &b = shape->contacts[i];
		Shape *shape1 = dynamic_cast<Shape*>(shape->contacts[i].b);
		int j = b.count;
		while(j--)
		{
			Contact &c = b.c[j];
			// c.n0w = shape->orientation * c.n0;  // if you wanted to do something with twisting contacts away - angular drifting
			c.p0w = shape->Position() + rotate(shape->Orientation(),c.p0);
			if(shape1) c.p1w = shape1->Position() + rotate(shape1->Orientation(),c.p1);
			if(magnitude(c.p0w-c.p1w) > physics_driftmax) 
			{
				b.count--;
				c = b.c[b.count]; // DelIndexWithLast
				//shape->contacts.DelIndexWithLast(i);
			}
		}
	}
}
void ShapeContactUpdate() 
{
	for(int i=0;i<Shapes.count;i++)
		ShapeContactUpdate(Shapes[i]);
}


int suspendpairupdate=0;
EXPORTVAR(suspendpairupdate);

static void pairupdate(BodyContact &bc,Contact &hitinfo)
{
	if(suspendpairupdate)return;
	// update the current colliding pair with the most recent contact/sample
	hitinfo.totalimpulse=0;  // todo move this into contact constructor if not already
	bc.tangent = Orth(hitinfo.normal);
	bc.binormal= cross(hitinfo.normal,bc.tangent);
	int j;
	bc.count=Min(bc.count,4); // make sure last (5th) slot is open.  Deletes last contact if necessary
	for(j=0;j<bc.count;j++)
	{
		bc.c[j].normal = hitinfo.normal;  // normal unification.  attempting to prevent any drift.
	}
	for(j=0;j<bc.count;j++)
	{
		if(magnitude(bc.c[j].impact-hitinfo.impact)<0.02f)
		{
			// replace an existing coincident contact with the new information.
			// keep the impulse history
			hitinfo.binormalimpulse = bc.c[j].binormalimpulse;
			hitinfo.tangentimpulse  = bc.c[j].tangentimpulse;
			hitinfo.totalimpulse    = bc.c[j].totalimpulse;     
			bc.c[j] = hitinfo;
			return;
		}
	}
	if(bc.count<4)  
	{
		bc.c[bc.count++] = hitinfo;  // add contact to set
		for(j=0;j<bc.count;j++)
		{
			int j1 = (j+1) %bc.count;
			int j2 = (j+2) %bc.count;
			float3& v0 = bc.c[j ].impact;
			float3& v1 = bc.c[j1].impact;
			float3& v2 = bc.c[j2].impact;
			if(dot(cross(v1-v0,v2-v1),hitinfo.normal)<0)
			{
				Swap(bc.c[j1],bc.c[j2]);  // ensure proper winding
			}
		}
		return;
	}
	assert(bc.count==4);
	// if we find that we can increase the contact patch area then replace a previous sample, 
	// otherwise just append the new contact onto the end so it will be used once for the current timestep
	for(j=0;j<bc.count;j++)
	{
		int jp = (j+bc.count-1) %bc.count;
		int jn = (j+1)          %bc.count;
		float3& vp = bc.c[jp].impact;
		float3& vn = bc.c[jn].impact;
		float3& vc = bc.c[j ].impact;
		float3& v  = hitinfo.impact;
		float3& n  = hitinfo.normal;
		if(dot(n,cross(v-vp,vn-v))>dot(n,cross(vc-vp,vn-vc)))
		{
			break;
		}
	}
	assert(j<=4);
	bc.c[j] = hitinfo;
}

void PenetrationRollback(Shape *shape)
{
	int i;
	RigidBody *rb = shape->rb;
	if(shape->rb->shapes.count>3) return; // this can be expensive  so for now I'm just testing on the small stuff.
	Array<WingMesh*> cells;
	//ProximityCells(shape,bsp,cells,physics_driftmax+ magnitude(rb->position_next - rb->position_old));  
	for(i=0;i<area_bsps.count;i++)
		ProximityCells(shape,area_bsps[i],cells,physics_driftmax+ magnitude(rb->position_next - rb->position_old));
	int k=0;

	// first test the rollback to see if its valid. abort if not.
	rb->position = rb->position_old;
	rb->orientation = rb->orientation_old;
	for(i=0;i<cells.count;i++)
	{
		Contact hitinfo;
		hitinfo.C[0] = shape;
		hitinfo.C[1] = cells[i];
		if(!Separated(shape,cells[i],hitinfo,0))
		{
			// previous state was invalid so just abort any attempt at rollback.
			rb->position = rb->position_next;
			rb->orientation = rb->orientation_next;
			return;
		}
	}

	rb->position = rb->position_next;
	rb->orientation = rb->orientation_next;
	for(i=0;i<cells.count;i++)
	{
		Contact hitinfo;
		hitinfo.C[0] = shape;
		hitinfo.C[1] = cells[i];
		if(!Separated(shape,cells[i],hitinfo,0))
		{
			break;
		}
	}
	if(i==cells.count) return; // didn't collide with anything   no penetration issues here

	// something penetrated so lets first try rewinding the rotation:
	rb->orientation = rb->orientation_old ;
	rb->orientation_next = rb->orientation_old;
	rb->rotation = float3(0,0,0);
	// i hate having copy-pasted these lines here:
	rb->Iinv = Transpose(rb->orientation.getmatrix()) * rb->tensorinv * (rb->orientation.getmatrix());
	rb->spin = rb->Iinv * rb->rotation;

	for(i=0;i<cells.count;i++)
	{
		Contact hitinfo;
		hitinfo.C[0] = shape;
		hitinfo.C[1] = cells[i];
		if(!Separated(shape,cells[i],hitinfo,0))
		{
			break;
		}
	}
	if(i==cells.count) return; // just linear motion used

	// ok lets try rolling back the position part way
	rb->position = rb->position_next = rb->position_old *0.75f + rb->position_next*0.25f; 
	rb->momentum *= 0.25f; 

return;  // lets keep the simulation at least going somewhat.

	for(i=0;i<cells.count;i++)
	{
		Contact hitinfo;
		hitinfo.C[0] = shape;
		hitinfo.C[1] = cells[i];
		if(!Separated(shape,cells[i],hitinfo,0))
		{
			break;
		}
	}
	if(i==cells.count) return; // half way linear

	rb->position = rb->position_next = rb->position_old;   // complete rollback
	rb->momentum = float3(0,0,0);
	return;
}

void PenetrationRollback()
{
	for(int i=0;i<Shapes.count;i++)
		PenetrationRollback(Shapes[i]);
}

int HitCheck(Array<BSPNode *> bsps,const float3 &v0,float3 v1,float3 *impact)
{
	int hit=0;
	for(int i=0;i<bsps.count;i++)
		hit |= HitCheck(bsps[i],1,v0,v1,&v1);
	if(hit && impact) 
		*impact=v1;
	return hit;
}


void CCD(RigidBody *rb)
{
	int i;
	float3 hitpoint;
	for(i=0;i<area_bsps.count;i++)
        if(HitCheck(area_bsps,rb->position_old,rb->position_old,&hitpoint)) return; // we were inside a bad spot last frame anyways.
	int k=0;
	while(k<4 && HitCheck(area_bsps,rb->position_old,rb->position_next,&hitpoint)) 
	{
		k++;
		extern float3 HitCheckImpactNormal;  // global variable storing the normal of the last hit 
		rb->position = rb->position_next = PlaneProject(Plane(HitCheckImpactNormal,-dot(hitpoint,HitCheckImpactNormal)-0.001f),rb->position_next);
		rb->momentum =  float3(0,0,0); //  rb->mass * (rb->position_next-rb->position_old)/physics_deltaT;
	}
	if(k==4)
	{
		rb->position_next = rb->position = rb->position_old;
		rb->momentum = float3(0,0,0);
	}
}
void CCD()
{
	int i;
	for(i=0;i<rigidbodies.count;i++) 
	{
		RigidBody *rb = rigidbodies[i];
		if(!rb->resolve) continue;
		CCD(rb); 
	}
}


void FindShapeWorldContacts(Shape *shape) 
{
	int i,j;
	if(!(shape->rb->collide&1)) { return; }
	Array<WingMesh*> cells;
	for(i=0;i<area_bsps.count;i++)
	{
		ProximityCells(shape,area_bsps[i],cells,physics_driftmax);
	}
	for(i=0;i<cells.count;i++)
	{
		if(!cells[i]) continue; // it was null, so dont collide. /// This is probably a bug!!
		Contact hitinfo;
		hitinfo.C[0] = shape;
		hitinfo.C[1] = cells[i];
		int sep = Separated(shape,cells[i],hitinfo,1);
		if(sep && hitinfo.separation>physics_driftmax) continue; 
		hitinfo.n0w = hitinfo.n1w = hitinfo.normal;
		hitinfo.n0 = rotate(Inverse(shape->Orientation()) , hitinfo.n0w);
		hitinfo.p0 = rotate(Inverse(shape->Orientation()) , hitinfo.p0w - shape->Position());
		hitinfo.n1 = hitinfo.n1w;
		hitinfo.p1 = hitinfo.p1w;
		for(j=0;j<shape->contacts.count;j++)
		{
			if(shape->contacts[j].b == hitinfo.C[1]) break;
		}
		if(j==shape->contacts.count)
		{
			shape->contacts.Add(BodyContact(hitinfo.C[1]));
		}
		BodyContact &bc = shape->contacts[j];
		
		pairupdate(bc,hitinfo);
	}

	if(showcontactnormal>0.0f) for(i=0;i<shape->contacts.count;i++)  // draw debug contact info
	{
		BodyContact &bc = shape->contacts[i];
		for(j=0;j<bc.count;j++)
		{
			Contact &c = bc.c[j];
			extern void Line(const float3 &,const float3 &,const float3 &color_rgb);
			Line(c.p0w,c.p1w,float3(1,0,1));
			Line(c.p0w,c.p0w+c.n0w*showcontactnormal,float3(0,0,1));
			Line(c.p1w,c.p1w+c.n1w*showcontactnormal,float3(0,0,1));
		}
	}
}
void FindShapeWorldContacts()
{
	for(int i=0;i<Shapes.count;i++)
		FindShapeWorldContacts(Shapes[i]);
}



void FindShapeShapeContacts()  // Dynamic-Dynamic contacts
{
	int i_rb,j_rb;
	for(i_rb=0;i_rb<rigidbodies.count;i_rb++)
	  for(j_rb=i_rb+1;j_rb<rigidbodies.count;j_rb++)
	{
		RigidBody *rb0 = rigidbodies[i_rb];
		RigidBody *rb1 = rigidbodies[j_rb];
		if(!(rb0->collide&2)) continue; 
		if(!(rb1->collide&2)) continue;
		if(magnitude(rb1->position - rb0->position) > rb0->radius + rb1->radius) continue;
		int ignore=0;
		for(int ign=0;ign<rb0->ignore.count;ign++)
		{
			ignore |= (rb0->ignore[ign]==rb1);
		}
		if(ignore)continue;
		int i_s,j_s,k;
		for(i_s=0 ; i_s<rb0->shapes.count ; i_s++)
		 for(j_s=0 ; j_s<rb1->shapes.count ; j_s++)
		{
			Shape *s0=rb0->shapes[i_s];
			Shape *s1=rb1->shapes[j_s];
			assert(s0->rb!=s1->rb);
			Contact hitinfo;
			hitinfo.C[0] = s0;
			hitinfo.C[1] = s1;
			int sep = Separated(s0,s1,hitinfo,1);  
			if(sep && hitinfo.separation>physics_driftmax) continue; 
			hitinfo.n0w = hitinfo.n1w = hitinfo.normal;
			hitinfo.n0 = rotate(Inverse(s0->Orientation()) , hitinfo.n0w);
			hitinfo.p0 = rotate(Inverse(s0->Orientation()) , hitinfo.p0w - s0->Position());
			hitinfo.n1 = rotate(Inverse(s1->Orientation()) , hitinfo.n1w);
			hitinfo.p1 = rotate(Inverse(s1->Orientation()) , hitinfo.p1w - s1->Position());
			for(k=0;k<s0->contacts.count;k++)
			{
				if(s0->contacts[k].b == hitinfo.C[1]) break;
			}
			if(k==s0->contacts.count)
			{
				s0->contacts.Add(BodyContact(s0,hitinfo.C[1]));
				assert(s0->contacts[k].b == s1);
			}
			BodyContact &bc = s0->contacts[k];
			pairupdate(bc,hitinfo);
		}
	}
}


float showspin=0.0f;
EXPORTVAR(showspin);
float aerotwirl=0.0f;
EXPORTVAR(aerotwirl);
float aeroflutter = 1.0f;
EXPORTVAR(aeroflutter);

float3 wind(0,0,0);
EXPORTVAR(wind);
float windtunnelheight = 3.0f;
EXPORTVAR(windtunnelheight);
float winddensity = 0.0f;  // also works as an 'enable' flag and a fudgefactor to avoid possibility of forward euler instability
EXPORTVAR(winddensity);  // technically this is pseudo density of the medium (air, water, watever) 0==vacuum 

void AddWindForces(RigidBody *rb,State s,float3 *force,float3 *torque)
{
	if(winddensity==0.0f) return;
	float3 wind = ::wind*((windtunnelheight-rb->position.z)/windtunnelheight);  // inverted cone - full wind at bottom, none at top
	struct wps 
	{ float3 p; float3 n; wps(float3 _p,float3 _n):p(_p),n(normalize(_n)){}
	} corners[4]=
	{
		wps(float3(-1,-1,0),float3(0,0,10)+float3(-1,-1,0)*aeroflutter+float3( 1,-1,0)*aerotwirl),
		wps(float3( 1,-1,0),float3(0,0,10)+float3( 1,-1,0)*aeroflutter+float3( 1, 1,0)*aerotwirl),
		wps(float3( 1, 1,0),float3(0,0,10)+float3( 1, 1,0)*aeroflutter+float3(-1, 1,0)*aerotwirl),
		wps(float3(-1, 1,0),float3(0,0,10)+float3(-1, 1,0)*aeroflutter+float3(-1,-1,0)*aerotwirl),
	};
	for(int i=0;i<4;i++)
	{
		float3 r = rotate(rb->orientation,corners[i].p);
		float3 v = rb->momentum * rb->massinv + cross(rb->spin,r); // abs velocity at corner
		float3 n = rotate(rb->orientation,corners[i].n);  // corner normal in world
		float3 w = wind - v;  // relative wind at point on object
		float3 u = normalize(w);
		float3 impulse = n*dot(w,n) * physics_deltaT * winddensity; 
		//ApplyImpulse
		rb->momentum = rb->momentum + impulse;
		float3 drot = cross(r,impulse);   
		rb->rotation = rb->rotation + drot ;
	}
}

void rbinitvelocity(RigidBody *rb) 
{
	// gather weak forces being applied to body at beginning of timestep
	// forward euler update of the velocity and rotation/spin 
	rb->old.position    = rb->position;
	rb->old.orientation = rb->orientation;
	float dampleftover = powf((1.0f-physics_damping),physics_deltaT);
	rb->momentum *= dampleftover; 
	rb->rotation *= dampleftover;
	State s = rb->state(); // (State) *rb;
	float3 force;
	float3 torque;
	force  = gravity*rb->mass;
	torque = float3(0,0,0);
	AddSpringForces(rb,s,&force,&torque);
	AddWindForces(rb,s,&force,&torque);
	rb->momentum += force*physics_deltaT; 
	rb->rotation += torque*physics_deltaT;
	rb->Iinv = Transpose(rb->orientation.getmatrix()) * rb->tensorinv * (rb->orientation.getmatrix());
	rb->spin = rb->Iinv * rb->rotation;
}

void rbcalcnextpose(RigidBody *rb) 
{
	// after an acceptable velocity and spin are computed, a forward euler update is applied ot the position and orientation.
	rb->position_next     = rb->position + rb->momentum * rb->massinv * physics_deltaT; 
	rb->orientation_next  = rkupdateq(rb->orientation,rb->tensorinv,rb->rotation,physics_deltaT);
	for(int i=0;i<3;i++)
		if(rb->orientation_next[i]< FLT_EPSILON/4.0 && rb->orientation_next[i]> -FLT_EPSILON/4.0 ) 
			rb->orientation_next[i]=0.0f; // dont keep needless precision, somehow this causes serious slowdown on Intel's centrino processors perhaps becuase as extra shifting is required during multiplications
}

void rbscalemass(RigidBody *rb,float s)
{
	rb->mass      *= s;
	rb->momentum  *= s;
	rb->massinv   *= 1.0f/s;
	rb->rotation  *= s;
	rb->tensor    *= s;
	rb->tensorinv *= 1.0f/s;
	rb->Iinv      *= 1.0f/s;
}

void rbupdatepose(RigidBody *rb) 
{
	// after an acceptable velocity and spin are computed, a forward euler update is applied ot the position and orientation.
	rb->position_old    = rb->position;  // should do this at beginning of physics loop in case someone teleports the body.
	rb->orientation_old = rb->orientation;
	rb->position        = rb->position_next ; 
	rb->orientation     = rb->orientation_next;
	rb->Iinv = Transpose(rb->orientation.getmatrix()) * rb->tensorinv * (rb->orientation.getmatrix());
	rb->spin = rb->Iinv * rb->rotation;
	if(showspin>0.0f)
	{
		extern void Line(const float3 &,const float3 &,const float3 &color_rgb);
		Line(rb->position,rb->position+safenormalize(rb->spin)*showspin ,float3(0,1,1));
		Line(rb->position,rb->position+safenormalize(rb->rotation)*showspin ,float3(1,0,1));
	}
}



LimitLinear::LimitLinear(RigidBody *_rb0,RigidBody *_rb1,const float3 &_position0,const float3 &_position1,const float3 &_normal,float _targetspeed,float _targetspeednobias,float _maxforce):
	Limit(_rb0,_rb1),position0(_position0),position1(_position1),
	normal(_normal),targetspeed(_targetspeed),targetspeednobias(_targetspeednobias),maxforce(_maxforce)
{
	equality=1;
	impulsesum=0.0f;
	impulseprev=0.0f;
}
void LimitLinear::PreIter()
{
	impulsesum=0;
}
void LimitLinear::WarmStart()
{
	// if(!warmstart) return;
	if(rb0) ApplyImpulse(rb0,rotate(rb0->orientation , position0),normal ,-impulseprev ); 
	if(rb1) ApplyImpulse(rb1,rotate(rb1->orientation , position1),normal , impulseprev ); 
	impulsesum += impulseprev;
}
void LimitLinear::PostIter()
{
	targetspeed=targetspeednobias; // (equality)?0:Min(targetspeed,0.0f);  // not zero since its ok to let it fall to the limit;
}
void LimitLinear::Finalize()
{
	impulseprev = impulsesum; 
}
void LimitLinear::Iter()  
{
// Want to consolidate what is currently in different structures to make a consistent low level api.  
// Immediate mode style works nicely for higher level joint objects such as limit cone.  
// With an immediate mode api there needs to be a mechanism for optional feedback to higher level for warmstarting and what forces being applied eg sound effects or reading teeter's weight
	if(!active) return;
	float3 r0  = (rb0) ? rotate(rb0->orientation , position0) :  position0;
	float3 r1  = (rb1) ? rotate(rb1->orientation , position1) :  position1;
	float3 v0  = (rb0) ? cross(rb0->spin,r0) + rb0->momentum*rb0->massinv : float3(0,0,0); // instantaneioius linear velocity at point of constraint
	float3 v1  = (rb1) ? cross(rb1->spin,r1) + rb1->momentum*rb1->massinv : float3(0,0,0); 
	float  vn  = dot(v1-v0,normal);  // velocity of rb1 wrt rb0
	float impulsen = -targetspeed - vn;
	float impulsed =  ((rb0)? rb0->massinv + dot( cross((rb0->Iinv*cross(r0,normal)),r0),normal):0) 
	                + ((rb1)? rb1->massinv + dot( cross((rb1->Iinv*cross(r1,normal)),r1),normal):0) ;
	float impulse = impulsen/impulsed;
	impulse = Min( maxforce*physics_deltaT-impulsesum,impulse);
	impulse = Max(((equality)?-maxforce:0.0f)*physics_deltaT-impulsesum,impulse);
	if(rb0) ApplyImpulse(rb0,r0,normal ,-impulse ); 
	if(rb1) ApplyImpulse(rb1,r1,normal , impulse ); 
	impulsesum += impulse;
}
void LimitLinear::PositionCorrection()
{
	if(!positioncorrection) return;

	float3 r0  = (rb0) ? rotate(rb0->orientation , position0) :  position0;
	float3 r1  = (rb1) ? rotate(rb1->orientation , position1) :  position1;
	float3 p0w = (rb0) ? rb0->position + r0 : r0;
	float3 p1w = (rb1) ? rb1->position + r1 : r1;
	//float3 v0  = (rb0) ? cross(rb0->spin,r0) + rb0->momentum*rb0->massinv : float3(0,0,0); // instantaneioius linear velocity at point of constraint
	//float3 v1  = (rb1) ? cross(rb1->spin,r1) + rb1->momentum*rb1->massinv : float3(0,0,0); 
	float  sn  = dot(p1w-p0w,normal);  // separation
	float impulsen = -sn / physics_deltaT; 
	if(!equality)
	{
		float minsep = physics_driftmax*0.25f;
		impulsen = 1.0f * -Min(0.0f,(sn-minsep)) / physics_deltaT;
	}
	float impulsed =  ((rb0)? rb0->massinv + dot( cross((rb0->Iinv*cross(r0,normal)),r0),normal):0) 
	                + ((rb1)? rb1->massinv + dot( cross((rb1->Iinv*cross(r1,normal)),r1),normal):0) ;
	float impulse = impulsen/impulsed;
	if(rb0) ApplyImpulseIm(rb0,r0,normal ,-impulse ); 
	if(rb1) ApplyImpulseIm(rb1,r1,normal , impulse ); 
}



void PreIterByShapes()
{
	int i,j,k;
	for(i=0;i<Limits.count;i++)
		if(Limits[i]->active) Limits[i]->PreIter();
	for(i=0;i<Limits.count;i++)
		if(Limits[i]->active) Limits[i]->WarmStart();

	for(i=0;i<Shapes.count;i++) 
	{
		Shape *shape0 = Shapes[i];
		RigidBody *rb0 = shape0->rb; 
		if(!rb0->resolve) continue;
		for(j=0;j<shape0->contacts.count;j++)
		{
			BodyContact &bc = shape0->contacts[j];
			Shape  *shape1 = dynamic_cast<Shape*>(bc.b);
			RigidBody *rb1 = (shape1)?shape1->rb:NULL; 
			for(k=0;k<bc.count;k++)
			{
				Contact &c = bc.c[k];
				float3 &n =c.normal;
				float3 v,vn,r0,v0,r1,v1;
				r0 = c.p0w - rb0->position;  
				v0 = cross(rb0->spin,r0)   + rb0->momentum*rb0->massinv; // instantaneioius linear velocity at point of collision
				r1= (rb1) ? c.p1w - rb1->position : float3(0,0,0);  
				v1= (rb1) ? cross(rb1->spin,r1) + rb1->momentum*rb1->massinv : float3(0,0,0); 
				v = v0-v1;  // todo would look cleaner if it was v1-v0.
				vn = n*dot(v,n);
				float minsep = physics_driftmax*0.25f;
				float separation = dot(n,c.p0w-c.p1w);  // todo verify direction here
				float extra = (minsep-separation);
				c.bouncevel = 0.0f;
				c.targvel = (extra/physics_deltaT)*Min(1.0f,((extra>0.0f)? physics_biasfactorpositive*physics_deltaT*60 : physics_biasfactornegative*physics_deltaT*60));  // "bias" fudgefactor to move past minseparation
				if(separation<=0)  // attempt to immediately pull completely out of penetration
				{
					c.targvel += -separation/physics_deltaT * (1.0f - Min(1.0f, physics_biasfactorpositive*physics_deltaT*60 ));  // ensure we go at least fast enough to stop penetration
				}
				if(dot(v,c.normal) < 0 && magnitude(vn) > magnitude(gravity)*physics_falltime_to_ballistic )
				{
					// take off any contribution of gravity on the current/recent frames to avoid adding energy to the system.
					c.bouncevel = Max(0.0f, (magnitude(vn)-magnitude(gravity)*physics_falltime_to_ballistic) * physics_restitution);  // ballistic non-resting contact, allow elastic response
					c.targvel   = Max(0.0f,c.targvel-c.bouncevel);
				}
				if(physics_positioncorrection) c.targvel=Min(c.targvel,0.0f);
				//tmplimits.Add(
				LimitLinear *lim = new LimitLinear(rb0,rb1,c.p0,c.p1,   -c.normal,-(c.targvel+c.bouncevel),-c.bouncevel);
				lim->equality =0;
				lim->positioncorrection=1;
				tmplimits.Add(lim);
				tmplimits.Add(new LimitFriction(lim,bc.tangent ));
				tmplimits.Add(new LimitFriction(lim,bc.binormal));
			}
		}
	}

}

int physics_iterations=6;
EXPORTVAR(physics_iterations);

void IteraterByShapes() 
{

	int s,i;
	for(s=0;s<physics_iterations;s++)  // iteration steps
		for(i=0;i<Limits.count;i++) 
			if(Limits[i]->active) Limits[i]->Iter();

		
}

int biasimpulseremove=1;
EXPORTVAR(biasimpulseremove);
void PostIterByShapes()
{
	// The objective of this step is to take away any velocity that was added strictly for purposes of reaching the constraint or contact.
	// i.e. we added some velocity to a point to pull it away from something that was 
	// interpenetrating but that velocity shouldn't stick around for the next frame,
	// otherwise we will have lots of jitter from our contacts and occilations from constraints.
	// this is accomplished by clearing the targetvelocities to zero and invoking the solver.
	if(biasimpulseremove==0) return;
	int i;
	for(i=0;i<Limits.count;i++) 
		if(Limits[i]->active) Limits[i]->PostIter();
	IteraterByShapes();
	for(i=0;i<Limits.count;i++) 
		if(Limits[i]->active) Limits[i]->Finalize();
}

void PositionCorrection()
{
	if(!physics_positioncorrection) return;
	// trying catto's suggestion
	for(int s=0 ; s<physics_iterations ; s++)
	 for(int i=0;i<Limits.count;i++) 
		if(Limits[i]->active) 
			Limits[i]->PositionCorrection();
}
int physics_penetration_rollback=0;
EXPORTVAR(physics_penetration_rollback);
int physics_ccd=1;
EXPORTVAR(physics_ccd);

int physics_enable=1;
EXPORTVAR(physics_enable);
void PhysicsUpdate() 
{
	if(!physics_enable) return;

	GetEnvBSPs(area_bsps);
	int i;
	for(i=0;i<rigidbodies.count;i++) 
	{
		RigidBody *rb = rigidbodies[i];
		if(!rb->resolve) continue;
		rbinitvelocity(rb); // based on previous and current force/torque
	}
	ShapeContactUpdate(); 
	FindShapeWorldContacts();
	FindShapeShapeContacts();
	
	PreIterByShapes();
	IteraterByShapes(); 

	for(i=0;i<rigidbodies.count;i++) 
	{
		RigidBody *rb = rigidbodies[i];
		if(!rb->resolve) continue;
		rbcalcnextpose(rb); 
	}
	PostIterByShapes();
	for(i=0;i<rigidbodies.count;i++) 
	{
		RigidBody *rb = rigidbodies[i];
		if(!rb->resolve) continue;
		rbupdatepose(rb); 
	}
	PositionCorrection();
	if(physics_ccd) 
	{
		CCD();
	}

	if(physics_penetration_rollback)
	{
		PenetrationRollback();
	}
	while(tmplimits.count)  // temporary limits intended only for the current iteration
	{
		delete tmplimits.Pop();
	}
}




void PutBodyToRest(RigidBody *rb) 
{
	rb->momentum=float3(0,0,0);
	rb->rotation=float3(0,0,0);
	rb->spin = float3(0,0,0);
}

