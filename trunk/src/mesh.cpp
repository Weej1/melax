//  
//             Mesh 
//  
// by: s melax 
//  
//  
//
//

#define NOMINMAX 
#include <assert.h>
#include <d3dx9.h>
#include "mesh.h"
#include "winsetup.h"
#include "xmlparse.h"
#include "vecmath.h"
#include "vertex.h"
#include "camera.h"
#include "console.h"
#include "material.h"
#include "stringmath.h"  // for I/O support


class VertexOld  // to be depricated, dont need to support too many vertex buffer formats
{
  public:
	float3  position;
	float4  orientation;  // tangent space basis in quat form
	float3  normal;
	float2 texcoord;	
	float3  tangent;
	float3  binormal;
	VertexOld(){}
	VertexOld(const float3& _pos,const float3 &_nrm, const float2 &_tex, const float3 &_tng, const float3 &_bnm)
		:position(_pos),normal(_nrm),texcoord(_tex),tangent(_tng),binormal(_bnm){orientation = normalize(quatfrommat<float>(float3x3(_tng,_bnm,_nrm)));assert(dot(_nrm,rotate(((Quaternion&)orientation),float3(0,0,1)))>0.0f);}
	static char *semantic() { return "position normal texcoord tangent binormal";}
};



static inline const BYTE DeclTypeOf(byte4&   ){return D3DDECLTYPE_UBYTE4;}
static inline const BYTE DeclTypeOf(float4&  ){return D3DDECLTYPE_FLOAT4;}
static inline const BYTE DeclTypeOf(float3&  ){return D3DDECLTYPE_FLOAT3;}
static inline const BYTE DeclTypeOf(float2&  ){return D3DDECLTYPE_FLOAT2;}
static inline const BYTE DeclTypeOf(D3DCOLOR&){return D3DDECLTYPE_D3DCOLOR;}
#define DECLTYPEOF(C,A) (DeclTypeOf((( C *)NULL)-> A))

IDirect3DVertexDeclaration9 *vertexdecl;
IDirect3DVertexDeclaration9 *vertexskindecl;




extern Camera camera;

Quaternion &cameraq = camera.orientation;
float3     &camerap = camera.position;
float4x4& View=camera.view;
float4x4& Proj=camera.project;
float4x4& ViewProj=camera.viewproject;
float4x4& ViewInv=camera.viewinv;


EXPORTVAR(cameraq);
EXPORTVAR(camerap);

EXPORTVAR(View);
EXPORTVAR(Proj);
EXPORTVAR(ViewProj);
EXPORTVAR(ViewInv);


//------- I/O for data types -------------


inline StringIter &operator >>(StringIter &s,Pose &p)
{
	return s >> p.position >> p.orientation;
}
inline StringIter &operator >>(StringIter &s,KeyFrame &k)
{
	return s >> k.time >> k.pose;
}


inline String operator<<(String &s,const Vertex &v)
{
	s<< v.position;  s<< "  ";
	s<< v.orientation; s<< "  ";
	s<< v.texcoord;  s<< "  ";
	//s+= v.tangent;   s+= "  ";
	//s+= v.binormal;  
	return s;//+= ",\n";
}
inline StringIter &operator >>(StringIter &s,VertexOld &v)
{
	s >> v.position >> v.normal >> v.texcoord >> v.tangent >> v.binormal;
	v.orientation = normalize(quatfrommat<float>(float3x3(v.tangent,v.binormal,v.normal)));
	return s;
}

inline StringIter &operator >>(StringIter &s,Vertex &v)
{
	s >> v.position >> v.orientation >> v.texcoord ;
	return s;
}

inline String operator<<(String &s,const VertexS &v)
{
	s<< v.position;  s<< "  ";
	s<< v.orientation;  s<< "  ";
	s<< v.texcoord;  s<< "  ";
	s<< v.bones;     s<< "  ";
	s<< v.weights;   s<< "  ";
	//s+= v.normal;    s+= "  ";
	//s+= v.color;     s+= "  ";
	//s+= v.tangent;   s+= "  ";
	//s+= v.binormal;  
	return s;//+= ",\n";
}
inline StringIter &operator >>(StringIter &s,VertexS &v)
{
	return s >> v.position >> v.orientation  >> v.texcoord  >> v.bones >> v.weights;
}


//------------------------------


