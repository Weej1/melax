
//------------
//  (c) Stan Melax 2006-2008
// an object layer above the physics layer to provide all those 
// hooks and interfaces with the rest of the libs.
//
//

#include <float.h>
#include "chuckable.h"
#include "manipulatori.h"
#include "console.h"
#include "xmlparse.h"
#include "mesh.h"
#include "camera.h" // for spawn
#include "stringmath.h" // for parsing members
#include "d3dfont.h" // for poststring used in game specific code below
#include "vertex.h"  // for model class details used by character class

extern void BSPRenderTest(BSPNode *bsp,const float3 &p,const Quaternion &q,int shadow=1,int matid_override=-1);
extern BSPNode *MakeBSP(xmlNode *model);
extern float DeltaT;
extern float physics_deltaT;
Array<Chuckable*> Chuckables;


void GBB(Chuckable *c);
void CM(Chuckable *c);

class Joint : public Entity
{
  public:
	Chuckable *chuckable0;
	Chuckable *chuckable1;
    float3 position0;
	float3 position1;
	Quaternion orientation;
	float drivetorque;
	int    active;
	int directive; // flag for extra stuff we want this to do.  just some hacking for now
	void update();
	const float3 &SetPosition0(const float3 &_position0){position0=_position0;return position0;}
	const float3 &SetPosition1(const float3 &_position1){position1=_position1;return position1;}
	Joint(Chuckable *rb,Chuckable *_rb1=NULL);
	~Joint(); 
	int SetActive(int _active) {active=_active;return active;}
	float maxforce;
};
Array<Joint*> Joints;

Joint::Joint(Chuckable *rb,Chuckable *_rb1):Entity("joint"),active(1)
{
	chuckable0 = rb;  
	chuckable1 = _rb1;
	maxforce = FLT_MAX;
	drivetorque = 0.0f;
	directive=0;
	Joints.Add(this);
}
Joint::~Joint() 
{
	Joints.Remove(this);
}
void Joint::update()
{
	if(!active) return;
	extern void createnail(RigidBody *rb0,const float3 &p0,RigidBody *rb1,const float3 &p1);
	createnail(this->chuckable0,this->position0,this->chuckable1,this->position1);

	extern void createdrive(RigidBody *rb0,RigidBody *rb1,Quaternion target,float maxtorque);
	if(drivetorque>0.0f) createdrive(chuckable0,chuckable1,orientation,drivetorque);
}
void JointUpdate()
{
	for(int i=0;i<Joints.count;i++)
		Joints[i]->update();
}
String jointalign(String s)
{
	for(int i=0;i<Joints.count;i++)
	{
		Chuckable *c0 = Joints[i]->chuckable0;
		Chuckable *c1 = Joints[i]->chuckable1;
		assert(c0);
		float3 wp =  (c1)?c1->position + rotate(c1->orientation,Joints[i]->position1) : Joints[i]->position1 ;
		float3 p0 =  rotate(Inverse(c0->orientation),wp-c0->position);
		Joints[i]->SetPosition0(p0);
	}
	return String(Joints.count) + " joints were aligned.";
}
EXPORTFUNC(jointalign);

StringIter &operator >>(StringIter &s,Joint &joint) 
{
	s.p=SkipChars(s.p," \t\n\r;,");
	s = s >> joint.position0 >> joint.position1;
	return s;
}

class JointManipulator : public Manipulator , public Tracker
{
  public:
	Joint *joint;
	float3 mypos;
	Quaternion& Orientation(){return joint->orientation;}
	float3 &Position(){mypos = (joint->chuckable1)? rotate(joint->chuckable1->orientation,joint->position1)+joint->chuckable1->position : joint->position1; return mypos;} 
	JointManipulator(Joint *_joint):joint(_joint){joint->trackers.Add(this);Position()=(joint->chuckable1)?joint->chuckable1->position+rotate(joint->chuckable1->orientation,joint->position1):joint->position1;}
	void PositionUpdated()
	{
		float3 position1 = (joint->chuckable1)? rotate(Inverse(joint->chuckable1->orientation), mypos-joint->chuckable1->position) : mypos;
		joint->SetPosition1(position1);
	}
	int Wheel(int w)
	{
		if(!InDrag())
		{
			return 1;
		}
		// else
		return Manipulator::Wheel(w);
	}
	int KeyPress(int k)
	{
		if(k==']' || k=='}') 
		{
			joint->drivetorque = Max(1.0f,joint->drivetorque*2.0f);
			return 1;
		}
		if(k=='[' || k=='{') 
		{
			joint->drivetorque = floorf(joint->drivetorque/2.0f);
			return 1;
		}
		if(k=='\b')
		{
			delete joint;
			return 1;
		}
		return 0;
	}
	void notify(Entity *){joint=NULL;delete this;}
	~JointManipulator(){if(joint)joint->trackers.Remove(this);}
};


