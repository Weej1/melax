//
// the module that deals with the static world/area mesh and bsp
//

#include <assert.h>
#include <stdio.h>
#include "winsetup.h"
#include "array.h"
#include "vecmath.h"
#include "bsp.h"
#include "vertex.h"
#include "console.h"
#include "material.h"
#include "manipulator.h"
#include "manipulatori.h"
#include "brush.h"
#include "mesh.h"
#include "light.h"
#include "chuckable.h"
#include "qplane.h"

int SplinesLoad(xmlNode *scene);
int SplineExportAll(xmlNode *scene);


BSPNode* area_bsp;
Brush *Area;

Array<Model*> ShadowOnlyModels;

Array<BSPNode *> area_bak;

char* areaback(const char*)
{
	area_bak.Add(BSPDup(area_bsp));
	return "OK";
}
EXPORTFUNC(areaback);

char* arearestore(const char*)
{
	if(!area_bak.count) return "nothing to undo";
	delete area_bsp;
	Area->bsp=area_bsp = area_bak.Pop();
	return "undo area modification";
}
EXPORTFUNC(arearestore);


String startarea("boxroom");
EXPORTVAR(startarea);
void AreaLoadDefault()
{
	extern Array<Brush*> Brushes;
	if(Brushes.count) return;  // we've already got some content going, so dont load anything.
	(Area = BrushLoad(startarea)) || (Area=BrushLoad("boxroom")) ;
	assert(Area);
	extern void BrushCreateManipulator(Brush *brush,int _active);
	BrushCreateManipulator(Area,0);
	area_bsp = Area->bsp;
	Area->model = CreateModel(Area->bsp,0);
}

String loadarea(const char *filename)
{
	if(!filename || !*filename) return "Yo, you must supply a filename"; 
	Brush *tmp = BrushLoad(filename);
	if(!tmp) return "ERROR: cant load area, does file even exist";
	delete Area;
	Area=tmp;
	area_bsp = Area->bsp;
	assert(Area);
	Area->model = CreateModel(Area->bsp,0);
	return String("loaded area: ")+filename;
}
EXPORTFUNC(loadarea);




String scenesave(String filename)
{
	int i;
	int unnamed=0;
	if(filename=="") return "usage:  scenesave <filename>";
	xmlNode *s = new xmlNode("scene");
	for(i=0;i<Brushes.count;i++)
	{
		Brush *brush = Brushes[i];
		if(brush->id =="") 
		{
			unnamed++;
			continue;
		}
		xmlNode *b = xmlexport(brush,GetClass("Brush"),"brush" ); //new xmlNode("brush",s);
		s->children.Add(b);
		xmlNode *n = new xmlNode("file",b);
		//xmlNode *p = new xmlNode("position",b);
		
		n->body = (brush->filename!="") ? splitpathfname(brush->filename) : brush->id;
		//p->body = AsString(brush->position);
	}
	LightExportAll(s);
	SplineExportAll(s);
	XMLSaveFile(s,filename);
	delete s;
	return String("Saved ") + filename + String(" brushcount: ") + String(Brushes.count-unnamed) +" unsaved:"+ String(unnamed) ;
}
EXPORTFUNC(scenesave);

String scenepath("./ brushes/");
//EXPORTVAR(scenepath);

String sceneload(String scenename)
{
	int i;
	int count=0;
	if(scenename=="") return "usage:  sceneload <scenename>";
	String scenefilename = filefind(scenename,scenename + " " + scenepath,".scene .xml");
	xmlNode *layout = XMLParseFile(scenefilename);
	if(!layout) return "no such file";
	for(i=0;i<layout->children.count;i++)
	{
		xmlNode *b = layout->children[i];
		if(b->tag!="brush") continue;
		String file = b->Child("file")->body;
		//float3 p = AsFloat3(b->Child("position")->body);
		Brush *brush =  BrushLoad(file,splitpathdir(scenefilename));
		if(!brush) continue;
		
		xmlimport(brush,GetClass("brush"),b);
		extern void BrushCreateManipulator(Brush *brush,int _active);
		BrushCreateManipulator(brush,0);
		assert(!brush->model);
		if(!brush->bspclip)
		{
			brush->model = CreateModel(brush->bsp,brush->shadowcast);
		}
		count ++;
		brush->positionold = brush->position_start =brush->position ;
	}
	LightsLoad(layout);
	SplinesLoad(layout);
	delete layout;
	return String("Loaded ") + scenefilename + String(" brushcount: ") + String(count);
}
EXPORTFUNC(sceneload);

static String scene(String filename)
{
	return sceneload(filename);
}
EXPORTFUNC(scene);