Bone::Bone(const char *_name, Model *_model, Bone *_parent) : id(_name), model(_model), parent(_parent),geometry(NULL)
{
}

Bone::~Bone()
{
}


DataMesh::~DataMesh()
{
	if(vb) vb->Release();
	if(ib) ib->Release();
	vb=NULL;
	ib=NULL;
	if(sverts)
		delete[] sverts;
	sverts=NULL;
}


Model::Model()
{
}
Model::~Model()
{
	int i;
	for(i=0;i<datameshes.count;i++)
	{
		delete datameshes[i];
	}
}
void ModelDelete(Model* model)
{
	delete model;
}

static void MakeVertexDecl()
{
	if(vertexdecl) return; 
	HRESULT hr;



	D3DVERTEXELEMENT9 vertlayout[] = 
	{
		{0, OFFSET(Vertex,position   ), DECLTYPEOF(Vertex,position   ), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,0},
		{0, OFFSET(Vertex,orientation), DECLTYPEOF(Vertex,orientation), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,1},  // not sure which usage makes most sense
		{0, OFFSET(Vertex,texcoord   ), DECLTYPEOF(Vertex,texcoord   ), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,0},
		{0, OFFSET(Vertex,color      ), DECLTYPEOF(Vertex,color      ), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR   ,0},
		D3DDECL_END()
	};
	hr = g_pd3dDevice->CreateVertexDeclaration(vertlayout,&vertexdecl);
	assert(hr == D3D_OK);
	assert(vertexdecl);

	D3DVERTEXELEMENT9 vertlayout_skin[] = 
	{
		{0, OFFSET(VertexS,position), DECLTYPEOF(VertexS,position), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,0},
		{0, OFFSET(VertexS,orientation), DECLTYPEOF(VertexS,orientation), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,1},  // not sure which usage makes most sense
		{0, OFFSET(VertexS,texcoord), DECLTYPEOF(VertexS,texcoord), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,0},
		{0, OFFSET(VertexS,bones   ), DECLTYPEOF(VertexS,bones   ), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES,0},
		{0, OFFSET(VertexS,weights ), DECLTYPEOF(VertexS,weights ), D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,0},
		D3DDECL_END()
	};
	hr = g_pd3dDevice->CreateVertexDeclaration(vertlayout_skin,&vertexskindecl);
	assert(hr == D3D_OK);
	assert(vertexskindecl);

}


int InitMaterial(xmlNode *xmlmesh)
{
	xmlNode *xmlmat;
	(xmlmat = xmlmesh->Child("material"))|| (xmlmat = xmlmesh->Child("materiallist")->Child("material"));
	assert(xmlmat);
	int matid = MaterialFind(xmlmat->body);
	if(matid<0) matid=0; 
	return  matid;
}


//	D3DXCreateTextureFromFile(g_pd3dDevice,"skeleton.tga",&texture) && VERIFY_RESULT;
//	assert(texture);