LDECLARE(Chuckable,position);
LDECLARE(Chuckable,orientation);
LDECLARE(Chuckable,rotation);  // angular
LDECLARE(Chuckable,momentum);  // linear
LDECLARE(Chuckable,message);
//LDECLARE(Chuckable,damping);
LDECLARE(Chuckable,friction);
LDECLARE(Chuckable,collide);
LDECLARE(Chuckable,render);



Chuckable::Chuckable(const char * name,WingMesh *_geometry,const Array<Face *> &_faces,const float3 &_position)
    :Entity(name),RigidBody(_geometry,_faces,_position)
{
	render=1;
	//position=_position;
	LEXPOSEOBJECT(Chuckable,id);
	Chuckables.Add(this);
	model=NULL;
	GBB(this);
	CM(this);
	submerged=0;
}

Chuckable::Chuckable(const char * name,BSPNode *_bsp,const Array<Face *> &_faces,const float3 &_position,int _shadowcast)
    :Entity(name),RigidBody(_bsp,_faces,_position)
{
	//position=_position;
	render=1;
	LEXPOSEOBJECT(Chuckable,id);
	Chuckables.Add(this);
	model=NULL;
	GBB(this);
	model = CreateModel(_bsp,_shadowcast);
	submerged=0;
	//CM(this);
}


String cpath("./ models/ meshes/ brushes/");
Chuckable *ChuckableLoad(const char* filename)
{
	// assumes that it can grab or make a bsp from the file
	// assumes that it is just a single convex piece.
	int i;
	xmlNode *e = NULL;
	e=XMLParseFile(filefind(filename,cpath,".brush .xml .bsh"));
	if(!e) return NULL;
	BSPNode *bsp=MakeBSP(e);
	Array<Face*> faces;
	BSPGetBrep(bsp,faces);
	Chuckable *chuck = new Chuckable(filename,bsp,faces, float3(0,0,0), 1);  // e->hasAttribute("shadowcast")?e->attribute("shadowcast").Asint():
	//chuck->name = filename;
	for(i=0;i<e->children.count;i++)
	{
		if(e->children[i]->tag == "joint")
		{
			Joint *joint = new Joint(chuck);
			StringIter(e->children[i]->body) >> *joint;
			if(e->children[i]->hasAttribute("name"))
				joint->id = e->children[i]->attribute("name");
			if(e->children[i]->hasAttribute("limit"))
			{
				extern void createlimit(RigidBody *_rb,float _limitangle);
				createlimit(chuck,DegToRad(e->children[i]->attribute("limit").Asfloat()));
			}
			JointManipulator *jm = new JointManipulator(joint);
		}
		if(e->children[i]->tag == "mass")
		{
			float mass=1.0f;
			StringIter(e->children[i]->body) >> mass;
			rbscalemass(chuck,mass);
		}
	}
	xmlimport(chuck,GetClass("Chuckable"),e);
	return chuck;
}



String spawnchuckable(String s)
{
	String name = 	PopFirstWord(s);
	Chuckable *c = ChuckableLoad(name);
	if(!c) return "no such object";
	extern Camera camera;
	c->position +=  camera.position+camera.orientation.zdir() * -1.5f ;
	StringIter(s) >> c->position;
	return "ok";
}
EXPORTFUNC(spawnchuckable);

Chuckable::~Chuckable()
{
	ModelDelete(model);
	Chuckables.Remove(this);
}