//----- 
// layer around the rendering engine that gets game assets into renderable things 
// this next bit has knowledge of brushes and bsps but doesn't need to know about d3d specifics 

Array<Model*> tempmodels;

Model *MakeBrepBuffersMat(BSPNode *bsp,int matid_override=-1);
Model *MakeBrepBuffersMat(const Array<Face*> &faces,int matid_override=-1);


Model *CreateModel(BSPNode *bsp,int shadow)
{
	Array<Face*> faces;
	BSPGetBrep(bsp,faces);
	Model *model = MakeBrepBuffersMat(faces);
	extern int rendershadows;
	if(shadow && rendershadows )  // make a shadow mesh
	{
		//extern int FaceSplitifyEdges(BSPNode *root);
		FaceSplitifyEdges(bsp);
		extern DataMesh *MakeRigedMesh(Array<Face*> &faces,float crease_angle_degrees);
		DataMesh *m= MakeRigedMesh(faces,10.0f);
		m->matid = MaterialFind("extendvolume");
		m->model = model;
		model->datameshes.Add(m);
	}
	extern int ModelCompileMeshes(Model *model);
	//ModelCompileMeshes(model); 
	return model;
}


void BSPRenderTest(BSPNode *bsp,const float3 &p,const Quaternion &q,int shadow=1,int matid_override=-1);

BSPNode *hackbsp=NULL;
float3   hackpos;
int bspclip=0;
EXPORTVAR(bspclip);
void BSPRenderTest(BSPNode *bsp,const float3 &p,const Quaternion &q,int shadow,int matid_override)
{
	//hak
	Model *model = NULL;
	if(shadow && matid_override==-1)
	{
		Array<Face *> faces;
		BSPGetBrep(bsp,faces);
		model = MakeBrepBuffersMat(bsp,matid_override); 
		extern int rendershadows;
		if(rendershadows)
		{
			extern DataMesh *MakeRigedMesh(Array<Face*> &faces,float crease_angle_degrees);
			DataMesh *m = MakeRigedMesh(faces,10.0f); 
			m->matid = MaterialFind("extendvolume");
			model->datameshes.Add(m);
			m->model = model;
		}
	}
	else if(bspclip&& matid_override==-1)
	{
		Array<Face *> under[2],over,created;
		BSPGetBrep(bsp,under[0]);
		extern void BSPClipFaces(BSPNode *bsp,Array<Face*> &faces,const float3 &position,Array<Face*> &under,Array<Face*> &over,Array<Face*> &created);
		int clipcount=0;
		// hack to find this brush:
		int k;
		for(k=0;k<Brushes.count;k++)
		{
			if(Brushes[k]->bsp==bsp) break;
		}
		assert(k<Brushes.count);
		for(int i=0;i<Brushes.count;i++) 
		{
			if(Brushes[i]->bsp==bsp) continue;
			if(!Brushes[i]->bspclip) continue;
			//if(BSPFinite(Brushes[i]->bsp)) continue;
			if(!overlap(Brushes[i]->bmin+Brushes[i]->position,Brushes[i]->bmax+Brushes[i]->position,Brushes[k]->bmin+Brushes[k]->position,Brushes[k]->bmax+Brushes[k]->position) ) continue;
			//else 
			BSPClipFaces(Brushes[i]->bsp,under[clipcount],p-Brushes[i]->position,under[clipcount^1],over,created);
			under[clipcount].count=0;
			clipcount^=1;
		}
		extern int hack_usealpha;
		model = MakeBrepBuffersMat(hack_usealpha?over:under[clipcount],matid_override);
		if(bspclip==3 && !hack_usealpha)
		{
			hack_usealpha++;
			Model* modela = MakeBrepBuffersMat(over,matid_override);
			modela->position =p;
			modela->orientation = q; 
			ModelRender(modela);
			tempmodels.Add(modela);
			hack_usealpha--;
		}
		while(created.count) delete created.Pop();
	}
	else
	{
		model = MakeBrepBuffersMat(bsp,matid_override);
	}
	model->position =p;
	model->orientation = q; 
	ModelRender(model);
	tempmodels.Add(model);
}



int brush_rendermodel=1;
EXPORTVAR(brush_rendermodel);


void BrushRenderTest()
{
	int i;

	for(i=0;i<Brushes.count;i++)
	{
		Brush *brush = Brushes[i];
		if(!brush->draw) continue;
		//if(brush->bsp == area_bsp) continue;// HACK
		//if(!bspclip && BSPInSolid(brush->bsp)) continue; 

		if(brush_rendermodel && brush->model)
		{
			brush->model->position = brush->position ;  // fixme data duplication ,MatrixFromQuatVec(Quaternion(),brush->position));
			ModelRender(brush->model);
		}
		else
		{
			bspclip+=brush->bspclip;  // FIXME  YUCK
			BSPRenderTest(brush->bsp,brush->position,Quaternion(),BSPFinite(brush->bsp)&&brush->shadowcast);
			bspclip-=brush->bspclip;
		}
	}
}

