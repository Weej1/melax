//
//  (c) Stan Melax 2004
//
// a simple subdivision surface implementation.
// based on rectangular meshes.
// used for a cloth demo.
//

#include "vertex.h"
#include "mesh.h"
#include "console.h"
#include "material.h"

float kobbelt_thickness;
EXPORTVAR(kobbelt_thickness);

static int kob_verts_required(int w,int h,int s)
{
	return (((w-1)<<s)+1) * (((h-1)<<s)+1);
}

static void KobbeltUpdateLin(Vertex *verts,int stride, int s,int n)
{
	while(s--)
	{
		for(int i=0;i<n-1;i++)
		{
			int im2 = ((i==0)?i:i-1) * stride;
			int im1 = (i+0) * stride;
			int ip1 = (i+1) * stride;
			int ip2 = ((i==n-2)?i+1:i+2) * stride;
			verts[(im1+ip1)/2].position =  verts[im2].position*(-1.0f/16.0f) + verts[im1].position*( 9.0f/16.0f) 
			                             + verts[ip1].position*( 9.0f/16.0f) + verts[ip2].position*(-1.0f/16.0f);
		}
		n=n*2-1;
		stride/=2;
	}
}

template<class T>
void KobbeltUpdate(const T *pos,int w,int h,int s,Array<Vertex> &verts)
{
	assert(verts.count/2==kob_verts_required(w,h,s));
	int rowsize = ((w-1)<<s)+1;  assert(rowsize==kob_verts_required(w,1,s));
	int rowcount= ((h-1)<<s)+1;  assert(rowcount==kob_verts_required(1,h,s));
	assert(rowsize*rowcount==verts.count/2);
	int gap     = kob_verts_required(2,1,s)-1;
	int i,j;
	int v=0;
	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			verts[v].position = *(float3*)(& pos[i*w+j]);//pos[i*w+j].xyz();
			v+= (j==w-1)? 1 : gap;
		}
		if(i!=h-1) v+= (gap-1)*rowsize;
	}
	assert(v == verts.count/2);
	v=0;
	for(j=0;j<w;j++)
	{
		KobbeltUpdateLin(&verts[v],rowsize*gap,s,h);
		v+=gap;
	}
	v=0;
	while(v<rowsize*rowcount)
	{
		KobbeltUpdateLin(&verts[v],gap,s,w);
		v+=rowsize;
	}
	for(i=0;i<rowcount;i++)
	{
		for(j=0;j<rowsize;j++)
		{
			int v = j+i*rowsize;
			int vw= (j==0)? v:v-1;			
			int ve= (j==rowsize-1)? v:v+1;
			int vn= (i==0)? v:v-rowsize;
			int vs= (i==rowcount-1)?v:v+rowsize;
			float3 binormal= normalize(verts[vn].position-verts[vs].position);
			float3 normal  = normalize(cross(verts[ve].position-verts[vw].position,verts[vn].position-verts[vs].position));
			float3 tangent = cross(binormal,normal);  //normalize(verts[ve].position-verts[vw].position);
			verts[v].orientation = normalize(quatfrommat<float>(float3x3(tangent,binormal,normal)));
		}
	}
	assert(v==verts.count/2);
	Vertex *vertsback = verts.element + rowcount*rowsize;
	for(i=0;i<rowcount*rowsize;i++)
	{
		vertsback[i].position   = verts[i].position - ((Quaternion&)verts[i].orientation).zdir()*kobbelt_thickness;
		vertsback[i].orientation= (Quaternion&)verts[i].orientation*Quaternion(0,1,0,0);
	}
}