int Chuckable::HitCheck(const float3 &_v0,const float3 &_v1,float3 *impact)
{
	int hit=0;
	float3 v0 = Inverse(pose()) * _v0; // rotate(Inverse(orientation),(_v0 - position));
	float3 v1 = Inverse(pose()) * _v1; //rotate(Inverse(orientation),(_v1 - position));
	if(local_geometry) hit = ConvexHitCheck(local_geometry,v0,v1,&v1);
	for(int i=0;i<shapes.count;i++)
	{
		hit |= ConvexHitCheck(shapes[i]->local_geometry,v0,v1,&v1);
	}
	if(hit && impact)
	{
		*impact = pose() * v1; // position + rotate(orientation,v1);
	}
	return hit;
}

Chuckable *ChuckableFind(String param)
{
	return NULL; // dynamic_cast<Chuckable*>(ObjectFind(param));
}



class Grabber : public Manipulator , public Tracker
{
  public:
	Grabber(Chuckable *_chuckable);
	~Grabber(){ if(chuckable) chuckable->trackers.Remove(this);}
	Chuckable *chuckable;
	virtual int Drag(const float3 &v0, float3 v1,float3 *impact);
	virtual int HitCheck(const float3 &v0, float3 v1,float3 *impact);
	void notify(Entity*){chuckable=NULL;delete this;}
	String tooltip() { return chuckable->message; }
};

class Thrower : public Manipulator, public Tracker
{
  public:
	Thrower(Chuckable *,const float3 &_position);
	virtual int Drag(const float3 &v0, float3 v1,float3 *impact);
	virtual int DragEnd(const float3 &v0,const float3 &v1);
	virtual int HitCheck(const float3 &v0, float3 v1,float3 *impact);
	Spring *spring;
	Joint *joint;
	RigidBody *dummy;
	Chuckable *chuckable;
	float massprevious;          // used to restore the previous mass if we lighten the object while manipulating it
	virtual int Wheel(int w);
	virtual int KeyPress(int k);
	void Detach();
	void notify(Entity*){chuckable=NULL;delete this;}
	~Thrower(){ if(chuckable) chuckable->trackers.Remove(this);}
};

int Thrower::KeyPress(int k)
{
	extern int developermode;
	if(!developermode) return Manipulator::KeyPress(k);
	switch(k)
	{
	case '\b':
		delete chuckable;
		assert(currentmanipulator==NULL);  // deletion of this should be automatic
		//delete grabber;  
		//delete this;
		return 1;
	case 'l':
	case 'L':
		joint->SetActive(!joint->active);
		return 1;
	default:
		break;
	}
	return Manipulator::KeyPress(k);
}


float grabforcespring=50.0f;
EXPORTVAR(grabforcespring);
float grabforcejoint=50.0f;
EXPORTVAR(grabforcejoint);
float grabdamp=0.5f;
EXPORTVAR(grabdamp);
float grabmassratio=0.1f;
EXPORTVAR(grabmassratio);
int grabviaconstraint=0;
EXPORTVAR(grabviaconstraint);

Thrower::Thrower(Chuckable *_rb,const float3 &_position)
{
	extern Camera camera;
	chuckable = _rb;
	Array<Face*> faces;
	dummy=new RigidBody((WingMesh*)NULL,faces,_position);
	this->Position()= _position;
	dummy->resolve =0;
	spring = CreateSpring(chuckable,rotate(Inverse(chuckable->orientation),_position-chuckable->position),dummy,float3(0,0,0),grabforcespring);
	int bodyconstrained=0;  // check for existing constraints on the rigidbody that fix it to the world
	int bodyconnected=0;  // check for existing constraints on the rigidbody to any other rigidbody
	joint  = new Joint(chuckable);
	joint->SetPosition0(spring->anchorA);
	joint->SetPosition1(_position);
	joint->SetActive(grabviaconstraint &&   !bodyconstrained);  // disable joint by default and just use springgrab if its attached already.
	joint->maxforce = grabforcejoint;
	massprevious = chuckable->mass;
	if(joint->active && !bodyconnected && !bodyconstrained)
	{
		rbscalemass(chuckable,grabmassratio);
	}
//	if(chuckable->hash.Exists("grabholddist"))
//		this->Position() = camera.position+camera.orientation.zdir() * -chuckable->hash("grabholddist").Get().Asfloat();
	chuckable->trackers.Add(this);
}

