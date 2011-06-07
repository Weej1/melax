//
//    light
//
//  This cpp module encapsulates our light objects and the manipulators for them.
//  
//

#include "light.h"
#include "console.h"
#include "manipulatori.h"
#include "mesh.h"
#include "camera.h"

extern Camera camera;
extern float DeltaT;


int lightmanipulatorsenable=1;
EXPORTVAR(lightmanipulatorsenable);

float lightmarkersize=0.02f;  // radius of box
EXPORTVAR(lightmarkersize); 

class LightManipulator: public Manipulator , public Tracker 
{
public:
	Light *light;
	float3 Bmin(){return Position()-float3(1.0f,1.0f,1.0f)*lightmarkersize;}
	float3 Bmax(){return Position()+float3(1.0f,1.0f,1.0f)*lightmarkersize;}
	float3 &Position(){return light->position;}
	int HitCheck(const float3 &v0,float3 v1,float3 *impact){ return (lightmanipulatorsenable && Manipulator::HitCheck(v0,v1,impact));}
	LightManipulator(Light *_light):light(_light){light->trackers.Add(this);}
	~LightManipulator(){if(light)light->trackers.Remove(this);}
	void Render() {	if(lightmanipulatorsenable) DrawBox(Bmin(),Bmax(),light->color);}
	void notify(Entity *obj){light=NULL;delete this;}
	int KeyPress(int k);
};
int LightManipulator::KeyPress(int k)
{
	if(k=='\b')
	{
		delete light;
		// delete this;  // should happen automatically
		assert(currentmanipulator==NULL);
		return 1;
	}
	switch(k)
	{
		case 'c':
			light->shadowcast = ! light->shadowcast;
			return 1;
		case '[':
		case '{':
			light->radius = Max(1.0f,light->radius-1.0f);
			return 1;
		case ']':
		case '}':
			light->radius +=1.0f;
			return 1;
		case 'r':
			light->color.x += 0.2f;
			return 1;
		case 'R':
			light->color.x = Max(0.0f,light->color.x-0.2f);
			return 1;
		case 'g':
			light->color.y += 0.2f;
			return 1;
		case 'G':
			light->color.y = Max(0.0f,light->color.y-0.2f);
			return 1;
		case 'b':
			light->color.z += 0.2f;
			return 1;
		case 'B':
			light->color.z = Max(0.0f,light->color.z-0.2f);
			return 1;
	}
	return 0;
}

String currentlight(String)
{
	LightManipulator *m = dynamic_cast<LightManipulator*>(currentmanipulator);
	if(!m) return "";
	Light *light = m->light;
	return light->id + " at: " + String(Round(light->position,0.1f)) +" color: " + String(light->color) + 
	       "  radius " + String(light->radius) + ((light->shadowcast)?" shadow ":" noshadow");
}
EXPORTFUNC(currentlight);


LDECLARE(Light,enable);
LDECLARE(Light,position);
LDECLARE(Light,radius);
LDECLARE(Light,color);
LDECLARE(Light,shadowcast);
	
Array<Light*> lights;
Light::Light():Entity("lite")
{
	radius = 5.0f;
	color  = float3(1.0f,1.0f,1.0f);
	shadowcast = 1;
	enable=1;
	extern lobj *rv_lobj;
	rv_lobj = LEXPOSEOBJECT(Light,this->id);
	EXPOSEMEMBER(enable);
	EXPOSEMEMBER(position);
	EXPOSEMEMBER(radius);
	EXPOSEMEMBER(color);
	EXPOSEMEMBER(shadowcast);
	new LightManipulator(this);
	lights.Add(this);
}
Light::~Light()
{
	lights.Remove(this);
	// get rid of its manipulator too??
}


Light *LightImport(xmlNode *n)
{
	assert(n->tag == "light");
	//const char *name = n->attribute("name");
	Light *light = new Light();
	xmlimport(light,GetClass("light"),n);
	return light;
}
xmlNode *LightExport(Light *light)
{
	xmlNode *e = xmlexport(light,GetClass("light"),"light");
	return e;
}
int LightsLoad(xmlNode *scene)
{
	if(!scene) return 0;
	if(scene->tag=="light") 
	{
		LightImport(scene);  // the node passed in is a light node, not a container with a number of lights
		return 1;
	}
	int i;
	int k=0;
	for(i=0;i<scene->children.count;i++)
	{
		xmlNode *c = scene->children[i];
		if(c->tag != "light") continue;
		LightImport(c);
		k++;
	}
	return k;  // number of lights created.
}

int LightExportAll(xmlNode *scene_output)
{
	for(int i=0;i<lights.count;i++)
	{
		scene_output->children.Add(LightExport(lights[i]));
	}
	return lights.count;
}

String lettherebelight(String)
{
	Light *light=new Light();
	light->position = camera.position+camera.orientation.zdir() * -1.5f;
	return light->id;  // should get overridden now
}
EXPORTFUNC(lettherebelight);

