




#include <assert.h>
#include <stdio.h>
#include <d3dx9.h>
#include "brush.h"
#include "winsetup.h"
#include "array.h"
#include "camera.h"
#include "vecmath.h"
#include "bsp.h"
#include "wingmesh.h"
#include "vertex.h"
#include "console.h"
#include "material.h"
#include "qplane.h"
#include "manipulatorI.h"
#include "mesh.h"
#include "physics.h"
#include "chuckable.h"

extern Camera camera;


int currentmaterial=0;
EXPORTVAR(currentmaterial);
int qsnapenable=1;
EXPORTVAR(qsnapenable);

void BSPRenderTest(BSPNode *bsp,const float3 &p,const Quaternion &q,int shadow=1,int matid_override=-1);


class BrushManipulator : public Manipulator , public Tracker
{
public:
	Brush *brush;
	BrushManipulator(Brush *_brush);
	~BrushManipulator();
	virtual int  HitCheck(const float3 &v0, float3 v1,float3 *impact);
	virtual int  Drag(const float3 &v0, float3 v1,float3 *impact);
	virtual int  DragStart(const float3 &v0, const float3 &v1);
	virtual int  DragEnd(const float3 &v0, const float3 &v1);
	virtual void Render();
	BSPNode *currentplane;
	BSPNode *currentleaf;
	BSPNode *currentoverleaf;
	float3 position_start;
	Plane lastplane;
	Face *facehit;
	Array<BSPNode *> backup;
	int firstdrag;
	Quaternion cumulativeq;
	int Wheel(int w);
	int KeyPress(int k);
	int active;
	float3 impact_local; // last impact on this manipulator in brush local space
	void notify(Entity*){brush=NULL;delete this;}
};


Array<BrushManipulator*> BrushManipulators;

BrushManipulator::BrushManipulator(Brush *_brush):brush(_brush)
{
	BrushManipulators.Add(this);
	active=0;
	brush->trackers.Add(this);
}

BrushManipulator::~BrushManipulator()
{
	if(currentmanipulator==this) currentmanipulator=NULL;
	BrushManipulators.Remove(this);
	if(brush) brush->trackers.Remove(this);
}

int brushmanipcollidables=0;
EXPORTVAR(brushmanipcollidables);

int BrushManipulator::HitCheck(const float3 &_v0, float3 v1, float3 *impact)
{
	firstdrag=0;
	cumulativeq = Quaternion();
	if(!brush->collide && !brushmanipcollidables) return 0;  // dont select non collidable brushes (likely invisible)
	float3 v0 = _v0-brush->position; // xfer to local space
	v1 -= brush->position;
	int h=::HitCheckSolidReEnter(brush->bsp,v0,v1,&v1);
	if(h) {
		extern BSPNode *HitCheckNode;
		currentplane = HitCheckNode;
		extern BSPNode *HitCheckNodeHitLeaf;
		currentleaf = HitCheckNodeHitLeaf;
		extern BSPNode *HitCheckNodeHitOverLeaf;
		currentoverleaf = HitCheckNodeHitOverLeaf;
		lastplane = Plane(currentplane->normal(),currentplane->dist());
		impact_local = v1;
		*impact = v1+brush->position;
		position_start = brush->position;
		extern Face* FaceHit(const float3& s); 
		facehit = FaceHit(v1);
		if(facehit) currentmaterial = facehit->matid;
	}
	return h;
}



void FaceQSnap(Face *face)
{
	int i;
	*(Plane*)face = Quantized(*(Plane*)face);
	for(i=0;i<face->vertex.count;i++)
	{
		face->vertex[i] = PlaneProject(*(Plane*)face,face->vertex[i]);
	}
}


void BSPQSnap(BSPNode *n)
{
	int i;
	if(!n ) {
		return;
	}
	if(!n->isleaf)
	{
		*(Plane*)n = Quantized(*(Plane*)n);
	}
	if(n->convex) {
		delete n->convex;
		n->convex = NULL;
	}
	for(i=0;i<n->brep.count;i++) {
		FaceQSnap(n->brep[i]);
	}
	BSPQSnap(n->under);
	BSPQSnap(n->over);
}



BrushManipulator *CurrentBrush()
{
	return dynamic_cast<BrushManipulator*>(currentmanipulator);
	// A non-RTTI implementation:
	//for(int i=0;i<BrushManipulators.count;i++) 
	//	if(BrushManipulators[i] == currentmanipulator)
	//		return BrushManipulators[i];
	//return NULL;

}



int currentbrushmode;
EXPORTVAR(currentbrushmode);
static char *brush_edit_modes[]=
{
	"translate entire object",
	"translate plane",
	"rotate plane",
	"grab and move plane",
	"rotate entire brush",
	"scale texture",
	"trans texture",
	"rotate texture",
};
#define NUM_BRUSH_EDIT_MODES (sizeof(brush_edit_modes)/sizeof(char*))
String brushmode(String)
{
	currentbrushmode %=NUM_BRUSH_EDIT_MODES;
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush || !currentbrush->active) return "";
	return String(brush_edit_modes[currentbrushmode]); // + "  currentbrush: " + currentbrush->brush->id;
}
EXPORTFUNC(brushmode);

String currentbrush(String)
{
	if(!CurrentBrush()) return "";
	return CurrentBrush()->brush->id;
}
EXPORTFUNC(currentbrush);