void KobbeltUpdate(const float *pos,int stride_inner,int stride_outer,int w,int h,int s,Array<Vertex> &verts)
{
	assert(stride_inner >=1);
	assert(stride_outer >=3);
	assert(verts.count/2==kob_verts_required(w,h,s));
	int rowsize = ((w-1)<<s)+1;  assert(rowsize==kob_verts_required(w,1,s));
	int rowcount= ((h-1)<<s)+1;  assert(rowcount==kob_verts_required(1,h,s));
	assert(rowsize*rowcount==verts.count/2);
	int gap     = kob_verts_required(2,1,s)-1;
	int i,j;
	int v=0;
	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			verts[v].position.x = pos[(i*w+j) *stride_outer + stride_inner*0 ];
			verts[v].position.y = pos[(i*w+j) *stride_outer + stride_inner*1 ];
			verts[v].position.z = pos[(i*w+j) *stride_outer + stride_inner*2 ];
			v+= (j==w-1)? 1 : gap;
		}
		if(i!=h-1) v+= (gap-1)*rowsize;
	}
	assert(v == verts.count/2);
	v=0;
	for(j=0;j<w;j++)
	{
		KobbeltUpdateLin(&verts[v],rowsize*gap,s,h);
		v+=gap;
	}
	v=0;
	while(v<rowsize*rowcount)
	{
		KobbeltUpdateLin(&verts[v],gap,s,w);
		v+=rowsize;
	}
	for(i=0;i<rowcount;i++)
	{
		for(j=0;j<rowsize;j++)
		{
			int v = j+i*rowsize;
			int vw= (j==0)? v:v-1;			
			int ve= (j==rowsize-1)? v:v+1;
			int vn= (i==0)? v:v-rowsize;
			int vs= (i==rowcount-1)?v:v+rowsize;
			float3 binormal= normalize(verts[vn].position-verts[vs].position);
			float3 normal  = normalize(cross(verts[ve].position-verts[vw].position,verts[vn].position-verts[vs].position));
			float3 tangent = cross(binormal,normal);  //normalize(verts[ve].position-verts[vw].position);
			verts[v].orientation = normalize(quatfrommat<float>(float3x3(tangent,binormal,normal)));
		}
	}
	assert(v==verts.count/2);
	Vertex *vertsback = verts.element + rowcount*rowsize;
	for(i=0;i<rowcount*rowsize;i++)
	{
		vertsback[i].position   = verts[i].position - ((Quaternion&)verts[i].orientation).zdir()*kobbelt_thickness;
		vertsback[i].orientation= (Quaternion&)verts[i].orientation*Quaternion(0,1,0,0);
	}
}

void GridDraw(Vertex *verts,int w,int h,const float3 &color)
{
	int i,j;
	for(i=0;i<h;i++)
	 for(j=0;j<w;j++)
	{
		if(i) Line(verts[i*w+j].position,verts[(i-1)*w+j].position,color);
		if(j) Line(verts[i*w+j].position,verts[i*w+(j-1)].position,color);
	}
}



void Kobbelt::Update()
{
	if( (((h-1)<<subdiv)+1) * (((w-1)<<subdiv)+1) != mesh->vertices.count)
	{
		BuildMesh(); // someone must have modified the subdivision count
	}
	KobbeltUpdate(pos,stride_inner,stride_outer,w,h,subdiv,mesh->vertices);
	if(pos3)
	{
		BoxLimits(pos3,w*h,model->bmin,model->bmax);
//		KobbeltUpdate(pos3,w,h,subdiv,mesh->vertices);
	}
	else if(pos4)
	{
		BoxLimits(pos4,w*h,model->bmin,model->bmax);
//		KobbeltUpdate(pos4,w,h,subdiv,mesh->vertices);
	}
	else
		BoxLimits(this->mesh->vertices.element,this->mesh->vertices.count, model->bmin,model->bmax);
}

Kobbelt::Kobbelt(const float4 *_pos,int _w,int _h,int _s):pos4(_pos),pos3(NULL),pos((float*)_pos),w(_w),h(_h),subdiv(_s),wireframe(0),texscale(1.0f),
                                                          model(new Model),mesh(new DataMesh()),shadow(new DataMesh()),matid(mesh->matid)
{
	stride_inner=1;
	stride_outer=4;

	model->datameshes.Add(mesh);
	mesh->matid = MaterialFind("tissue"); // shouldnt be here but oh well.
	model->datameshes.Add(shadow);
	shadow->matid = MaterialFind("extendvolume");
	shadow->model = mesh->model = model;
	BuildMesh();
}

Kobbelt::Kobbelt(const float3 *_pos,int _w,int _h,int _s):pos3(_pos),pos4(NULL),pos((float*)_pos),w(_w),h(_h),subdiv(_s),wireframe(0),texscale(1.0f),
                                                          model(new Model),mesh(new DataMesh()),shadow(new DataMesh()),matid(mesh->matid)
{
	stride_inner=1;
	stride_outer=3;
	model->datameshes.Add(mesh);
	mesh->matid = MaterialFind("tissue"); // shouldnt be here but oh well.
	model->datameshes.Add(shadow);
	shadow->matid = MaterialFind("extendvolume");
	shadow->model = mesh->model = model;
	BuildMesh();
}