String lightsenable(String param)
{
	int i;
	for(i=0;i<lights.count;i++)
	{
		lights[i]->enable = param.Length() ? !strncmp(lights[i]->id,param,param.Length()) : 1;
	}
	return param + " lights turned on!";
}
EXPORTFUNC(lightsenable);

class Spline :public Entity
{
public:
	Array<float3> points; // controlpoints
	float interval;  // its length in parametric space i.e. 0 <= t <= interval
	float3 bmin,bmax;
	int loop;
	Spline();
	~Spline();
	void CalcBBox();
	float3 Position(float t);
};

Array<Spline*> Splines;
Spline::Spline():Entity("spline")
{
	EXPOSEMEMBER(loop);
	EXPOSEMEMBER(interval);
	loop=0;
	interval=1.0f; // parametric length of the curve.
	Splines.Add(this);
}
Spline::~Spline()
{
	Splines.Remove(this);
}

void Spline::CalcBBox()
{
	BoxLimits(points.element,points.count,bmin,bmax);
}
float3 Spline::Position(float t)
{
	t/=interval;  // put t into range [0..1]
	if(points.count==0) return float3(0,0,0);
	if(points.count==1) return points[0];
	if(loop && t>=1.0f || t<0.0f) 
	{
		t = fmodf(t,1.0f);  
		if(t<0.0f) t+= 1.0f;
	}
	if(!loop) t = clamp(t ,0.0,1.0f);
	float tg = t * ((float)points.count - ((loop)?0.0f:1.0f));
	int interval = (int) floorf(tg);
	float ts = tg - floorf(tg);
	return points[interval] * (1.0f-ts) + points[(interval+1)%points.count] * ts;
}

float splinepointsize=0.02f; // half extent or radius of box around each control point

class SplineManipulator: public Manipulator , public Tracker
{
  public:
    Spline *spline;
	int index;
	//int indrag; // should be in manipulator base class
	float3 previous;
	SplineManipulator(Spline *_spline):spline(_spline){spline->trackers.Add(this);}
	~SplineManipulator(){if(spline)spline->trackers.Remove(this);}
	void notify(Entity*){spline=NULL; delete this;}
	float3& Position(){assert(index>=0); return spline->points[index];}
	//int Drag(const float3 &v0, float3 v1,float3 *impact){*v=previous;Manipulator::Drag(v0,v1,impact);if(previous!=*v){cloth->Wake();previous=*v;}return 1;}
	//int DragStart(const float3 &v0, const float3 &v1){cloth->S.blocks.Add(float3Nx3N::Block(index,index));previous=*v;indrag=1;return Manipulator::DragStart(v0,v1);}
	int DragEnd(const float3 &v0, const float3 &v1){spline->CalcBBox(); return Manipulator::DragEnd(v0,v1);}
	//virtual int Wheel(int w){Manipulator::Wheel(w); if(!InDrag()) return 0; previous=*v; return 1;}
	virtual float3 Bmin() { return spline->bmin;}
	virtual float3 Bmax() { return spline->bmax;}
	virtual void Render()
	{
		Array<float3> &points = spline->points;
		float3 be=float3(1.0f,1.0f,1.0f)*splinepointsize;
		for(int i=0;i<points.count;i++)
		{
			DrawBox(points[i]-be,points[i]+be,(i==index)?float3(1,1,1):float3(1,0.5,0));
			if(i||spline->loop) Line(points[i],points[(i+points.count-1)%points.count],float3(1.0f,0,0));
		}
	};
	int HitCheck(const float3 &v0, float3 v1,float3 *impact)
	{
		index=-1;
		Array<float3> &points = spline->points;
		float3 be=float3(1.0f,1.0f,1.0f)*splinepointsize;
		for(int i=0;i<points.count;i++)
		{
			if(BoxIntersect(v0,v1,points[i]-be,points[i]+be,&v1))
			{
				index = i;
				if(impact) *impact=v1;
			}
		}
		return (index>=0);
	}
	int KeyPress(int k)
	{
		assert(index>=0);
		if(k=='\b')
		{
			spline->points.DelIndex(index);
			index=-1;
			if(spline->points.count==0)
			{
				delete spline;  // should cause this manipulator to also be deleted.
			}
			return 1;
		}
		if(k=='+' || k=='=')
		{
			Array<float3> &points = spline->points;
			points.Insert((points[index]+points[(index+1)%points.count])*0.5f,index+1);
			return 1;
		}
		if(k=='l' || k=='L')
		{
			spline->loop = !spline->loop;
			return 1;
		}
		return 0;
	}
};

String currentspline(String)
{
	SplineManipulator *sm = dynamic_cast<SplineManipulator*>(currentmanipulator);
	if(!sm) return "";
	assert(sm->spline);
	Spline *s = sm->spline;
	return s->id + " with " + String(s->points.count) + " points with interval 0.." + String(s->interval) + ((s->loop)?" looped ":" unlooped");
}
EXPORTFUNC(currentspline);