int BrushManipulator::Wheel(int w)
{
	if(!InDrag() && active)
	{
		currentbrushmode += w;
		currentbrushmode  = max(0,currentbrushmode);
		currentbrushmode  = min(currentbrushmode,NUM_BRUSH_EDIT_MODES-1);
		return 1;
	}
	// else
	return Manipulator::Wheel(w);
}



int BrushManipulator::Drag(const float3 &_v0, float3 v1,float3 *impact)
{
	if(!active) return 0;
	assert(NUM_BRUSH_EDIT_MODES==8);
	currentbrushmode %=NUM_BRUSH_EDIT_MODES;
	float3 v0old = manipv0-brush->position;
	float3 v1old = manipv1-brush->position;
	float3 v0 = _v0-brush->position;
	v1-=brush->position;
	if(currentbrushmode==5 || currentbrushmode==6 || currentbrushmode==7)
	{
		v1 = PlaneLineIntersection(*this->facehit,v0,v1);
		float2 texcoord = FaceTexCoord(this->facehit,v1old);
		if(currentbrushmode==5)
		{
			this->facehit->gu *= dot(v1old,this->facehit->gu)/dot(v1,this->facehit->gu);
			this->facehit->gv *= dot(v1old,this->facehit->gv)/dot(v1,this->facehit->gv);
		}
		else if (currentbrushmode==6)
		{
			assert(currentbrushmode==6);
			this->facehit->ot.x += dot(v1old,this->facehit->gu)-dot(v1,this->facehit->gu);
			this->facehit->ot.y += dot(v1old,this->facehit->gv)-dot(v1,this->facehit->gv);
		}
		else
		{
			assert(currentbrushmode==7);
			float3 rold = v1old - PlaneProject(*this->facehit,this->facehit->ot);
			float3 rnew = v1    - PlaneProject(*this->facehit,this->facehit->ot);
			Quaternion q = RotationArc(rold,rnew);
			this->facehit->gu = rotate(q, this->facehit->gu);
			this->facehit->gv = rotate(q, this->facehit->gv);
			assert(currentbrushmode==7);
		}
		impact_local = v1;
		*impact = v1+brush->position;
		return 1;
	}
	v1 = v0 + (v1-v0) * (magnitude(v1old-v0old)/magnitude(v1-v0));
	impact_local = v1;
	*impact = v1+brush->position;
	if(currentbrushmode==0)
	{
		brush->position = Round(position_start+ v1+brush->position - manipv1_start  ,0.25f);
		return 1;
	}
	if(currentbrushmode==4)
	{
		float3 n = safenormalize(cross(v1old,v1));
		Quaternion q = QuatFromAxisAngle(n,magnitude(v1old-v1)/magnitude(v1old));
		cumulativeq = q*cumulativeq;
		if(!firstdrag) backup.Add(BSPDup(brush->bsp));
		else 
		{
			delete brush->bsp;
			brush->bsp = BSPDup(backup[backup.count-1]);
		}
		BSPRotate(brush->bsp,cumulativeq);
		if(qsnapenable)BSPQSnap(brush->bsp);
		currentplane = currentleaf = currentoverleaf = NULL;
		Retool(brush);
		firstdrag=1;
		return 1;
	}
	Quaternion q = RotationArc(safenormalize(v1old-v0old),safenormalize(v1-v0));
	cumulativeq = q*cumulativeq;
	if(currentbrushmode==2)
	{
		float3 c = manipv1_start-brush->position + normalize(manipv1_start - manipv0_start)*0.25f;
		q = RotationArc(normalize(v1old-c),normalize(v1-c));
	}
	lastplane.normal() = rotate(q,lastplane.normal());
	if(currentbrushmode==0 || currentbrushmode==2) currentplane->normal() = Quantized(lastplane.normal());
	currentplane->dist() = -dot((currentbrushmode==2)?manipv1_start-brush->position:v1,currentplane->normal());
	*(Plane*)currentplane = Quantized(*currentplane);
	Retool(this->brush);
	return 1;
}

int BrushManipulator::DragStart(const float3 &v0, const float3 &v1)
{
	if(!active) 
	{
		FuncInterp(brush->onclick);
		return 1;
	}
	if(currentbrushmode!=0 && brush->model)
	{
		brush->Modified();
		//ModelDelete(brush->model);
		//brush->model=NULL;
	}
	return 1;
}
int BrushManipulator::DragEnd(const float3 &v0, const float3 &v1)
{
	if(!active) return 1;
	if(brush->model) return 1;  //because it was just an update in position, no modification
	brush->Modified();
	brush->model = CreateModel(brush->bsp,brush->shadowcast);
	return 1;
}


String brushplane(const char*)
{
	BrushManipulator *brush = CurrentBrush();
	if(!brush|| !brush->active) return "";
	extern Plane CubeProjected(const Plane &p);
	if(!brush->currentplane) return "no current plane";
	Plane p= CubeProjected(*(Plane*)brush->currentplane);
	String s;
	s.sprintf("%2g %2g %2g  %g  ",p.x,p.y, p.z,p.dist());
	return s;
}
EXPORTFUNC(brushplane);

String brushposition(const char*)
{
	BrushManipulator *brush = CurrentBrush();
	if(!brush|| !brush->active) return "";
	return String(brush->brush->position);
}
EXPORTFUNC(brushposition);