Kobbelt::Kobbelt(const float  *_pos,int _stride_inner, int _stride_outer,int _w,int _h,int _s):pos3(NULL),pos4(NULL),pos(_pos),w(_w),h(_h),subdiv(_s),wireframe(0),texscale(1.0f),
                                                          model(new Model),mesh(new DataMesh()),shadow(new DataMesh()),matid(mesh->matid)
{
	stride_inner=_stride_inner;
	stride_outer=_stride_outer;
	model->datameshes.Add(mesh);
	mesh->matid = MaterialFind("tissue"); // shouldnt be here but oh well.
	model->datameshes.Add(shadow);
	shadow->matid = MaterialFind("extendvolume");
	shadow->model = mesh->model = model;
	BuildMesh();
}



void Kobbelt::BuildMesh()
{
	int i,j;
	int collen=((h-1)<<subdiv)+1;  // length of col or number of rows of the lattice
	int rowlen=((w-1)<<subdiv)+1;  // length of row or number of colums of the lattice
	assert(kob_verts_required(w,h,subdiv)==collen*rowlen);
	mesh->vertices.count=0;
	mesh->vertices.SetSize(collen*rowlen * 2);
	int b = collen*rowlen;  // index offset for verts on backside of kobbelt.
	for(i=0;i<collen;i++)
		for(j=0;j<rowlen;j++)
	{
		mesh->vertices[i*rowlen+j  ].texcoord = float2((float)j/(rowlen-1.0f)*texscale,1.0f-(float)i/(collen-1.0f)*texscale);
		mesh->vertices[i*rowlen+j+b].texcoord = float2(1.0f-(float)j/(rowlen-1.0f)*texscale,1.0f-(float)i/(collen-1.0f)*texscale);
	}
	short tr=((w-1)) << subdiv;
	short tc=((h-1)) << subdiv;
	mesh->tris.count=0;
	mesh->tris.SetSize(tr*tc*2 *2 + (tr+tc)*4); // front, back and sides
	mesh->tris.count=0;
	for(i=0;i<tc;i++)
		for(j=0;j<tr;j++)
	{
		short v0=i*(tr+1) + j;
		short v2=(i+1)*(tr+1) +j;
		mesh->tris.Add(short3(v0,v2  ,v2+1));
		mesh->tris.Add(short3(v0,v2+1,v0+1));
		mesh->tris.Add(short3(v0,v2+1,v2  )+short3(b,b,b));
		mesh->tris.Add(short3(v0,v0+1,v2+1)+short3(b,b,b));
	}
	for(i=0;i<tr;i++)  // do the seams
	{
		mesh->tris.Add(short3( i+0  , i+1   , i+1+b ));
		mesh->tris.Add(short3( i+0  , i+1+b , i+0+b ));
		mesh->tris.Add(short3( i+0+b, i+1+b , i+1   )+short3(1,1,1)*(short)((tr+1)*tc));  // last row
		mesh->tris.Add(short3( i+0+b, i+1   , i+0   )+short3(1,1,1)*(short)((tr+1)*tc));  
	}
	for(i=0;i<tc;i++)  // first and last columns
	{
		short v0=i*(tr+1);
		short v2=(i+1)*(tr+1);
		mesh->tris.Add(short3( v0   , v0+b  , v2 ));  // first column
		mesh->tris.Add(short3(  v2 , v0+b  ,v2+b ));  
		mesh->tris.Add(short3( v0  , v2   , v0+b )+short3(tr,tr,tr));  // last column
		mesh->tris.Add(short3( v0+b  , v2 , v2+b )+short3(tr,tr,tr));
	}
	shadow->vertices.element = mesh->vertices.element;
	shadow->vertices.count   = mesh->vertices.count;
	shadow->vertices.array_size=0;
	shadow->tris.element = mesh->tris.element;
	shadow->tris.count   = mesh->tris.count;
	shadow->tris.array_size=0;
}

Kobbelt *KobbeltCreate(const float4 *_pos,int _w,int _h,int _s)
{
	Kobbelt *kob = new Kobbelt(_pos,_w,_h,_s);
	return kob;
}


int kobbeltwire=0; // global for all kobbelt meshes
EXPORTVAR(kobbeltwire);

void Kobbelt::Draw()
{
	if( (((h-1)<<subdiv)+1) * (((w-1)<<subdiv)+1) != mesh->vertices.count)
	{
	// note, i'm generating  backfaces for shadow vols  	Update(); // someone must have modified the subdivision count
	}
	if(kobbeltwire||wireframe)
		GridDraw(mesh->vertices.element,kob_verts_required(w,1,subdiv),kob_verts_required(1,h,subdiv),color);
	else
		ModelRender(model);
}