int Thrower::Drag(const float3 &v0, float3 v1,float3 *impact)
{
	Manipulator::Drag(v0,v1,&v1);
	if(impact)*impact=v1;
	dummy->position = this->Position() = v1;
	joint->SetPosition1(v1);
	float dampleftover = powf((1.0f-grabdamp),DeltaT);
	chuckable->momentum *= dampleftover; 
	chuckable->rotation *= dampleftover;
	return 1;
}
int Thrower::Wheel(int w)
{
	Manipulator::Wheel(w);
	//manipv1 = manipv0 + (manipv1-manipv0) * powf(1.1f,(float)w);
	return 1;
}

void Thrower::Detach()
{
	if(spring) DeleteSpring(spring);
	spring=NULL;
	delete dummy;
	dummy=NULL;
	delete joint;
	joint=NULL;
}

int Thrower::HitCheck(const float3 &v0, float3 v1,float3 *impact)
{
	assert(0);
	return 0;
}
int Thrower::DragEnd(const float3 &v0,const float3 &v1)
{
	if(chuckable)
	{
		rbscalemass(chuckable,massprevious/chuckable->mass);
//		if(chuckable->hash.Exists("throwforce"))
//		{
//			extern void ApplyImpulse(RigidBody*,float3,float3,float);
//			ApplyImpulse(chuckable,float3(0,0,0),safenormalize(v1-v0),chuckable->hash("throwforce").Get().Asfloat());
//		}
	}
	Detach();
	delete this;
	return 0;
}

String pinit(String other)
{
	Thrower *thrower = dynamic_cast<Thrower*>(currentmanipulator);
	if(!thrower) return "no current chuckable";
	Joint *joint = new Joint(thrower->chuckable);
	joint->position0 = thrower->spring->anchorA;
	joint->position1 = joint->chuckable0->position + rotate(joint->chuckable0->orientation, joint->position0); // thrower->dummy->position;
	Chuckable *chuckable1 = ChuckableFind(other);
	if(chuckable1)
	{
		joint->chuckable1 = chuckable1;
		joint->position1 = rotate(Inverse(chuckable1->orientation), joint->position1 - chuckable1->position);
	}
	new JointManipulator(joint);
	return thrower->chuckable->id + " is now attached at " + String(Round(joint->position0,0.1f));
}
EXPORTFUNC(pinit);

String limittest(String s)
{
	float a =  (s=="") ? 20.0f : s.Asfloat() ;
	if(a<=2.0f) return "limit must be positive angle greater than 2 degrees";
    extern void createlimit(RigidBody *_rb,float a);
	Thrower *thrower = dynamic_cast<Thrower*>(currentmanipulator);
	if(!thrower) return "no current chuckable";
	createlimit(thrower->chuckable,DegToRad(  a));
	return thrower->chuckable->id + " should have limited rotation range now";
}
EXPORTFUNC(limittest);

Grabber::Grabber(Chuckable *_chuckable):chuckable(_chuckable)
{
	chuckable->trackers.Add(this);
}

int Grabber::Drag(const float3 &v0, float3 v1,float3 *impact)
{
	Manipulator::Drag(v0,v1,&v1);
	if(impact)*impact=v1;
	chuckable->rest=0;
	Thrower *thrower = new Thrower(chuckable,v1);
	currentmanipulator = thrower;
	assert(impact);
	*impact = thrower->Position();
	return 0;
}


int Grabber::HitCheck(const float3 &v0, float3 v1,float3 *impact)
{
	return chuckable->HitCheck(v0,v1,impact);
}

void GBB(Chuckable *c)
{
	new Grabber(c);
}

void CM(Chuckable *c)
{
	BSPNode n;
	n.isleaf=UNDER;
	n.brep.count=c->faces.count;
	n.brep.element=c->faces.element;
	float crease_angle = CREASE_ANGLE_DEFAULT;
//	if(c->hash.Exists("creaseangle"))
//	{
//		crease_angle = (float)c->hash["creaseangle"];
//	}
	c->model = CreateModel(&n,1); // c->hash["shadowcast"].Get().Asint());
	n.brep.count=0;
	n.brep.element=NULL;
}