xmlNode *SplineExportXML(Spline *spline)
{
	xmlNode *n = new xmlNode("spline");
	//ObjectExport(spline,n);
	Array<float3> &points = spline->points;
	// XMLEXPORTARRAY(points);	
	xmlNode *p = new xmlNode("points",n);
	p->attribute("count") = String(points.count);
	for(int i=0;i<points.count;i++)
	{
		p->body << points[i] << ",   ";
	}
	return n;
}


int SplineExportAll(xmlNode *scene)
{
	for(int i=0;i<Splines.count;i++)
	{
		scene->children.Add(SplineExportXML(Splines[i]));
	}
	return Splines.count;
}

Spline *SplineImport(xmlNode *n)
{
	assert(n->tag == "spline");
	String name = n->attribute("name");
	Spline *spline = new Spline();
	if(name!="") spline->id = name;
	for(int i=0;i<n->children.count;i++)
	{
		xmlNode *c = n->children[i];
		if(c->tag=="points")
		{
			spline->points.SetSize(c->attribute("count").Asint());
			ArrayImport(spline->points,c->body,c->attribute("count").Asint());
			spline->CalcBBox();
			continue;
		}
		assert(0);
	}
	return spline;
}

int SplinesLoad(xmlNode *scene)
{
	if(scene->tag=="spline") // caller sent us a spline node directly instead of a container
	{
		SplineImport(scene); 
		return 1;
	}
	int count=0;
	for(int i=0;i<scene->children.count;i++)
	{
		xmlNode *c = scene->children[i];
		if(c->tag != "spline") continue;
		Spline *spline = SplineImport(c);
		count++;
	}
	return count;
}



String splinesave(String s)
{
	SplineManipulator *sm = dynamic_cast<SplineManipulator*>(currentmanipulator);
	if(!sm) return "No current spline";
	Spline *spline = sm->spline;
	String filename = (s!="")? s : "testspline.spline";
	xmlNode *n = SplineExportXML(spline);
	XMLSaveFile(n,filename);
	delete n;
	return spline->id + " saved into file: " + filename;
}
EXPORTFUNC(splinesave);

String splinespawn(String s)
{
	int n=(s!="")?s.Asint():4;
	Spline *spline=new Spline();
	new SplineManipulator(spline);
	float3 position = camera.position+camera.orientation.zdir() * -1.5f;
	for(int i=0;i<n;i++)
		spline->points.Add(position+float3(0,0,(float)i/(float)n));
	return "created spline";
}
EXPORTFUNC(splinespawn);

String spline(String filename)  // load a .spline file
{
	xmlNode *n = XMLParseFile(filename);
	if(!n) return "failed to parse file";
	Spline *spline = SplineImport(n);
	delete n;
	return String("spline loaded: ") + spline->id ;
}
EXPORTFUNC(spline);

String splinesnoedit(String s)
{
	int i=Manipulators.count;
	while(i--)
		delete dynamic_cast<SplineManipulator*>(Manipulators[i]);  // if its a spline manipulator it gets deleted otherwise not
	return "spline editing disabled";
}
EXPORTFUNC(splinesnoedit);
String splinesedit(String s)
{
	splinesnoedit("");  // remove current spline manipulators since we are creating new ones anyways
	for(int i=0;i<Splines.count;i++)
		new SplineManipulator(Splines[i]);
	return "spline editing enabled";
}
EXPORTFUNC(splinesedit);

class Animation
{
public:
	Spline *spline;
	float3 *animatand;
	float speed;
	float time;
	int loop;
	Animation(Spline *_spline,float3 *_target);
	~Animation();
	void Update(float dt);
};
Array<Animation*> animations;
Animation::Animation(Spline *_spline,float3 *_target):spline(_spline),animatand(_target)
{
	time =0.0f;
	speed=1.0f;
	loop=spline->loop;
	animations.Add(this);
}
Animation::~Animation()
{
	animations.Remove(this);
}
void Animation::Update(float dt)
{
	time += dt*speed;
	if(time > spline->interval) 
		time=(loop)? fmodf(time,spline->interval) : spline->interval;
	*animatand = spline->Position(time);
	if(time>=spline->interval && !loop) 
		delete this;
}

void AnimationUpdate()
{
	int i=animations.count;
	while(i--)
	{
		animations[i]->Update(DeltaT);
	}
}

String animate(String s)
{
	String splinename = PopFirstWord(s," ");
	Spline *spline = NULL; // need a solution here!!  SplineFind(splinename);
	if(!spline) return String("didn't find a spline with name ") + splinename;
	assert(0);
	//extern Reference GetObjectVar(String varname); 
	//Reference r = (IsOneOf('.',s)) ? GetObjectVar(s) : GetVar(s);
	//if(r.type!=3 || !r.data) return "not a valid target to animate";
	//SplineManipulator *sm = dynamic_cast<SplineManipulator*>(currentmanipulator);
	//if(!sm) return "No current spline";
	//Animation *a = new Animation(spline,(float3*)r.data);
	return spline->id + " is controlling " + s;
}
EXPORTFUNC(animate);