String hitpoint(const char*)
{
	BrushManipulator *brush = CurrentBrush();
	if(!brush || !brush->active|| !brush->facehit) return "";
	String s;
	s << "xyz=" << brush->impact_local << "  uv=" << FaceTexCoord(brush->facehit,brush->impact_local) ; 
	return s;
}
EXPORTFUNC(hitpoint);

char *brushrotate(String s)
{
	BrushManipulator *brush = CurrentBrush();
	if(!brush) return "no current brush";
	float y,p,r;
	y=p=r=0;
	int rc;
	rc = sscanf_s(s,"%f%f%f",&y,&p,&r);
	if(rc<=0) return "usage:  brushrotate yaw pitch roll";
	brush->backup.Add(BSPDup(brush->brush->bsp));
	BSPRotate(brush->brush->bsp,YawPitchRoll(y,p,r));
	if(qsnapenable)BSPQSnap(brush->brush->bsp);
	Retool(brush->brush);
	return "OK";
}
EXPORTFUNC(brushrotate);


char *brushtranslate(String s)
{
	BrushManipulator *brush = CurrentBrush();
	if(!brush) return "no current brush";
	float3 t(0,0,0);
	int rc;
	rc = sscanf_s(s,"%f%f%f",&t.x,&t.y,&t.z);
	if(rc<=0) return "usage:  brushtranslate  x y z";
	brush->backup.Add(BSPDup(brush->brush->bsp));
	BSPTranslate(brush->brush->bsp,t);
	if(qsnapenable)BSPQSnap(brush->brush->bsp);
	Retool(brush->brush);
	return "OK";
}
EXPORTFUNC(brushtranslate);


char *brushscale(String params)
{
	BrushManipulator *brush = CurrentBrush();
	if(!brush) return "no current brush";
	float s;
	s=0;
	int rc;
	rc = sscanf_s(params,"%f",&s);
	if(rc<=0 || s <=0.0f) return "usage:  brushscale positive_scale_factor";
	brush->backup.Add(BSPDup(brush->brush->bsp));
	BSPScale(brush->brush->bsp,s);
	//BSPQSnap(brush->brush->bsp);
	Retool(brush->brush);
	return "OK";;
}
EXPORTFUNC(brushscale);


float3 highlight(0.05f,0.05f,0.05f);
EXPORTVAR(highlight);

void BrushManipulator::Render()
{
	// highlight the entire brush, the current convex, the current face
	int i;
	if(this != currentmanipulator) return;
	if(active || brush->highlight) BSPRenderTest(brush->bsp,brush->position,Quaternion(),0,MaterialFind("highlight"));
	if(!active) return;
	BSPNode n;
	n.isleaf=UNDER;
	if(currentleaf)
	  for(i=0;i<currentleaf->brep.count;i++)
	{
		if(coplanar(*currentleaf->brep[i],*currentplane))
			n.brep.Add(currentleaf->brep[i]);
	}
	BSPRenderTest(currentleaf,brush->position,Quaternion(),0,MaterialFind("highlight"));
	BSPRenderTest(&n,brush->position,Quaternion(),0,MaterialFind("highlight"));
	n.brep.count=0;
}




char *physitcell(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	BSPNode *n =currentbrush->brush->bsp;
	while(!n->isleaf) {n=n->under;}
	Chuckable *c = new Chuckable(currentbrush->brush->id,n->convex,n->brep,currentbrush->brush->position);
	Brush *b=currentbrush->brush;
//	for(i=0;i<b->hash.slots_count;i++)if(b->hash.slots[i].used) if(b->hash.slots[i].value !="name") 
//	{
//		c->hash[b->hash.slots[i].key].Set(b->hash.slots[i].value.Get());
//	}
	delete currentbrush->brush;
	return "OK, its your brush to throw around";
}
EXPORTFUNC(physitcell);

Chuckable *brushtochuckable(Brush *brush)
{
	BSPNode *n =brush->bsp;
	Array<Face*> faces;
	BSPGetBrep(n,faces);
	Chuckable *c = new Chuckable(brush->id,n,faces,brush->position,brush->shadowcast);
	Brush *b=brush;
//	for(i=0;i<b->hash.slots_count;i++)if(b->hash.slots[i].used) if(b->hash.slots[i].value.Get() !="name") // should this be .key.Get() instead??
//	{
//		c->hash[b->hash.slots[i].key].Set(b->hash.slots[i].value.Get());
//	}
	delete brush;
	return c;
}

char *physit(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	brushtochuckable(currentbrush->brush);
/*
	BSPNode *n =currentbrush->brush->bsp;
	Array<Face*> faces;
	BSPGetBrep(n,faces);
	Chuckable *c = new Chuckable(n,faces,currentbrush->brush->position,currentbrush->brush->shadowcast);
	Brush *b=currentbrush->brush;
	for(i=0;i<b->hash.slots_count;i++)if(b->hash.slots[i].used) if(b->hash.slots[i].value.Get() !="name") // should this be .key.Get() instead??
	{
		c->hash[b->hash.slots[i].key].Set(b->hash.slots[i].value.Get());
	}
	delete currentbrush->brush;
*/
	return "OK, its your brush to throw around";
}
EXPORTFUNC(physit);


void BrushCreateManipulator(Brush *brush,int _active)
{
	BrushManipulator *m=new BrushManipulator(brush);
	m->active=_active;
}

