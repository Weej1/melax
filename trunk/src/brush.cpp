




#include <assert.h>
#include <stdio.h>
#include "brush.h"
#include "winsetup.h"
#include "array.h"
#include "vecmath.h"
#include "xmlparse.h"
#include "bsp.h"
#include "wingmesh.h"
#include "vertex.h"
#include "console.h"
#include "material.h"
#include "qplane.h"
#include "console.h"
#include "mesh.h"





void Smooth(DataMesh *mesh,float creaseangle)
{
	Array<Vertex> &vertices = mesh->vertices;
	Array<short3> &tris     = mesh->tris;
	int i;
	Array<float> areas;
	float cosangle = cosf(DegToRad(creaseangle));
	for(i=0;i<vertices.count;i++)
	{
		areas.Add(0);
	}
	for(i=0;i<tris.count;i++)
	{
		short3 t = tris[i];
		float a = magnitude(cross(vertices[t[1]].position-vertices[t[0]].position,vertices[t[2]].position-vertices[t[0]].position));
		areas[t[0]] +=a;
		areas[t[1]] +=a;
		areas[t[2]] +=a;
	}
	Array<int> group;
	for(i=0;i<vertices.count;i++)
	{
		if(areas[i]==0.0f) continue;
		int j;
		group.count=0;
		group.Add(i);
		for(j=i+1;j<vertices.count;j++)
		{
			if(areas[j]==0.0f) continue;
			
			if( vertices[i].position==vertices[j].position  && dot(vertices[i].normal,vertices[j].normal) > cosangle)
				group.Add(j);
			else if(magnitude(vertices[i].position-vertices[j].position)<0.001f && dot(vertices[i].normal,vertices[j].normal) > cosangle)
				group.Add(j);
		}
		float3 t,b,n;
		for(j=0;j<group.count;j++)
		{
			t+= areas[group[j]] * vertices[group[j]].tangent;
			b+= areas[group[j]] * vertices[group[j]].binormal;
			n+= areas[group[j]] * vertices[group[j]].normal;
		}
		t = normalize(t);
		b = normalize(b);
		n = normalize(n);
		for(j=0;j<group.count;j++)
		{
			vertices[group[j]].tangent  =  t ;
			vertices[group[j]].binormal =  b ;
			vertices[group[j]].normal   =  n ;
			vertices[group[j]].orientation = normalize(quatfrommat<float>(float3x3(t,b,n)));
			assert(dot(n,rotate((Quaternion&)vertices[group[j]].orientation,float3(0,0,1)))>0.0f);
			areas[group[j]]=0.0f;
		}
	}
}

float brush_smooth_angle=0.0f;
EXPORTVAR(brush_smooth_angle);

Model *ModelText(String s_,Model *model,float2 size)
{
	const char *s=s_;
	float x=0.0f;
	if(!model)
	{	
		model = 	new Model;
		DataMesh *m=m=new DataMesh;
		m->matid = MaterialFind("font");
		m->model=model;
		model->datameshes.Add(m);
	}
	DataMesh *m=model->datameshes[0];
	m->vertices.count=0;
	m->tris.count=0;
	float y=-1.0f;
	float maxx=1.0f;
	while(*s)
	{
		char c = *s++;
		if(c=='\n'){y=y-1.0f;x=0;continue;}
        if( (c-32) < 0 || (c-32) >= 128-32 ||  c == ' ' )
		{
            x+=0.5f;
			continue;
		}
		extern float4 getfontlookup(int c);
		float4 tex = getfontlookup(c);
		extern int   getfontspacing();  // this shouldn't exist. datafile's texcoords should be adjust (todo)
        float tx0 = tex[0]+getfontspacing()/256.0f;
        float ty0 = 1.0f-tex[1];
        float tx1 = tex[2]-getfontspacing()/256.0f;
        float ty1 = 1.0f-tex[3];
		float3 p = float3((float)x,(float)y, 0.0f); 
		float w = (tx1-tx0) / (ty0-ty1);
		int b=m->vertices.count;
        m->vertices.Add(Vertex(float3((x+0),(y+0.0f), 0.0f) ,float4(0,0,0,1),float2(tx0,ty1)));
        m->vertices.Add(Vertex(float3((x+w),(y+0.0f), 0.0f) ,float4(0,0,0,1),float2(tx1,ty1)));
        m->vertices.Add(Vertex(float3((x+0),(y+1.0f), 0.0f) ,float4(0,0,0,1),float2(tx0,ty0)));
        m->vertices.Add(Vertex(float3((x+w),(y+1.0f), 0.0f) ,float4(0,0,0,1),float2(tx1,ty0)));
		m->tris.Add(short3(b+0,b+1,b+2));
		m->tris.Add(short3(b+2,b+1,b+3));
		x+=w;
		maxx=Max(maxx,x);
	}
	float scale = 1.0f/Max(maxx/size.x,-y/size.y);
	for(int i=0;i<m->vertices.count;i++)
	{
		m->vertices[i].position = (m->vertices[i].position + float3(0,-y,0)) * scale-float3(size.x,size.y,0)*0.5f;
	}
	return model;
}