void RenderChuckables()
{
	int i;
	for(i=0;i<Chuckables.count;i++)
	{
		Chuckable *c=Chuckables[i];
		int j;
		for(j=0;j<c->springs.count;j++)
		{
			Spring *s=c->springs[j];
			extern void Line(const float3 &,const float3 &,const float3 &color_rgb);
			Line(s->bodyA->position+rotate(s->bodyA->orientation,s->anchorA),
				 s->bodyB->position+rotate(s->bodyB->orientation,s->anchorB) , float3(0,1,1));
		}
		if(!c->render) continue;
		if(c->model)
		{
			c->model->position = c->position;   // why the redundancy?
			c->model->orientation=c->orientation;
			ModelRender(c->model);
		}
		else
		{
			BSPNode n;
			n.isleaf=UNDER;
			n.brep.count=c->faces.count;
			n.brep.element=c->faces.element;
			BSPRenderTest(&n,c->position,c->orientation);
			n.brep.count=0;
			n.brep.element=NULL;
		}
	}
}
int ChuckablesHitCheck(const float3 &v0, float3 v1,float3 *impact)
{
	*impact=v1;
	int h=0;
	for(int i=0;i<Chuckables.count;i++)
		h|=Chuckables[i]->HitCheck(v0,*impact,impact);
	return h;
}

String chuckdelall(String s)
{
	int i=Chuckables.count;
	String rv("deleted:  ");
	while(i--)
	{
		if(!strncmp(s,Chuckables[i]->id,s.Length()))
		{
			rv <<  Chuckables[i]->id << " ";
			delete Chuckables[i];
		}
	}
	return rv;
}
EXPORTFUNC(chuckdelall);

Chuckable* closest(const float3 &p)
{
	Chuckable *c=NULL;
	for(int i=0;i<Chuckables.count;i++)
	{
		if(!c || magnitude(Chuckables[i]->position-p)<magnitude(c->position-p))
			c=Chuckables[i];
	}
	return c;
}



//-------------- Character --------------

class Character : public Entity, public Pose
{
public:
	Model *model;
	Array<Chuckable*> bods; // should I use a different name here?
	Array<Joint*> joints;
	Character(String _name);
	~Character();
	void MakePhysical(xmlNode *mnode);
	void Drive();
	void Render();
};
Array<Character*> characters;
Character::Character(String _name):Entity(_name)
{
	characters.Add(this);
}
Character::~Character()
{
	characters.Remove(this);
}
int character_jointignoreall=0;
EXPORTVAR(character_jointignoreall);
void Character::MakePhysical(xmlNode *m)
{
	// i hate the way things are split between loading the physics-agnostic mesh/model and loading the rest of the stuff here
	int j;
	for(int i=0;i<model->skeleton.count;i++)
	{
		xmlNode *bnode = m->Child("skeleton")->children[i];
		assert(bnode);
		Bone *bone = model->skeleton[i];
		Array<Face*> faces;
		assert(bone->geometry);
		WingMeshToFaces(bone->geometry,faces);
		Chuckable *rb = new Chuckable(id + "_" + bone->id,bone->geometry,faces,bone->position);
		//rb->id = id + "_" + bone->id;
		if(bnode->Child("mass"))
			rbscalemass(rb,bnode->Child("mass")->body.Asfloat());
		xmlimport(rb,GetClass("Chuckable"),bnode);

		for(j=0;character_jointignoreall && j<bods.count;j++) 
		{
			rb->ignore.Add(bods[j]);
			bods[j]->ignore.Add(rb);
		}
		j=bods.count;
		while(j>=0 && model->skeleton[j]!= bone->parent) { j--; }
		if(j>=0)
		{
			assert(bone->parent);
			Chuckable *rb0 = bods[j];
			Joint *joint = new Joint(rb0,rb);
			joint->position0  = bone->position - rb0->com;
			joint->position1  = -rb->com;
			if(bnode->Child("directive"))
				joint->directive = bnode->Child("directive")->body.Asint();
			if(!character_jointignoreall)
			{
				rb0->ignore.Add(rb);  
				rb->ignore.Add(rb0);
			}
			joints.Add(joint);
		}
		bods.Add(rb);
	}
}