char* spawn(String filename)
{
	Brush *brush = BrushLoad(filename);
	if(!brush) brush=BrushLoad(filename+".xml");
	if(!brush) return "file dont exist";
	brush->position = camera.position+camera.orientation.zdir() * -1.5f;
	brush->position = Round(brush->position,0.5f);
	extern int currentmaterial;
	new BrushManipulator(brush);
	return "OK";
}
EXPORTFUNC(spawn);


String brushsaveas(String filename)
{
	if(filename == "") {filename = "testbrush.xml";}
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	BrushSave(currentbrush->brush,filename);
	return String("Saved brush: ") << currentbrush->brush->id << " into: " << currentbrush->brush->filename;
}
EXPORTFUNC(brushsaveas);



String brushsaveit(String ignore)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	BrushSave(currentbrush->brush,"");
	return String() << "Saved brush " << currentbrush->brush->id <<  " into file: " << currentbrush->brush->filename;
}
EXPORTFUNC(brushsaveit);



char* snap(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	BSPNode *bsp = currentbrush->brush->bsp;
	BSPQSnap(bsp);
	Retool(currentbrush->brush);
	return "Snap successful.";
}
EXPORTFUNC(snap);


Brush *currentoperand=NULL;

String setoperand(String param)
{
	currentoperand = (param=="" || param==" () ")?CurrentBrush()->brush : BrushFind(param);
	return (currentoperand)? currentoperand->id : "NULL - No Current Operand";
}
EXPORTFUNC(setoperand);

Brush *GetOperand()
{
	if(currentoperand) return currentoperand;
	extern Brush *currentroom;
	if(currentroom) return currentroom;
	extern Brush *Area;
	return Area;
}

String weld(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	Brush *operand = GetOperand();
	if(!operand) return "No operand to weld to";
	if(operand==currentbrush->brush) return String("Cant weld Area '") + operand->id + "' to Itself";
	BSPNode *bsp = currentbrush->brush->bsp;
	currentbrush->brush->bsp = NULL;
	BSPTranslate(bsp,currentbrush->brush->position-operand->position);
	bsp=BSPUnion(bsp,operand->bsp);
	assert(bsp==operand->bsp);
	delete currentbrush->brush;
	currentbrush=NULL;
	operand->Modified(); // ModelDelete(operand->model); operand->model = NULL;
	return "Weld successful.";
}
EXPORTFUNC(weld);

String intersect(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	Brush *operand = GetOperand();
	if(!operand) return "No operand to weld to";
	if(operand==currentbrush->brush) return String("Cant intersect Area '") + operand->id + "' to Itself";
	BSPNode *bsp = currentbrush->brush->bsp;
	currentbrush->brush->bsp = NULL;
	BSPTranslate(bsp,currentbrush->brush->position-operand->position);
	if(BSPFinite(bsp))
	{
		NegateTree(bsp);
	}
	bsp=BSPIntersect(bsp,operand->bsp);
	assert(bsp==operand->bsp);
	delete currentbrush->brush;
	currentbrush=NULL;
	operand->Modified(); // ModelDelete(operand->model); operand->model = NULL;
	return "intersect successful.";
}
EXPORTFUNC(intersect);

char* negator(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	BSPNode *bsp = currentbrush->brush->bsp;
	NegateTree(bsp);
	currentbrush->brush->Modified();
	return "negate successful.";
}
EXPORTFUNC(negator);

float3 brushrange(20.0f,20.0f,10.0f);

void AreaMotionController()
{
	// invoke controllers to specify velocity and potentially new postion.
	for(int i=0;i<Brushes.count;i++)
	{
		Brushes[i]->positionold = Brushes[i]->position;
		for(int j=0;j<3;j++)  // bounce off of a reasonalbe limit
		{
			if(Brushes[i]->position[j] > brushrange[j]) Brushes[i]->velocity[j] = -fabsf(Brushes[i]->velocity[j]);
			if(Brushes[i]->position[j] <-brushrange[j]) Brushes[i]->velocity[j] =  fabsf(Brushes[i]->velocity[j]);
		}
		Brushes[i]->position = Brushes[i]->position + Brushes[i]->velocity * DeltaT; // next position
	}
}


int areaaggregation =0;
EXPORTVAR(areaaggregation);
void AreaAggregate()
{
	extern Brush *Area;
	if(!Area) return;
	extern int bspnodecount;
	extern char* areaback(const char*);
	extern char* arearestore(const char*);
	int i;
	if(!areaaggregation) return;
	arearestore("");
	areaback("");
	for(i=0;i<Brushes.count;i++)
	{
		if(Brushes[i]==Area) continue;
		if(BSPFinite(Brushes[i]->bsp)) continue;
		BSPNode *bsp = BSPDup(Brushes[i]->bsp);
		BSPTranslate(bsp,Brushes[i]->position);
		int areanodesbefore = BSPCount(bsp) + BSPCount(Area->bsp);
		int bspnodecountbefore = bspnodecount;
		bsp=BSPIntersect(bsp,Area->bsp);
		int areanodesafter = BSPCount(Area->bsp);
		int unaccounted = (bspnodecount-bspnodecountbefore)-(areanodesafter-areanodesbefore);
		assert(!unaccounted);
	}
	Area->Modified(); // ModelDelete(area->model);	area->model = NULL;
}