Model *MakeBrepBuffersMat(const Array<Face*> &faces,int matid_override)
{
	float3 bmin,bmax;
	int i,j;
	Array<DataMesh*> dms;
	for(i=0;i<Materials.count;i++)
	{
		dms.Add(NULL);
		//if(matid_override!=-1) break; // just need one mesh.
	}
	for(i=0;i<faces.count;i++)
	{
			Face *f=faces[i];
			int matid = (matid_override>=0)?matid_override:f->matid;
			if(!dms[matid]) 
			{
				dms[matid] = new DataMesh();
				dms[matid]->matid = matid;
			}
			DataMesh *datamesh = dms[matid];
			int base = datamesh->vertices.count;
			float3 tng = safenormalize(f->gu);
			float3 bnm = safenormalize(cross(f->normal(),tng));
			tng = safenormalize(cross(bnm,f->normal()));
			datamesh->vertices.Add(Vertex(f->vertex[0],QuatFromMat(tng,bnm,f->normal()),FaceTexCoord(f,0)));
			datamesh->vertices.Add(Vertex(f->vertex[1],QuatFromMat(tng,bnm,f->normal()),FaceTexCoord(f,1)));
			bmin=VectorMin(bmin,f->vertex[0]);
			bmax=VectorMax(bmax,f->vertex[0]);
			bmin=VectorMin(bmin,f->vertex[1]);
			bmax=VectorMax(bmax,f->vertex[1]);
			for(j=2;j<f->vertex.count;j++)
			{
				datamesh->vertices.Add(Vertex(f->vertex[j],QuatFromMat(tng,bnm,f->normal()),FaceTexCoord(f,j)));
				datamesh->tris.Add(short3(base+0,base+j-1,base+j));
				bmin=VectorMin(bmin,f->vertex[j]);
				bmax=VectorMax(bmax,f->vertex[j]);
			}

	}
	Model *model = new Model;
	model->bmin=bmin;
	model->bmax=bmax;
	for(i=0;i<dms.count;i++)
	{
		if(dms[i]) 
		{
			if(brush_smooth_angle>0.0f) 
				Smooth(dms[i],brush_smooth_angle);
			model->datameshes.Add(dms[i]);
			dms[i]->model = model;
		}
	}
	return model;
}

Model *MakeBrepBuffersMat(BSPNode *bsp,int matid_override)
{
	Array<Face*> faces;
	BSPGetBrep(bsp,faces);
	return MakeBrepBuffersMat(faces,matid_override);
}

Model *MakeBrepBuffersMatOld(BSPNode *bsp,int matid_override)
{
	if(!bsp) return NULL;
	int i,j;
	Array<DataMesh*> dms;
	for(i=0;i<Materials.count;i++)
	{
		dms.Add(NULL);
		//if(matid_override!=-1) break; // just need one mesh.
	}
	Array<BSPNode*>stack;
	stack.Add(bsp);
	while(stack.count)
	{
		BSPNode *b = stack.Pop();
		if(!b)continue;
		stack.Add(b->over);
		stack.Add(b->under);
		for(i=0;i<b->brep.count;i++)
		{
			Face *f=b->brep[i];
			int matid = (matid_override>=0)?matid_override:f->matid;
			if(!dms[matid]) 
			{
				dms[matid] = new DataMesh();
				dms[matid]->matid = matid;
			}
			DataMesh *datamesh = dms[matid];
			int base = datamesh->vertices.count;
			float3 tng = safenormalize(f->gu);
			float3 bnm = safenormalize(cross(f->normal(),f->gu));
			datamesh->vertices.Add(Vertex(f->vertex[0],QuatFromMat(tng,bnm,f->normal()),FaceTexCoord(f,0)));
			datamesh->vertices.Add(Vertex(f->vertex[1],QuatFromMat(tng,bnm,f->normal()),FaceTexCoord(f,1)));
			for(j=2;j<f->vertex.count;j++)
			{
				datamesh->vertices.Add(Vertex(f->vertex[j],QuatFromMat(tng,bnm,f->normal()),FaceTexCoord(f,j)));
				datamesh->tris.Add(short3(base+0,base+j-1,base+j));
			}
		}
	}
	Model *model = new Model;
	for(i=0;i<dms.count;i++)
	{
		if(dms[i]) model->datameshes.Add(dms[i]);
	}
	return model;
}