void BoxLimits(const Vertex *verticies,int count,float3 &bmin,float3 &bmax,int reset_prior_limits)
{
	if(reset_prior_limits)
	{
		bmin = float3( FLT_MAX, FLT_MAX, FLT_MAX);
		bmax = float3(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	}
	for(int i=0;i<count;i++)  // last chance to compute bounds.
	{
		bmin=VectorMin(bmin,verticies[i].position);
		bmax=VectorMax(bmax,verticies[i].position);
	}
}
void BoxLimits(const VertexS *verticies,int count,float3 &bmin,float3 &bmax,int reset_prior_limits)
{
	if(reset_prior_limits)
	{
		bmin = float3( FLT_MAX, FLT_MAX, FLT_MAX);
		bmax = float3(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	}
	for(int i=0;i<count;i++)  // last chance to compute bounds.
	{
		bmin=VectorMin(bmin,verticies[i].position);
		bmax=VectorMax(bmax,verticies[i].position);
	}
}


xmlNode *MakeXMLMesh(DataMesh *datamesh)
{
	int i;
	xmlNode *e = new xmlNode("mesh"); //XMLParseFile("meshtemplate.xml");
	assert(e);
	xmlNode *vertlist = new xmlNode("vertexbuffer",e);  	assert(vertlist);
	xmlNode *trilist  = new xmlNode("indexbuffer",e);     assert(trilist);
	(new xmlNode("material",e))->body = MaterialGetName(datamesh->matid);
	vertlist->attribute("count")=String(datamesh->vertices.count);
	vertlist->attribute("semantic") = datamesh->semantic();
	for(i=0;i<datamesh->vertices.count;i++)
	{
		vertlist->body << datamesh->vertices[i];
		vertlist->body << ",\n";
	}
	trilist->attribute("count") = String(datamesh->tris.count);
	for(i=0;i<datamesh->tris.count;i++)
	{
		trilist->body << datamesh->tris[i];
		trilist->body << ",\n";
	}
	return e;
}

xmlNode *MakeXMLModel(Model *model)
{
	// export a model into an xml format
	int i;
	xmlNode *e = new xmlNode("model"); 
	e->attribute("bmin") = String(model->bmin);
	e->attribute("bmax") = String(model->bmax);
	for(i=0;i<model->datameshes.count;i++)
	{
		e->children.Add(MakeXMLMesh(model->datameshes[i]));
	}
	return e;
}


DataMesh *DataMeshCreate(xmlNode *xmlmesh)
{
	assert(xmlmesh);
	assert(xmlmesh->tag == "mesh");
	if(!vertexdecl) {
		MakeVertexDecl();
	}
	xmlNode *vertlist = xmlmesh->Child("vertexbuffer");  	assert(vertlist);
	xmlNode *trilist  = xmlmesh->Child("indexbuffer");       assert(trilist);
	DataMesh *mesh = new DataMesh();
	int verts_count = vertlist->attribute("count").Asint();
	int tris_count = trilist->attribute("count").Asint();
	if(vertlist->attribute("semantic")=="position orientation texcoord")
	{
		ArrayImport(mesh->vertices,vertlist->body,verts_count);
	}
	else
	{
		Array<VertexOld> verts;
		ArrayImport(verts,vertlist->body,verts_count);
		for(int i=0;i<verts_count;i++) 
			mesh->vertices.Add(Vertex(verts[i].position,verts[i].orientation,verts[i].texcoord));
	}
	ArrayImport(mesh->tris,trilist->body,tris_count);
	mesh->matid = InitMaterial(xmlmesh);
    return mesh;
}

DataMesh *DataMeshDup(DataMesh *src)
{
	DataMesh *dst = new DataMesh();
	dst->manifold=src->manifold;
	dst->model=src->model;
	for(int i=0;i<src->vertices.count;i++)
		dst->vertices.Add(src->vertices[i]);
	for(int i=0;i<src->tris.count;i++)
		dst->tris.Add(src->tris[i]);
	dst->matid=src->matid;
	return dst;
}

DataMesh *DataMeshCreateSkin(xmlNode *xmlmesh)
{
	assert(xmlmesh);
	assert(xmlmesh->tag == "mesh");
	if(!vertexdecl) {
		MakeVertexDecl();
	}
	xmlNode *vertlist = xmlmesh->Child("vertexbuffer");  	assert(vertlist);
	xmlNode *trilist  = xmlmesh->Child("indexbuffer");       assert(trilist);
	DataMesh *mesh = new DataMesh();
	int verts_count = vertlist->attribute("count").Asint();
	int tris_count = trilist->attribute("count").Asint();
	mesh->manifold = (trilist->hasAttribute("manifold")) ? trilist->attribute("manifold").Asint() : 0;
	if(vertlist->attribute("semantic")==VertexS::semantic())
	{
		mesh->sverts = new VertexS[verts_count];
		ArrayImport(mesh->sverts,vertlist->body,verts_count);
		for(int i=0;i<verts_count;i++) 
			mesh->vertices.Add(Vertex(mesh->sverts[i].position,mesh->sverts[i].orientation,mesh->sverts[i].texcoord));
	}
	else if(vertlist->attribute("semantic")==Vertex::semantic() )
	{
		ArrayImport(mesh->vertices,vertlist->body,verts_count);
	}
	else
	{
		Array<VertexOld> verts;
		ArrayImport(verts,vertlist->body,verts_count);
		for(int i=0;i<verts_count;i++) 
			mesh->vertices.Add(Vertex(verts[i].position,verts[i].orientation,verts[i].texcoord));
	}
	ArrayImport(mesh->tris,trilist->body,tris_count);
	mesh->matid = InitMaterial(xmlmesh);
    return mesh;
}


void MeshHWCreate(DataMesh *datamesh)
{
	if(datamesh->vertices.count==0 || datamesh->tris.count==0) return ;
	if(!vertexdecl) {
		MakeVertexDecl();
	}
	if(datamesh->sverts)
	{
		VertexS *pverticies;
		datamesh->vdecl = vertexskindecl;
		g_pd3dDevice->CreateVertexBuffer(sizeof(VertexS)*datamesh->vertices.count,0,0,D3DPOOL_MANAGED,&datamesh->vb,NULL)   && VERIFY_RESULT;
		datamesh->vb->Lock(0,0,(void**)&pverticies,0);
		memcpy(pverticies,datamesh->sverts,datamesh->vertices.count*sizeof(VertexS));
		datamesh->vb->Unlock();
	}
	else
	{
		Vertex *pverticies;
		datamesh->vdecl = vertexdecl;
		g_pd3dDevice->CreateVertexBuffer(sizeof(Vertex)*datamesh->vertices.count,0,0,D3DPOOL_MANAGED,&datamesh->vb,NULL)   && VERIFY_RESULT;
		datamesh->vb->Lock(0,0,(void**)&pverticies,0);
		memcpy(pverticies,datamesh->vertices.element,datamesh->vertices.count*sizeof(Vertex));
		datamesh->vb->Unlock();
	}

	g_pd3dDevice->CreateIndexBuffer(datamesh->tris.count*sizeof(short3),0,D3DFMT_INDEX16, D3DPOOL_MANAGED,&datamesh->ib,NULL)  && VERIFY_RESULT;
	short3 *ptris;
	datamesh->ib->Lock(0,0,(void**)&ptris,0) && VERIFY_RESULT;
	memcpy(ptris,datamesh->tris.element,datamesh->tris.count*sizeof(short3));
	datamesh->ib->Unlock();
}


Pose GetPose(const Array<KeyFrame> &animation,float t)
{
	int k=0;
	while(k<animation.count && t>=animation[k].time) 
		k++;
	if(k==animation.count) 
		return animation[k-1].pose;
	if(k==0) 
		return animation[0].pose;
	float a = (t-animation[k-1].time) / (animation[k].time-animation[k-1].time);
	const Pose &p0 = animation[k-1].pose;
	const Pose &p1 = animation[k  ].pose;
	return slerp(p0,p1,a); // Pose(p0.position * a + p1.position * (1.0f-a),slerp(p0.orientation,p1.orientation,a));
}

int dxskin=1;
EXPORTVAR(dxskin);

float4 qmul(float4 a,float4 b) {return Quaternion(a)*Quaternion(b);}
float4 qconj(float4 a) {return Conjugate(Quaternion(a));}
Pose skin_dualquat(byte4 bones, float4 weights,Quaternion *currentposeq,float3 *currentposep)  // dual quat skinning (screw motion)
{
	Pose p;
	float4 b0=currentposeq[bones[0]];
	float4 b1=currentposeq[bones[1]];
	float4 b2=currentposeq[bones[2]];
	float4 b3=currentposeq[bones[3]];
	if(dot(b0,b1)<0) b1=-(b1);
	if(dot(b0+b1,b2)<0) b2=-(b2);
	if(dot(b0+b1+b2,b3)<0) b3=-(b3);
		float4 q = (
		b0*weights[0] +
		b1*weights[1] +
		b2*weights[2] +
		b3*weights[3]  );
	p.position =         // convert translational inputs to dualquat's dual part and back.
	   qmul( 
		  qmul(float4(currentposep[bones[0]],0),b0) * weights[0] +
		  qmul(float4(currentposep[bones[1]],0),b1) * weights[1] +
		  qmul(float4(currentposep[bones[2]],0),b2) * weights[2] +
		  qmul(float4(currentposep[bones[3]],0),b3) * weights[3]  
		 ,qconj(q)
		).xyz() /dot(q,q); // this works since q hasn't been normalized yet
	p.orientation = (Quaternion&)normalize(q);
	return p;
}
void ModelMeshSkin(Model *model)
{
	if(dxskin)  //using gpu skinning (or still cpu if dx initialized with softwarevertexprocessing)
		return;  // dont need to update regular format array vertices[] 
	for(int m=0;m<model->datameshes.count;m++)
	{
		DataMesh *mesh = model->datameshes[m];
		if(!mesh->sverts) continue;
		VertexS *sv = mesh->sverts;
		for(int i=0;i<mesh->vertices.count;i++)
		{
			Pose p=skin_dualquat(sv[i].bones,sv[i].weights,model->currentposeq.element,model->currentposep.element);
			mesh->vertices[i].position    = rotate(p.orientation,sv[i].position) + p.position;
			mesh->vertices[i].orientation = qmul(p.orientation,sv[i].orientation);
		}
	}
}

void ModelAnimate(Model *model,float t)
{
	// Set pose of each of the bones based on time t using animation associated with model.
	// Also sets the matrix array for the model using the hierarchy.
	// If no animation for a bone we dont touch it.  let the gamecode decide those bones.
	int i;
	if(model->currentpose.count == 0) return ; // warning here would be good.
	for(i=0;i<model->currentpose.count;i++)
	{
		Bone *b =model->skeleton[i]; 
		if(b->animation.count>1)
		{
			b->pose() = GetPose(b->animation,fmodf(t,b->animation[b->animation.count-1].time));
		}

		b->modelpose = (b->parent)? b->parent->modelpose * b->pose() : b->pose() ;
		Pose p = b->modelpose * Inverse( b->basepose );
		model->currentpose[i] =  MatrixFromQuatVec(model->orientation,model->position) * MatrixFromQuatVec(p.orientation,p.position);
		Quaternion mq = model->orientation;
		model->currentposeq[i] =  mq * p.orientation; 
		model->currentposep[i] =  model->position +  rotate(mq,p.position);
	}
}

int skinalgorithm=2; // 0 none, 1 single quat, 2 dual quat, 3 blended post-transform (uses quats but equiv to matrix skinning)
EXPORTVAR(skinalgorithm);

int trymat;
EXPORTVAR(trymat);

void DrawDataMesh(DataMesh *datamesh)
{
	if(!datamesh) return;
	if(datamesh->tris.count==0) {return;}
	assert(datamesh->vertices.count);

	LPD3DXEFFECT effect = MaterialSetup(datamesh->matid);
	if(!effect) return;  // this material is not to be rendered at this time
	// done by material now: effect->SetTechnique((hack_usealpha)?"alphatechnique":"t0") && effect->SetTechnique("t0") &&VERIFY_RESULT;

	int skinning=(dxskin && datamesh->model->currentpose.count);
	if(skinning)
	{
		effect->SetInt("useskin",skinalgorithm);
		effect->SetFloatArray("currentposep",(float*)datamesh->model->currentposep.element,datamesh->model->currentposep.count*3) && VERIFY_RESULT;
		effect->SetFloatArray("currentposeq",(float*)datamesh->model->currentposeq.element,datamesh->model->currentposep.count*4) && VERIFY_RESULT;
	
	}
	else
	{
		effect->SetInt("useskin",0);
		effect->SetInt("trymat",trymat);
	}

	if(datamesh->vb)
	{
		g_pd3dDevice->SetVertexDeclaration(datamesh->vdecl) && VERIFY_RESULT;
		g_pd3dDevice->SetStreamSource(0,datamesh->vb,0,(datamesh->vdecl==vertexskindecl)?sizeof(VertexS):sizeof(Vertex))  && VERIFY_RESULT;
		g_pd3dDevice->SetIndices(datamesh->ib) && VERIFY_RESULT;
		unsigned int n;
		effect->Begin(&n,1)  &&VERIFY_RESULT;
		for(int i=0;i<(int)n;i++)
		{
			effect->BeginPass(i)      &&VERIFY_RESULT; 
			g_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,datamesh->vertices.count,0,datamesh->tris.count) &&VERIFY_RESULT;
			effect->EndPass();

		}
		effect->End()        &&VERIFY_RESULT;
	}
	else
	{
		g_pd3dDevice->SetVertexDeclaration((skinning)?vertexskindecl:vertexdecl) && VERIFY_RESULT;
		unsigned int n;
		effect->Begin(&n,1)  &&VERIFY_RESULT;
		for(int i=0;i<(int)n;i++)
		{
			effect->BeginPass(i)      &&VERIFY_RESULT; 
			if(skinning)
				g_pd3dDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,datamesh->vertices.count,datamesh->tris.count,&datamesh->tris[0],D3DFMT_INDEX16,datamesh->sverts,sizeof(VertexS)) &&VERIFY_RESULT;
			else
				g_pd3dDevice->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,datamesh->vertices.count,datamesh->tris.count,&datamesh->tris[0],D3DFMT_INDEX16,&datamesh->vertices[0],sizeof(Vertex)) &&VERIFY_RESULT;
			effect->EndPass();
		}
		effect->End()        &&VERIFY_RESULT;
	}
}