char* slicebrush(const char*s)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	currentbrush->backup.Add(BSPDup(currentbrush->brush->bsp));
	float3 normal(0,0,1); // default to z
	if(*s=='y' || *s=='Y') normal=float3(0,1,0);
	if(*s=='x' || *s=='X') normal=float3(1,0,0);
	Plane p(normal,Round(-dot(normal,manipv1-currentbrush->brush->position),0.25f));
	BSPNode *n = new BSPNode(p.normal(),p.dist());
	BSPPartition(currentbrush->brush->bsp,p,n->under,n->over);
	currentbrush->brush->bsp = n;
	Retool(currentbrush->brush);
	return "OK";
}
EXPORTFUNC(slicebrush);

char* brushmakemeshes(String s)
{
	Brush *brush = (s!="" && s!=" () ")?BrushFind(s) : (CurrentBrush())? CurrentBrush()->brush : NULL;  // Find brush by name if given or try by mouse position
	if(!brush) return "No Current Brush";
	brush->Modified();
	brush->model = CreateModel(brush->bsp,brush->shadowcast);
	return "Ok";
}
EXPORTFUNC(brushmakemeshes);


char* sliceleaf(const char*s)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	currentbrush->backup.Add(BSPDup(currentbrush->brush->bsp));
	float3 normal(0,0,1); // default to z
	if(*s=='y' || *s=='Y') normal=float3(0,1,0);
	if(*s=='x' || *s=='X') normal=float3(1,0,0);
	Plane p(normal,Round(-dot(normal,manipv1-currentbrush->brush->position),0.25f));
	BSPNode n(p);
	BSPNode *tmp = new BSPNode();
	*tmp = *currentbrush->currentleaf;
	*currentbrush->currentleaf = n;
	BSPPartition(tmp,p,currentbrush->currentleaf->under,currentbrush->currentleaf->over);
	Retool(currentbrush->brush);
	return "OK";
}
EXPORTFUNC(sliceleaf);

String brushbspclean(String s)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	currentbrush->brush->bsp = BSPClean(currentbrush->brush->bsp);
	return "ok";
}
EXPORTFUNC(brushbspclean);

char* promote(const char*s)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	currentbrush->backup.Add(BSPDup(currentbrush->brush->bsp));
	if(currentbrush->currentplane == currentbrush->brush->bsp)
	{
		return "plane already at top level";
	}
	currentbrush->brush->bsp = BSPClean(currentbrush->brush->bsp);
	Plane p = *currentbrush->currentplane;
	BSPNode *n = new BSPNode(p);
	n->convex = WingMeshDup(currentbrush->brush->bsp->convex);
	BSPPartition(currentbrush->brush->bsp,p,n->under,n->over);
	n=BSPClean(n);
	currentbrush->brush->bsp = n;
	Retool(currentbrush->brush);
	return "OK";
}
EXPORTFUNC(promote);

static void bevelbrushleaf(BrushManipulator *bm,const Plane &p)
{
	bm->backup.Add(BSPDup(bm->brush->bsp));
	bm->currentleaf->normal() = p.normal();
	bm->currentleaf->dist()   = p.dist();
	bm->currentleaf->isleaf = 0;
	bm->currentleaf->over   = new BSPNode();
	bm->currentleaf->over->isleaf = OVER;
	bm->currentleaf->under  = new BSPNode();
	bm->currentleaf->under->isleaf = UNDER;
	bm->currentplane = bm->currentleaf;
	bm->currentleaf = bm->currentleaf->under;
	bm->lastplane = p;
	Retool(bm->brush);

}

static int ClosestVertexIndex(Array<float3> &points,const float3 &v)
{
	int i;
	if(!points.count) return -1; 
	int closest=0;
	float min_dist=magnitude(v-points[0]);
	for(i=1;i<points.count;i++)
	{
		float dist = magnitude(v-points[i]);
		if(dist<min_dist)
		{
			min_dist=dist;
			closest=i;
		}
	}
	return closest;
}