inline String &operator<<(String &s,const BSPNode &n)
{
	return s << n.isleaf << "   " << n.normal() << " " << n.dist()   ;
}

inline StringIter &operator >>(StringIter &s,BSPNode &n)
{
	return s >> n.isleaf >> n.normal() >> n.dist();
}




inline String appendtree(String &s,const BSPNode *n)
{
	static int d=0;
	if(!n) return s;
	d++;
	for(int i=0;i<d;i++){ s << "  ";} // indent
	s << *n << ",\n";
	appendtree(s,n->under);
	appendtree(s,n->over);
	d--;
	return s;
}
BSPNode *BSPImport(StringIter &s)
{
	BSPNode *n=new BSPNode(float3(0,0,0),0);
	s >> *n;
	if(!n->isleaf)
	{
		n->under = BSPImport(s);
		n->over  = BSPImport(s);
	}
	return n;
}

xmlNode *MakeXMLBSP(BSPNode *bsp)
{
	char *semantic="celltype plane";
//	unsigned char * data=Linearize(bsp,OFFSET(BSPNode,under),OFFSET(BSPNode,over),sizeof(BSPNode),ctype,&count);
	xmlNode *b = new xmlNode("bsp");
	//b->data = data;
	b->attribute("semantic") = semantic;
	b->attribute("count") = String(BSPCount(bsp));
	b->body << "\n";
	appendtree(b->body,bsp);
	xmlNode *childhak = new xmlNode("x");
	b->children.Add(childhak);
	return b;
}

static void ExtractFaces(xmlNode *e, Array<Face *> &faces)
{
	int i,j;
	for(i=0;i<e->children.count;i++) 
	  if(e->children[i]->tag=="mesh")
	{
		extern DataMesh *DataMeshCreate(xmlNode *xmlmesh);
		DataMesh *datamesh = DataMeshCreate(e->children[i]);
		Array<Vertex> &verts = datamesh->vertices;
		Array<short3> &tris = datamesh->tris;
		for(j=0;j<tris.count;j++)
		{
			float3 &v0 = verts[tris[j][0]].position;
			float3 &v1 = verts[tris[j][1]].position;
			float3 &v2 = verts[tris[j][2]].position;
			float2 &t0 = verts[tris[j][0]].texcoord;
			float2 &t1 = verts[tris[j][1]].texcoord;
			float2 &t2 = verts[tris[j][2]].texcoord;
			Face *fc = FaceNewTriTex(v0,v1,v2,t0,t1,t2);
			fc->matid  = datamesh->matid; // todo:  get e->children[i]'s material index;
			faces.Add(fc);
		}
		delete datamesh;
	}
}

int forcerecompile=0;
EXPORTVAR(forcerecompile);
BSPNode *MakeBSP(xmlNode *model)
{
	Array<Face *> faces;
	ExtractFaces(model,faces);
	assert(faces.count);
	BSPNode *bsp;
	xmlNode *bx = model->Child("bsp");
	BBox bbox = VertexExtremes(faces);
	if(!forcerecompile &&  bx && bx->hasAttribute("semantic") && bx->attribute("semantic")=="celltype plane" )
	{
		bsp = BSPImport(StringIter(bx->body));
		BSPDeriveConvex(bsp,WingMeshCube(bbox.bmin-float3(10,10,10),bbox.bmax+float3(10,10,10)));
	}
	else
	{
		bsp = BSPCompile(faces,WingMeshCube(bbox.bmin-float3(10,10,10),bbox.bmax+float3(10,10,10)));
		xmlNode *belem = MakeXMLBSP(bsp); 
		model->children.Add(belem);
	}
	BSPMakeBrep(bsp,faces);

	return bsp;
}


xmlNode *MakeXMLModel(BSPNode *bsp)
{
	Model *model = MakeBrepBuffersMat(bsp,-1);
	extern xmlNode *MakeXMLModel(Model *model);
	xmlNode *m=MakeXMLModel(model);
	delete model;
	return m;
}