float animationspeed=0.0f;
EXPORTVAR(animationspeed);
int character_jointlimithack=0;
EXPORTVAR(character_jointlimithack);
float character_torque=1000.0f;
EXPORTVAR(character_torque);
void Character::Drive()
{
	int i,j;
	for(i=0;i<joints.count;i++)
	{
		Joint *joint = joints[i];
		for(j=0;j<bods.count;j++)
			if(bods[j]==joint->chuckable1) break;
		assert(j<bods.count);
		extern Pose GetPose(const Array<KeyFrame> &animation,float t);
		Bone *b = model->skeleton[j];
		extern float GlobalTime;
		float t = fmodf(GlobalTime*animationspeed,b->animation[b->animation.count-1].time);
		Quaternion qdrive = GetPose(b->animation,t).orientation;

// for rendering only skinning hack bugfix aaarrrgggg:
//if(dot(joint->chuckable0->orientation,joint->chuckable1->orientation)<0)
// joint->chuckable1->orientation = - joint->chuckable1->orientation;
		extern void createdrive(RigidBody *rb0,RigidBody *rb1,Quaternion target,float maxtorque);
		createdrive(joint->chuckable0,joint->chuckable1,qdrive,character_torque);
		extern void createlimits(RigidBody *rb0,RigidBody *rb1, const float3 &axis, const float coneangle);
		extern void createlimits2(RigidBody *rb0,RigidBody *rb1, const float3 &axis, const float3 &rxmax);
		if(character_jointlimithack)
			createlimits(joint->chuckable0,joint->chuckable1, float3(0,0,0),0);
		if(joint->directive==1)   // just prototyping some ideas here
		{
			createlimits(joint->chuckable0,joint->chuckable1, float3(1,0,0),DegToRad(10.0f));
		}
		if(joint->directive==2)
		{
			createlimits(joint->chuckable0,joint->chuckable1, float3(1,0,0),DegToRad(0.0f));
		}
			
	}
}

int rendercharacterbones=0;
EXPORTVAR(rendercharacterbones);
void Character::Render()
{
//	model->modelmatrix = MatrixFromQuatVec(bods[0]->orientation,bods[0]->position);  // dont like this, but modelmatrix is only used for finding lights for skinned meshes
model->position = float3(0,0,0);
model->orientation = Quaternion(0,0,0,1.0f); 
	for(int i=0;i<bods.count;i++)
	{
		Bone *b = model->skeleton[i];
		b->modelpose.orientation = bods[i]->orientation;
		b->modelpose.position    = bods[i]->position + rotate(bods[i]->orientation,-bods[i]->com);
		Pose p = b->modelpose * Inverse( b->basepose );
		model->currentpose[i]  = /* model->modelmatrix */ MatrixFromQuatVec(p.orientation,p.position);
		model->currentposep[i] =  p.position;
		model->currentposeq[i] =  p.orientation;

		bods[i]->render = rendercharacterbones;
	}

	void ModelMeshSkin(Model *model);
	ModelMeshSkin(model);

	if(!rendercharacterbones) ModelRender(model);
}

void DoCharacters()
{
	for(int i=0;i<characters.count;i++)
		characters[i]->Drive();
}
void CharactersRender()
{
	for(int i=0;i<characters.count;i++)
		characters[i]->Render();
}
 
Character *CharacterSpawn(String filename,float3 offset)
{
	xmlNode *e = NULL;
	String f=filefind(filename,"models",".xml .ezm .chr");
	if(f=="")return NULL;
	e=XMLParseFile(f);
	assert(e);
	if(!e)return NULL;
	xmlNode *mnode = (e->tag=="model")?e:e->Child("model");
	if(mnode==NULL)
	{
		delete e; 
		return NULL;
	}
	Model *model = ModelLoad(mnode);
	if(!model) return NULL;;
	Character *c = new Character(splitpathfname(filename));
	c->model = model;
	c->MakePhysical(mnode);
	for(int i=0;i<c->bods.count;i++) 
		c->bods[i]->position += offset;

	delete e;
	return c;
}
String chr(String param)
{
	String filename = PopFirstWord(param);
	extern float3 focuspoint;
	float3 offset = focuspoint;
	StringIter(param)>>offset;
	Character *c = CharacterSpawn(filename,offset);
	if(!c) return "unable to create character from file";
	return c->id + " loaded";
}
EXPORTFUNC(chr);