/*
static int ClosestVertexOnPlaneIndex(Convex *convex,int pid,const float3 &sample_point)
{
	int i;
	if(!convex->edges.count) return -1; 
	int closest_vid=-1;
	float min_dist;
	for(i=0;i<convex->edges.count;i++)
	{
		if(convex->edges[i].p!=pid) continue;
		int vid = convex->edges[i].v;
		float dist = magnitude(sample_point-convex->vertices[vid]);
		if(closest_vid==-1 || dist<min_dist)
		{
			min_dist=dist;
			closest_vid=vid;
		}
	}
	return closest_vid;
}

static void EdgeRingForFacet(Convex *convex, int fid, Array<Convex::HalfEdge> &ring)
{
	int i;
	ring.count=0;
	for(i=0;i<convex->edges.count;i++)
	{
		Convex::HalfEdge &edge = convex->edges[i];
		if(edge.p == fid)
		{
			ring.Add(edge);
		}
	}
	assert(ring.count>=3);
}

float3 ConvexEdgeVertex0(Convex *convex,int edge_id)
{
	return convex->vertices[convex->edges[edge_id].v];
}
float3 ConvexEdgeVertex1(Convex *convex,int edge_id)
{
	return convex->vertices[convex->edges[convex->edges[edge_id].ea].v];
}
static int ClosestEdgeOnPlaneIndex(Convex *convex,int fid,const float3 &sample_point)
{
	int i;
	if(!convex->edges.count) return -1; 
	int closest=-1;
	float mindr;
	Plane p1a;
	for(i=0;i<convex->edges.count;i++)
	{
		if(convex->edges[i].p!=fid) continue;
		float3 v0 = ConvexEdgeVertex0(convex,i); 
		float3 v1 = ConvexEdgeVertex1(convex,i); 
		float d = magnitude(LineProject(v0,v1,sample_point)-sample_point);
		if(closest==-1 || d<mindr)
		{
			mindr=d;
			closest = i;
		}
	}
	assert(closest>=0);
	assert(convex->edges[closest].p==fid);
	return closest;
}
static int ConvexFindPlaneIndex(Convex *convex,const Plane &plane)
{
	int i;
	for(i=0;i<convex->facets.count;i++)
	{
		if(convex->facets[i].normal==plane.normal) 
		{
			return i;
		}
	}
	return -1;
}

float DistToClosestEdgeOnPlane(Convex *convex,const Plane &plane,const float3 &sample_point)
{
	int pid   = ConvexFindPlaneIndex(convex,plane); assert(pid>=0);
	int eid   = ClosestEdgeOnPlaneIndex(convex,pid,sample_point); assert(eid>=0);
	float3 v0 = ConvexEdgeVertex0(convex,eid); 
	float3 v1 = ConvexEdgeVertex1(convex,eid); 
	return magnitude(LineProject(v0,v1,sample_point)-sample_point);
}
*/


/*
classifyspot(BSPNode *root,BSPNode *leaf,const Plane &p0, const Plane &p1 ,const float3 &s)
{
	BSPNode *splitter=NULL;  // if we find a plane that hits the region before we hit the plane of the surface
	BSPNode *surface=NULL;
	BSPNode *surface_adj=NULL;
	BSPNode *overleaf=NULL;
	BSPNode *besideleaf=NULL;
	BSPNode *overbesideleaf=NULL;
	overbesideleaf=root;
	assert(!PlaneTest(p0.normal,p0.dist,s));
	assert(!PlaneTest(p1.normal,p1.dist,s));
	while(!overbesideleaf->isleaf)
	{
		int f = PlaneTest(overbesideleaf->normal,overbesideleaf->dist,s);
		if(!f && coplanar(p0,*overbesideleaf)) f=(dot(p0.normal,overbesideleaf->normal)>0)? OVER  : UNDER;
		if(!f && coplanar(p1,*overbesideleaf)) f=(dot(p1.normal,overbesideleaf->normal)>0)? OVER  : UNDER;
		if(!f) f= PlaneTest(overbesideleaf->normal,overbesideleaf->dist,PlaneProject(p0,s+ p1.normal));
		assert(f);
		overbesideleaf = (f==OVER) overbesideleaf->over: overbesideleaf->under;
	}
	Array<BSPNode*> stack;
	stack.Add(root);
	while(stack.count)
	{
		n=stack.Pop();
		if(!n) continue;
		if(n->isleaf)
		{
			
		}
		else if(coplanar(p0,*n)
		{
			surface = n;
			break;
		}
		else 
		{
		}
		stack->Add(
	}
	if(surface)
	{

	}
	return 0;
}
*/