LDECLARE(Brush,highlight);
LDECLARE(Brush,position);
LDECLARE(Brush,velocity);
LDECLARE(Brush,shadowcast);
LDECLARE(Brush,filename);
LDECLARE(Brush,collide);
LDECLARE(Brush,onenter);
LDECLARE(Brush,onexit);
LDECLARE(Brush,onclick);
LDECLARE(Brush,message);
LDECLARE(Brush,mergeable);
LDECLARE(Brush,bspclip);

Array<Brush*> Brushes;
Brush::Brush(const char *_name):Entity(_name),shadowcast(1),collide(1),playerinside(0)
{
	bsp=NULL;model=NULL;
	highlight=0;
	LEXPOSEOBJECT(Brush,id);
	bspclip=0;
	mergeable=1; // 0 is probably a better default
	Brushes.Add(this);
	draw=1;
}

Brush::~Brush()
{
	extern Brush *Area;
	if(Area==this)
	{
		Area=NULL;
		extern BSPNode *area_bsp;
		area_bsp=NULL;
	}
	extern Brush *currentroom;
	extern Brush *groundcontact;
	if(this==currentroom)   currentroom=NULL;
	if(this==groundcontact) groundcontact=NULL;
	Brushes.Remove(this);
}
void Brush::Modified()
{
	if(model) ModelDelete(model);
	model=NULL;
	Array<Face*> faces;
	BSPGetBrep(this->bsp,faces);
	BBox bbox = VertexExtremes(faces);
	bmin = bbox.bmin;
	bmax = bbox.bmax;
	extern void PhysicsPurgeContacts();
	PhysicsPurgeContacts();
}

void Retool(Brush *brush)
{
	Array<Face*> faces;
	BSPRipBrep(brush->bsp,faces);
	BBox bounds = VertexExtremes(faces);
	if(faces.count==0) bounds.bmin=bounds.bmax=float3(0,0,0);
	BSPDeriveConvex(brush->bsp,WingMeshCube(bounds.bmin-float3(10,10,10),bounds.bmax+float3(10,10,10)));
	BSPMakeBrep(brush->bsp,faces);
	brush->Modified();
	//AssignTex(brush->bsp);
}

void ReCompile(Brush *brush)
{
	Array<Face*> faces;
	BSPRipBrep(brush->bsp,faces);
	BBox bounds = VertexExtremes(faces);
	delete brush->bsp;
	brush->bsp = BSPCompile(faces,WingMeshCube(bounds.bmin-float3(10,10,10),bounds.bmax+float3(10,10,10)));
	BSPMakeBrep(brush->bsp,faces);
	brush->Modified();
}


String brushpath("./ models/ brushes/ meshes/");
Brush *BrushLoad(String _filename,String additionalpath)
{
	xmlNode *e = NULL;
	String filename = filefind(_filename,additionalpath+" "+brushpath,".bsh .brush .xml");
	e=XMLParseFile(filename);
	if(!e) return NULL;
	Brush *brush = new Brush(splitpathfname(filename));
	brush->filename = filename;
	brush->bsp = MakeBSP(e);
	brush->shadowcast = !HitCheck(brush->bsp,1,float3(0,0,-1000.0f),float3(0,0,-1000.0f),NULL);
	xmlimport(brush,GetClass("brush"),e);  
	assert(!brush->model);
	brush->Modified(); 
	return brush;
}
void BrushSave(Brush *brush,String _filename)
{
	if(_filename != "") brush->filename = _filename;
	brush->id = splitpathfname(brush->filename);
	xmlNode *m = MakeXMLModel(brush->bsp);
	xmlNode *b = MakeXMLBSP(brush->bsp);
	m->children.Add(b);
	//	need to add something to export all the additional data
	XMLSaveFile(m,brush->filename);
	delete m;
}

Brush* BrushFind(String name)
{
	for(int i=0;i<Brushes.count;i++) if(Brushes[i]->id==name) return Brushes[i];
	return NULL;
}

int GetEnvBSPs(Array<BSPNode*>& bsps)
{
	extern BSPNode *area_bsp;
	bsps.count=0;
	for(int i=0;i<Brushes.count;i++)
		if(Brushes[i]->collide==3)
			bsps.Add(Brushes[i]->bsp);
	if(area_bsp && !bsps.Contains(area_bsp)) 
		bsps.Add(area_bsp);
	return bsps.count;
}