void BrushRenderTestAlpha()
{
	int i;
	extern float alpha;
	if(!bspclip || alpha==0.0f) return;
	extern int hack_usealpha;
	hack_usealpha++;
	for(i=0;i<Brushes.count;i++)
	{
		Brush *brush = Brushes[i];
		if(!brush->draw) continue;
		//if(brush->bsp == area_bsp) continue;// HACK
		if(BSPFinite(brush->bsp)) continue;

		if(brush_rendermodel && brush->model)
		{
			brush->model->position = brush->position ; // fixme
			ModelRender(brush->model);
		}
		else
		{
			BSPRenderTest(brush->bsp,brush->position,Quaternion(),0);
		}
	}
	hack_usealpha--;
}


void OrphanShadowRender()
{
	for(int i=0;i<ShadowOnlyModels.count;i++)
		ModelRender(ShadowOnlyModels[i]);
}

class Drawable : public Entity , public Pose
{
 public:
	//float3 position;
	//Quaternion orientation;
	Model *model;
	float animspeed;
	float time;
	Drawable(String _name,Model *_model);
	~Drawable();
};
LDECLARE(Drawable,position);
LDECLARE(Drawable,orientation);
LDECLARE(Drawable,animspeed);
LDECLARE(Drawable,time);
Array<Drawable *> drawables;
Drawable::Drawable(String _name,Model *_model):Entity(_name),model(_model),animspeed(0.0f),time(0.0f)
{
	LEXPOSEOBJECT(Drawable,id);
	drawables.Add(this);
}
Drawable::~Drawable()
{
	drawables.Remove(this);
}
Drawable *spawndrawable(String filename,const float3 &position=float3(0,0,0),const Quaternion &orientation=Quaternion(0,0,0,1.0f))
{
	Model *model=ModelLoad(filename);
	if(!model) return NULL;
	model->position    = position;
	model->orientation = orientation;
	String name = splitpathfname(filename);
	Drawable *drawable = new Drawable(name,model);
	drawable->position = position;
	drawable->orientation = orientation;
	return drawable;
}

extern Model *ModelText(String s,Model *model,float2 size=float2(1.0f,1.0f));

class ConsoleManipulator : public Manipulator , public Tracker
{
public:
	float3 size;
	Drawable *dr;
	String command;
	String result;
	int kgrab;
	int repeat;
	ConsoleManipulator(Drawable *_dr):repeat(0){dr=_dr;kgrab=0;update();size=float3(1.0f,1.0f,0);};
	~ConsoleManipulator(){}
	virtual int  HitCheck(const float3 &_v0, float3 v1,float3 *impact)
	{
		float3 v0= rotate(Inverse(dr->orientation),(_v0-dr->position)); 
		v1= rotate(Inverse(dr->orientation),(v1-dr->position)); 
		float3 h= (v0.z*v1.z>0.0f) ? float3(FLT_MAX,0,0) : PlaneLineIntersection(Plane(float3(0,0,1),0.0f),v0,v1);
		float3 s=size*0.5f;
		if(h.x<-s.x || h.y<-s.y || h.x>s.x || h.y>s.y){if(kgrab){kgrab=0;update();}return 0;}
		if(impact)*impact=rotate(dr->orientation,h)+dr->position;
		return 1;
	}
	virtual float3 ScaleIt(float3 &s) { size += s*2.0f; size = vabs(size); size.x = Max(size.x,0.01f);size.y = Max(size.y,0.01f);return size;}
	virtual float3& Position(){return dr->position;}
	virtual Quaternion& Orientation(){return dr->orientation;}
	//virtual int  Drag(const float3 &v0, float3 v1,float3 *impact);
	virtual int  DragStart(const float3 &v0, const float3 &v1){if(!kgrab){kgrab=2;repeat=0;update();}return 1;}
	virtual int  DragEnd(const float3 &v0, const float3 &v1){kgrab--;Orientation()=Quantized(Orientation()) ; update();return 1;}
	virtual void Render()
	{
		float3 c = ((kgrab)?float3(0,1,0):float3(1,0,1)) * ((currentmanipulator==this)?1.0f:0.25f);
		float3 s=size*0.5f;
		Line(dr->pose()*float3(-s.x,-s.y,0),dr->pose()*float3( s.x,-s.y,0),c);
		Line(dr->pose()*float3(-s.x,-s.y,0),dr->pose()*float3(-s.x, s.y,0),c);
		Line(dr->pose()*float3( s.x, s.y,0),dr->pose()*float3( s.x,-s.y,0),c);
		Line(dr->pose()*float3( s.x, s.y,0),dr->pose()*float3(-s.x, s.y,0),c);
	}
	virtual float3 Bmin(){return Position()-size*0.5f-float3(0.01f,0.01f,0.01f);}
	virtual float3 Bmax(){return Position()+size*0.5f+float3(0.01f,0.01f,0.01f);}
	//int Wheel(int w);
	int KeysGrab(){return kgrab;}
	void update()
	{
		dr->model = ModelText(((kgrab)?String(".  LispTerm   .\n] "):String((repeat>1)?".   LispTrace   .\n":"L-Click to Type\n")) + command + (result.Length()?("\n\n" + result):"_") ,dr->model ,float2(size.x,size.y));
	}
	int KeyPress(int k)
	{
		if(!kgrab) return 0;
		if(k!='\r')
			repeat=0;
		if(k=='\b')
		{
			if(result.Length())
				result="";
			else if(command.Length()) 
				command[command.Length()-1]='\0';
			command.correctlength();
		}
		else if(  k=='\r')
		{
			if(repeat++)
				kgrab=0;  // go into trace mode
			result = FuncInterp(command); 
		}
		else if(k==27) //ESC
			kgrab=0;
		else if(k)
		{
			command << (char) k;
		}
		update();
		return 1;
	}
	//float3 impact_local; // last impact on this manipulator in brush local space
	void notify(Entity*){dr=NULL;delete this;}
};