/*
char* bevelbrushconvex(char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	//Plane p(Quantized(normalize(manipv0-manipv1)),0);
	//p.dist = -dot(p.normal,manipv1-currentbrush->brush->position);
	float3 s = manipv1-currentbrush->brush->position;
	Plane p0 = (Plane)*currentbrush->currentplane;
	Convex *convex = currentbrush->currentleaf->convex;
	int pid0 = ConvexFindPlaneIndex(convex,p0); assert(pid0>=0);
	int eid = ClosestEdgeOnPlaneIndex(convex,pid0,s); assert(eid>=0);
	Plane p1 = convex->facets[convex->edges[convex->edges[eid].ea].p];

	float3 cp = normalize(cross(p0.normal,p1.normal));
	float3 se = ThreePlaneIntersection(p0,p1,Plane(cp,-dot(cp,s)));

	Plane p(Quantized(p0.normal+p1.normal),0);
	p.dist = -dot(se,p.normal);
	p = Quantized(p);
	while(dot(p.normal,se)+p.dist < QuantumDist(p.normal)/2.0f) {p.dist += QuantumDist(p.normal);}
	p=Quantized(p); // should not do anything

	bevelbrushleaf(currentbrush,p);
	return "OK";
}
EXPORTFUNC(bevelbrushconvex);


char* bevelbrushconcave(char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	//Plane p(Quantized(normalize(manipv0-manipv1)),0);
	//p.dist = -dot(p.normal,manipv1-currentbrush->brush->position);
	float3 s = manipv1-currentbrush->brush->position;
	Plane p0 = (Plane)*currentbrush->currentplane;
	Convex *convex = currentbrush->currentoverleaf->convex;
	int pid0 = ConvexFindPlaneIndex(convex,Plane(-p0.normal,-p0.dist)); assert(pid0>=0);
	int eid = ClosestEdgeOnPlaneIndex(convex,pid0,s); assert(eid>=0);
	Plane p1 = convex->facets[convex->edges[convex->edges[eid].ea].p];
	p1.normal*=-1; p1.dist*=-1;
	float3 cp = normalize(cross(p0.normal,p1.normal));
	float3 se = ThreePlaneIntersection(p0,p1,Plane(cp,-dot(cp,s)));

	Plane p(Quantized(p0.normal+p1.normal),0);
	p.dist = -dot(se,p.normal);
	p = Quantized(p);
	while(dot(p.normal,se)+p.dist > -QuantumDist(p.normal)/2.0f) {p.dist -= QuantumDist(p.normal);}
	p=Quantized(p); // should not do anything

	currentbrush->currentleaf = currentbrush->currentoverleaf;
	bevelbrushleaf(currentbrush,p);
	return "OK";
}
EXPORTFUNC(bevelbrushconcave);

char *bevelbrush(char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	float3 s = manipv1-currentbrush->brush->position;
	Plane p = (Plane)*currentbrush->currentplane;
	if(DistToClosestEdgeOnPlane(currentbrush->currentleaf->convex,p,s)<=DistToClosestEdgeOnPlane(currentbrush->currentoverleaf->convex,Plane(-p.normal,-p.dist),s))
	{
		bevelbrushconvex(NULL);
	}
	else
	{
		bevelbrushconcave(NULL);
	}
	return "OK";
}
EXPORTFUNC(bevelbrush);

char* bevelbrushvert(char*)
{
	int i;
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";

	float3 s = manipv1-currentbrush->brush->position;
	Plane p0 = (Plane)*currentbrush->currentplane;
	Convex *convex = currentbrush->currentleaf->convex;
	int pid0 = ConvexFindPlaneIndex(convex,p0); assert(pid0>=0);

	int svid = ClosestVertexOnPlaneIndex(convex,pid0,s);
	float3 sv = convex->vertices[svid];
	float3 cn(0,0,0);
	for(i=0;i<convex->edges.count;i++)
	{
		if(convex->edges[i].v == svid)
		{
			cn += convex->facets[convex->edges[i].p].normal;
		}
	}
	Plane p(Quantized(cn),0);
	p.dist = -dot(sv,p.normal);
	p = Quantized(p);
	while(dot(p.normal,sv)+p.dist<0) {p.dist += QuantumDist(p.normal);}
	p=Quantized(p); // should not do anything

	bevelbrushleaf(currentbrush,p);
	return "OK";
}
EXPORTFUNC(bevelbrushvert);
*/

char* brushbev(const char*)
{
    BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	float3 s = manipv1-currentbrush->brush->position;
	Plane p(Quantized(camera.orientation.zdir()),0);
	p.dist() = -dot(s,p.normal());
	p = Quantized(p);
	while(dot(p.normal(),s)+p.dist()<0) {p.dist() += QuantumDist(p.normal());}
	p=Quantized(p); // should not do anything

	bevelbrushleaf(currentbrush,p);
	return "OK";
}
EXPORTFUNC(brushbev);

char *brushrecompile(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	ReCompile(currentbrush->brush);	
	return "brush recompiled";
}
EXPORTFUNC(brushrecompile);

char *brushrevert(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return "No Current Brush";
	if(!currentbrush->backup.count) return "Nothing to UNDO";
	delete currentbrush->brush->bsp;
	currentbrush->brush->bsp = currentbrush->backup.Pop();
	return "UNDO brush edit";
}
EXPORTFUNC(brushrevert);


int pickedmaterial=0;


int paintmaterial(String pick)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush || !currentbrush->facehit) return -1;
	if(pick.Asint()) pickedmaterial=currentbrush->facehit->matid;
	currentbrush->facehit->matid=pickedmaterial;
	return currentbrush->facehit->matid;
}
EXPORTFUNC(paintmaterial);



int cyclefacemat(const char*)
{
	BrushManipulator *currentbrush=CurrentBrush();
	if(!currentbrush) return -1;
	if(!currentbrush->facehit) return -1;
	currentbrush->facehit->matid ++;
	currentbrush->facehit->matid%= Materials.count;
	pickedmaterial = currentbrush->facehit->matid;
	return currentbrush->facehit->matid;
}
EXPORTFUNC(cyclefacemat);



String brush_edge_splitify(String)
{
	if(!CurrentBrush()) return "no current brush";
	int c=FaceSplitifyEdges(CurrentBrush()->brush->bsp);
	return String("OK, count edges split: ") + String(c)+" on brush:" + CurrentBrush()->brush->id;
}
EXPORTFUNC(brush_edge_splitify);

String brushdelmodel(String)
{
	if(!CurrentBrush()) return "no current brush";
	Brush *brush = CurrentBrush()->brush;
	if(!brush->model) return "no currently cached model";
	ModelDelete(brush->model);
	brush->model=NULL;
	return String("OK, deleted model on: ") + brush->id;
}
EXPORTFUNC(brushdelmodel);



void brushfill(Brush *brush,int matid)
{
	Array<Face*> faces;
	BSPGetBrep(brush->bsp,faces);
	for(int i=0;i<faces.count;i++)
		faces[i]->matid = matid;

	brush->Modified();
}

