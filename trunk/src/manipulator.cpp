//  
// Manipulators
// by: s melax (c) 2003-2007
// 
// if you like to think of that model-view-controller then these things are the "controller" 
// i.e. the UI shim that lets me pull, push, rotate, edit, or activate some sort of object in the scene
//  


#define NOMINMAX
#include <assert.h>

#include "array.h"
#include "winsetup.h"
#include "console.h"
#include "mesh.h"
#include "manipulator.h"
#include "manipulatorI.h"
#include "bsp.h"


static Model *tag;

Array<Manipulator*> Manipulators;
Manipulator *currentmanipulator=NULL;
float3 manipv0,manipv1;
float3 manipv0_start,manipv1_start;
static int dragging=0;

Manipulator::Manipulator():positiona(0,0,0),orientationa(0,0,0,1.0f)
{
	Manipulators.Add(this);
	drawit=1;
	mode=0;
}

Manipulator::~Manipulator()
{
	if(currentmanipulator==this) currentmanipulator=NULL;
	Manipulators.Remove(this);
}

int Manipulator::HitCheck(const float3 &v0, float3 v1,float3 *impact)
{
	return BoxIntersect(v0,v1,Bmin(),Bmax(),impact);
}

int Manipulator::InDrag()
{
	return (currentmanipulator==this && dragging);
}

Manipulator* ManipulatorHit(const float3 &v0, float3 v1,float3 *impact)
{
	Manipulator *mh=NULL;
	if(dragging && currentmanipulator)
	{
		currentmanipulator->DragEnd(manipv0,manipv1);
		currentmanipulator=NULL;
	}
	dragging=0;
	int i;
	i=Manipulators.count;
	while(i--) 
	{
		Manipulator *m=Manipulators[i];
		if(m->HitCheck(v0,v1,&v1))
		{
			mh = m;
		}
	}
	if(mh&&impact) *impact = v1;
	currentmanipulator=mh;
	manipv0_start=manipv0=v0;
	manipv1_start=manipv1=v1;
	return mh;
}

int Manipulator::DragStart(const float3 &v0, const float3 &v1)
{
	return 1;
}

int Manipulator::DragEnd(const float3 &v0, const float3 &v1)
{
	return 1;
}

int Manipulator::Drag(const float3 &v0, float3 v1,float3 *impact)
{
	v1 = v0+ (v1-v0) * magnitude(manipv1-manipv0) / magnitude(v1-v0);
	extern int shiftdown;
	extern int ctrldown;
	if(ctrldown)
	{
		ScaleIt(vabs(rotate(Inverse(Orientation()),v1-Center()))-vabs(rotate(Inverse(Orientation()),manipv1-Center())));
	}
	else if(!shiftdown)
		Position() += v1-manipv1;
	else
		Orientation() = VirtualTrackBall(v0,Center(),manipv1-v0,v1-v0) * Orientation();
	*impact=v1;
	PositionUpdated();
	return 1;
}

int Manipulator::Wheel(int w)
{
	if(!InDrag())
	{
		mode = (w>0);
		return 1;
	}
	float3 manipv1old = manipv1;
	// manipv1 = manipv0 + (manipv1-manipv0) * powf(a.Asfloat(),DeltaT); // from former pushpull console command
	manipv1 = manipv0 + (manipv1-manipv0) * powf(1.1f,(float)w);
	Position() += manipv1-manipv1old;
	PositionUpdated();
	return 1;
}


int PlanarManipulator::Drag(const float3 &v0, float3 v1,float3 *impact)
{
	v1 = PlaneLineIntersection(plane,v0,v1);
	manipv1 = PlaneLineIntersection(plane,manipv0,manipv1);
	Position() += v1-manipv1;
	Position() = PlaneProject(plane,Position());
	*impact=v1;
	return 1;
}

Manipulator* ManipulatorDrag(const float3 &v0, float3 v1,float3 *impact)
{
	if(!currentmanipulator) return NULL;
	if(!dragging)
	{
		currentmanipulator->DragStart(v0,v1);
		if(!currentmanipulator) return NULL;  // in case we delete ourselves here
		dragging=1;
	}
	currentmanipulator->Drag(v0,v1,&v1);

	manipv0=v0;
	manipv1=v1;
	if(impact) *impact=v1;
	return currentmanipulator;
}

int ManipulatorKeyPress(int k)
{
	if(!currentmanipulator) return 0;
	return currentmanipulator->KeyPress(k);
}
int ManipulatorWheel(int w)
{
	if(!currentmanipulator) return 0;
	return currentmanipulator->Wheel(w);
}
int ManipulatorKeysGrab()
{
	if(!currentmanipulator) return 0;
	return currentmanipulator->KeysGrab();
}

void Manipulator::Render()
{
	DrawBox(Bmin(),Bmax(),float3(0,1,0));
}

void ManipulatorRender()
{
	int i;
	float4x4 world(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1);
	for(i=0;i<Manipulators.count;i++)
	{
		Manipulator *m=Manipulators[i];
		if(!m->drawit) continue;
		m->Render();
	}
}



Manipulator* ManipulatorNew(const float3 &p)
{
	Manipulator *m = new Manipulator();
	m->Position() = p;
	return m;
}

class float3Manipulator :public Manipulator
{
public:
	float3 *position;
	float3Manipulator(float3 *_pos):position(_pos){}
	float3 &Position(){return *position;}
};

Manipulator* ManipulatorNew(float3 *p)
{
	Manipulator *m = new float3Manipulator(p);
	return m;
}

class VertexManipulator: public Manipulator
{
  public:
	Face *face;
	int v;
	VertexManipulator(Face *_face,int _v):face(_face),v(_v){}
	float3& Position(){return face->vertex[v];}
};
class TexSlideManipulator : public PlanarManipulator
{
public:
	Face *face;
	TexSlideManipulator(Face *_face):PlanarManipulator(*_face),face(_face){Position()=FaceCenter(face);}
	int Drag(const float3 &v0,float3 v1,float3* impact){PlanarManipulator::Drag(v0,v1,&v1);*impact=v1;face->ot.x-=dot(v1-manipv1,face->gu);face->ot.y-=dot(v1-manipv1,face->gv);return 1;}
};


String tooltip(String)
{
	if(!currentmanipulator) return "";
	return currentmanipulator->tooltip();
}
EXPORTFUNC(tooltip);




char *testo(const char*)
{
	extern BSPNode *area_bsp;
	Array<BSPNode *>stack;
	stack.Add(area_bsp);
	while(stack.count)
	{
		BSPNode *n=stack.Pop();
		if(!n)continue;
		stack.Add(n->over);
		stack.Add(n->under);
		for(int i=0;i<n->brep.count;i++)
		{
			Face *f=n->brep[i];
			for(int j=0;j<f->vertex.count;j++)
			{
				new VertexManipulator(f,j);
			}
			new TexSlideManipulator(f);
		}
	}
	return "OK";
}
EXPORTFUNC(testo);


void ManipulatorInit()
{
	//tag = ModelLoad("tinybox.xml");
	//assert(tag);
	// were used for testing
	//ManipulatorNew(float3(0,0,1));
	//ManipulatorNew(float3(0,1,1));
	//ManipulatorNew(float3(1,1,1));
}