int ModelCompileMeshes(Model *model)
{
	int i;
	for(i=0;i<model->datameshes.count;i++)
	{
		MeshHWCreate(model->datameshes[i]);
	}
	return i;
}

void ModelDeleteNonShadows(Model *model)
{
	int shadowmatid = MaterialFind("extendvolume");
	int i=model->datameshes.count;
	while(i--)
	{
		if(model->datameshes[i]->matid != shadowmatid)
		{
			delete model->datameshes[i];
			model->datameshes.DelIndex(i);
		}
	}
}

static Array<Vertex> linebucket;
Vertex VertexPC(const float3 &p,const D3DCOLOR &c) {Vertex v;v.position=p,v.color=c;return v;}
void Line(const float3 &_v0,const float3 &_v1,const float3 &_c)
{

	D3DCOLOR c = D3DCOLOR_COLORVALUE(_c.x,_c.y,_c.z,1.0f);
	linebucket.Add(VertexPC(_v0,c));
	linebucket.Add(VertexPC(_v1,c));
}
void DrawBox(const float3 &bmin,const float3 &bmax,const float3 &color)
{
	float3 p[2];
	p[0] = bmin;
	p[1] = bmax;
	for(int i=0;i<4;i++)
	{
		Line(float3(p[i%2].x,p[i/2].y,p[0].z  ),float3(p[i%2].x,p[i/2].y,p[1].z  ),color);
		Line(float3(p[i%2].x,p[0].y  ,p[i/2].z),float3(p[i%2].x,p[1].y  ,p[i/2].z),color);
		Line(float3(p[0].x  ,p[i/2].y,p[i%2].z),float3(p[1].x  ,p[i/2].y,p[i%2].z),color);
	}
}