int developermode=1;
EXPORTVAR(developermode);
int BrushManipulator::KeyPress(int k)
{
	if(!developermode) return 0;
	if(k=='\\')
	{
		active = !active;
		return 1;
	}
	if(k=='\b')
	{
		delete brush;
		// delete this;  // should happen automatically
		assert(currentmanipulator==NULL);
		return 1;
	}
	if(!active)
	{
		return 0;
	}
	void texcylinder(Face *face);
	void texplanar(Face *face);
	switch(k)
	{
		case '[':
		case '{':
			if(facehit) pickedmaterial = facehit->matid = (facehit->matid+1) % Materials.count;
			break;
		case ']':
		case '}':
			if(facehit) pickedmaterial = facehit->matid = (facehit->matid-1+Materials.count) % Materials.count;
			break;
		case 'p':
			if(facehit) facehit->matid = pickedmaterial;
			break;
		case 'P':
			brushfill(brush,pickedmaterial);
			break;
		case 'u':
			if(facehit) texcylinder(facehit);
			break;
		case 'y':
			if(facehit) texplanar(facehit);
			break;
		case 'i':
			if(facehit) facehit->gu *= -1.0f;
			break;
		case 'I':
			if(facehit) facehit->gv *= -1.0f;
			break;
		default:
			return 0;  // no keys used
	}
	brush->Modified(); // since options that reach this statement have changed the model somehow
	return 1;
}

String beditrefresh(String)
{
	while(BrushManipulators.count)
		delete BrushManipulators[0];
	for(int i=0;i<Brushes.count;i++)
		new BrushManipulator(Brushes[i]);
	return "ok";
}
EXPORTFUNC(beditrefresh);



static Brush *brushgetbiggest(Brush *avoid,int dothefiniteguys)
{
	// gets the largest "room" according to diagonal of the brush's extents
	// routine ignores the passed in brush.
	// only choose amongst the non-Finite brushes.
	Brush *biggest=NULL;
	float diag=0.0f;  // largest diagonal found so far.
	for(int i=0;i<Brushes.count;i++)
	{
		Brush *b=Brushes[i];
		if(b==avoid) continue;
		if(!b->collide) continue;
		if(!b->mergeable) continue;
		if(BSPFinite(b->bsp) != dothefiniteguys) continue;
		float d = magnitude(b->bmax-b->bmin);
		if(d>=diag)
		{
			biggest=b;
			diag=d;
		}
	}
	return biggest;
}


String intersectall(String param)
{
	Brush *area=GetOperand();
	int k=0;
	if(!area) 
	{
		area=brushgetbiggest(NULL,0);
	}
	if(!area) return "no current operand";
	BSPTranslate(area->bsp,area->position);
	area->position = float3(0,0,0);
	float3 bmin,bmax;
	for(int i=0;i<Brushes.count;i++)
	{
		Brush *b=Brushes[i];
		bmin=VectorMin(bmin,b->position-b->bmin);
		bmax=VectorMax(bmax,b->position+b->bmax);
	}
	BSPDeriveConvex(area->bsp,WingMeshCube(bmin-float3(10,10,10),bmax+float3(10,10,10)));
	Brush *b;
	while((b=brushgetbiggest(area,0)))
	{
		BSPTranslate(b->bsp,b->position - area->position);
		BSPDeriveConvex(b->bsp,WingMeshCube(bmin-float3(10,10,10),bmax+float3(10,10,10)));
		BSPIntersect(b->bsp,area->bsp);
		delete b;
		k++;
	}
	if(param.Asint()==1)
	 while((b=brushgetbiggest(area,1)))
	{
		BSPTranslate(b->bsp,b->position - area->position);
		//BSPDeriveConvex(b->bsp,WingMeshCube(bmin-float3(10,10,10),bmax+float3(10,10,10)));
		BSPUnion(b->bsp,area->bsp);
		if(b->model && b->shadowcast)
		{
			ModelSetMatrix(b->model,MatrixFromQuatVec(Quaternion(),b->position));  // make sure its in the right spot.
			extern void ModelDeleteNonShadows(Model *model);
			ModelDeleteNonShadows(b->model);
			extern Array<Model*> ShadowOnlyModels;
			ShadowOnlyModels.Add(b->model);
			b->model=NULL;
		}
		delete b;
		k++;
	}
	FaceSplitifyEdges(area->bsp);
	beditrefresh("");
	area->Modified(); // ModelDelete(area->model);	area->model = NULL;
	area->shadowcast = 0;
	area->model = CreateModel(area->bsp,area->shadowcast);
	extern BSPNode* area_bsp;
	area_bsp= area->bsp;   // we do this so that the physics code knows how to collide with the world.
	extern void PhysicsPurgeContacts();
	PhysicsPurgeContacts();
	return String("intersected ") + String(k) + " brushes into: " + area->id ;
}
EXPORTFUNC(intersectall);


char *xml(String filename)
{
	return spawn(filename);
}
EXPORTFUNC(xml);

static String bsh(String filename)
{
	return spawn(filename);
}
EXPORTFUNC(bsh);

static String jpg(String filename)
{
	int m = MaterialFind(splitpathfname(filename));
	if(!CurrentBrush()) return "no current brush";
	Brush *brush = CurrentBrush()->brush;
	if(!CurrentBrush()->facehit) return "no current face";
	CurrentBrush()->facehit->matid = m;
	brush->Modified();
	return brush->id + " now using texture " + String(m);
}
EXPORTFUNC(jpg);
static String png(String filename)
{
	return jpg(filename);
}
EXPORTFUNC(png);

String message(String)
{
	return (CurrentBrush()&&!CurrentBrush()->InDrag())?CurrentBrush()->brush->message:"";
}
EXPORTFUNC(message);