String computer(String s)
{
	ConsoleManipulator *cm;
	static int termnum=0;
	cm = new ConsoleManipulator(new Drawable(String("lterm") << termnum++ ,ModelText(s,NULL)));
	// not the best starting position, but whatever...
	cm->dr->orientation = YawPitchRoll(0,90.0f,0);
	cm->dr->position.z=2.0f;
	return "ok";
}
EXPORTFUNC(computer);

void UpdateTerms()
{
	ConsoleManipulator *cm;
	for(int i=0;i<Manipulators.count;i++)
	 if((cm = dynamic_cast<ConsoleManipulator*>(Manipulators[i])) && cm->repeat>1)
	{
		cm->result = FuncInterp(cm->command); 
		cm->update();
	}
}

static Drawable *hackmodeltest=NULL;
String ezm(String s)
{
	if(s=="") return "provide a model filename";
	String filename = PopFirstWord(s," ,\t\n");
	float3 position;
	Quaternion orientation;
	extern float3 focuspoint;
	if(s=="") position = focuspoint;
	else StringIter(s) >> position >> orientation;
	hackmodeltest = spawndrawable(filename,position,orientation);
	return (hackmodeltest)? "ok" : "unable to load model";
}
EXPORTFUNC(ezm);

String sceneclear(String)
{
	extern Array<Brush*> Brushes;
	int k=Brushes.count; 
	while(Brushes.count) 
		delete Brushes[Brushes.count-1]; // dont pop from list, the destructor does that.
	extern String beditrefresh(String);
	beditrefresh("");
	while(drawables.count)
	{
		delete drawables[drawables.count-1];
	}
	while(ShadowOnlyModels.count)
	{
		delete ShadowOnlyModels.Pop();
	}
	while(lights.count)
	{
		delete lights[lights.count-1];
	}
	while(Chuckables.count)
	{
		delete Chuckables[Chuckables.count-1];
	}
	return String("removed all brushes.  count=") + String(k);
}
EXPORTFUNC(sceneclear);



void SceneRender()
{
	int i;
	// PreRenderSetup
	// here's the game specific stuff to gather things into renderable data:
	{
		//RenderAreaTest();
		for(i=0;i<drawables.count;i++) 
		{
			drawables[i]->time += drawables[i]->animspeed * DeltaT;
			ModelAnimate(drawables[i]->model,drawables[i]->time);
			drawables[i]->model->position    = drawables[i]->position   ;
			drawables[i]->model->orientation = drawables[i]->orientation;
			ModelRender(drawables[i]->model);
		}

		BrushRenderTest();

		OrphanShadowRender();

		extern void CharactersRender();
		CharactersRender();

		extern void RenderChuckables();
		RenderChuckables();
		

		ManipulatorRender();

		BrushRenderTestAlpha();
	}
	// invoke the graphics engine to render the frame.
	extern void Render();
	Render();

	while(tempmodels.count) 
		delete tempmodels.Pop();
}