void RenderLines()
{
	int i;
	unsigned int n;
	int matid=MaterialFind("marker");
	assert(matid>=0);
	LPD3DXEFFECT effect = MaterialSetup(matid);
	assert(effect);
	g_pd3dDevice->SetVertexDeclaration(vertexdecl) && VERIFY_RESULT;
	effect->Begin(&n,1)  &&VERIFY_RESULT;
	for(i=0;i<(int)n;i++)
	{
		effect->BeginPass(i)      &&VERIFY_RESULT; 
		g_pd3dDevice->DrawPrimitiveUP(D3DPT_LINELIST,linebucket.count/2,linebucket.element,sizeof(Vertex))&&VERIFY_RESULT;
		effect->EndPass();
	}
	effect->End()        &&VERIFY_RESULT;
	linebucket.count=0;
	g_pd3dDevice->SetVertexDeclaration(vertexdecl) && VERIFY_RESULT;
}





String meshpath("./ models/ meshes/");

Model *ModelLoad(const char *filename)
{
	xmlNode *e = NULL;
	String f=filefind(filename,meshpath,".xml .ezm");
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
	delete e;
	return model;
}
Model *ModelLoad(xmlNode *mnode)
{
	Model *model = new Model();
	model->bmin = float3( FLT_MAX, FLT_MAX, FLT_MAX);
	model->bmax = float3(-FLT_MAX,-FLT_MAX,-FLT_MAX);
	for(int i=0;i<mnode->children.count;i++) 
	  if(mnode->children[i]->tag=="mesh")
	{
		xmlNode *c = mnode->children[i];
		DataMesh *datamesh = DataMeshCreateSkin(c);
		BoxLimits(datamesh->vertices.element,datamesh->vertices.count,model->bmin,model->bmax,0);
		datamesh->model=model;

		//MeshHW *mesh = MeshHWCreate(datamesh); //((c->Child("vertexbuffer")->attribute("semantic")==VertexS::semantic())?MeshCreateSkin:MeshCreate)(c,model->bmin,model->bmax);
		//MeshHW *mesh = ((c->Child("vertexbuffer")->attribute("semantic")==VertexS::semantic())?MeshCreateSkin:MeshCreate)(c,model->bmin,model->bmax);
		//model->meshes.Add(mesh);
		model->datameshes.Add(datamesh);
		//assert(mesh->model == model);
		// temp hack:
		if(datamesh->manifold && datamesh->matid != MaterialFind("extendvolume"))
		{
			DataMesh *datameshshadow = DataMeshCreateSkin(c); //DataMeshDup(datamesh);
			datameshshadow->model=model;
			datameshshadow->matid = MaterialFind("extendvolume");
			//*meshshadow=*mesh;
			// MeshCreateSkin(c,model->bmin,model->bmax);  // create a duplicate - not the most efficient, but works for now.
			//model->meshes.Add(meshshadow);
			model->datameshes.Add(datameshshadow);
			// MeshHW *meshshadow;// = new MeshHW;
			//meshshadow = MeshHWCreate(datameshshadow); 
			//assert(meshshadow->model == model);
			//assert(meshshadow->matid == MaterialFind("extendvolume"));
		}
	}
	xmlNode *snode = mnode->Child("skeleton");
	if(snode)
	{
		Array<VertexS> vertices;
		ArrayImport(vertices,mnode->Child("mesh")->Child("vertexbuffer")->body,mnode->Child("mesh")->Child("vertexbuffer")->attribute("count").Asint());  // dont like having this here!!!
		//ArrayImport(vertices,mnode->Child("mesh")->Child("vertexbuffer")->body,model->datameshes[0]->vertices.count);  // dont like having this here!!!
		for(int i=0;i<snode->children.count;i++)
		{
			xmlNode *bnode = snode->children[i];
			if(bnode->tag != "bone") continue; // not a bone ?????
			Bone *parent=NULL;
			for(int j=0;j<model->skeleton.count;j++)
				if(bnode->hasAttribute("parent") && bnode->attribute("parent")==model->skeleton[j]->id)
					parent = model->skeleton[j];
			Bone *b = new Bone(bnode->attribute("name"),model,parent);
			model->skeleton.Add(b);
			model->currentpose.Add(float4x4(1.0f,0,0,0,0,1.0f,0,0,0,0,1.0f,0,0,0,0,1.0f));
			model->currentposep.Add(float3(0,0,0));
			model->currentposeq.Add(Quaternion(0,0,0,1));
			StringIter(bnode->attribute("orientation")) >> b->orientation   ;
			b->position = AsFloat3(bnode->attribute("position"));  // local
			b->basepose = b->parent ? b->parent->basepose * b->pose() : b->pose();  // base pose in localmost
			Array<float3> points;
			for(int j=0;j<vertices.count;j++)
				for(int k=0;k<4;k++)
					if(vertices[j].bones[k]==i && vertices[j].weights[k] > 0.35f)
						points.Add(Inverse(b->basepose)*vertices[j].position);
			int3 *tris;
			int  tris_count;
			assert(points.count);
			if(calchull(points.element,points.count,tris,tris_count,40))
				b->geometry = WingMeshCreate(points.element,tris,tris_count);
			assert(b->geometry);
		}
	}
	xmlNode *anode = mnode->Child("animation");
	if(anode)
	{
		for(int i=0;i<anode->children.count;i++)
		{
			xmlNode *tnode = anode->children[i];
			Bone *b=NULL;
			for(int j=0;j<model->skeleton.count;j++)
				if(tnode->attribute("name")==model->skeleton[j]->id)
					b=model->skeleton[j];
			if(!b) continue; // no bone for this animation.
			ArrayImport(b->animation,tnode->body,tnode->attribute("count").Asint());
		}
	}
	// todo: get bmin and bmax from file if available.
	return model;
}

float2 texgen(const float3 &p,const float3 &n)
{
	float3 gu,gv;
	if(fabsf(n.x)>fabsf(n.y) && fabsf(n.x)>fabsf(n.z)) 
	{
		gu = float3(0,1,0);
		gv = float3(0,0,1);
	}
	else if(fabsf(n.y)>fabsf(n.z)) 
	{
		gu = float3(1,0,0);
		gv = float3(0,0,1);
	}
	else 
	{
		gu = float3(1,0,0);
		gv = float3(0,1,0);
	}
	return float2(dot(p,gu),dot(p,gv));
}



