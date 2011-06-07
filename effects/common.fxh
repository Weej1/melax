//   common.fxh
//
//  Instead of coping and pasting, these things are used in a number of .fx files 
//
//  not exactly like a normal header file since everything must be included in link-less the fx world.
//
//  Only putting in here the things that I know everybody wants.  i.e. those funcs and decls that are otherwise subject to copy-paste 
//
//  see default.fx 
// 


float4   meshq;   // orientation of the model/mesh 
float3   meshp;   // position    of the model/mesh
float4   cameraq; // orientation of the camera
float3   camerap; // position    of the camera

static const int MAX_BONES = 26;
float3 currentposep[MAX_BONES];  // for skinned meshes my bones are in world space - they usually correspond to physics rigid bodies
float4 currentposeq[MAX_BONES];
int useskin=0;
float4x4 ViewProj : ViewProjection;  

float4 tex2Drh(sampler2D s,float2 t) // right handed version
{ 
	return tex2D(s,float2(t.x,1.0-t.y));   // Prefer origin in bottom left - consistent with 3dsmax and with OpenGL
}


struct pose 
{
	float3 position;  
	float4 orientation;
};

struct Vertex 
{
    float3 position    : POSITION;
    float4 orientation : TEXCOORD1;
    float2 texcoord	   : TEXCOORD0;
};


struct VertexS 
{
    float3 position    : POSITION;
    float4 orientation : TEXCOORD1;
	float2 texcoord    : TEXCOORD0;
    float4 weights     : BLENDWEIGHT;
    int4   bones       : BLENDINDICES;
};

struct Fragment
{
    float4 screenpos   : POSITION;
    float2 texcoord    : TEXCOORD0;
	float3 position    : TEXCOORD6;
	float4 orientation : TEXCOORD7; 
	float3 tangent     : TEXCOORD3; 
	float3 binormal    : TEXCOORD4; 
	float3 normal      : TEXCOORD5; 
};


float4 qconj(float4 q)
{
	return float4(-q.x,-q.y,-q.z,q.w);
}

float4 qmul(float4 a, float4 b)
{
	float4 c;
	c.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z; 
	c.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y; 
	c.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x; 
	c.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w; 
	return c;
}


float3 qrotate(  float4 q,  float3 v )
{
// return qmul(qmul(q,float4(v,0)),q*float4(-1,-1,-1,1)).xyz; 
 return qmul(qmul(q,float4(v,0)), float4(-q.x,-q.y,-q.z,q.w)).xyz; 
 
 /*
 	float qx2 = q.x*q.x;
	float qy2 = q.y*q.y;
	float qz2 = q.z*q.z;

	float qxqy = q.x*q.y;
	float qxqz = q.x*q.z;
	float qxqw = q.x*q.w;
	float qyqz = q.y*q.z;
	float qyqw = q.y*q.w;
	float qzqw = q.z*q.w;
	return float3(
		(1-2*(qy2+qz2))*v.x + (2*(qxqy-qzqw))*v.y + (2*(qxqz+qyqw))*v.z ,
		(2*(qxqy+qzqw))*v.x + (1-2*(qx2+qz2))*v.y + (2*(qyqz-qxqw))*v.z ,
		(2*(qxqz-qyqw))*v.x + (2*(qyqz+qxqw))*v.y + (1-2*(qx2+qy2))*v.z  );
*/
}

float dp4(float4 a,float4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;}
float4 neg4(float4 a) {return float4(-a.x,-a.y,-a.z,-a.w); }

pose skin_blendedpose(int4 bones, float4 weights)  // nlerp orientations and lerp positions independantly (linear translation)
{
	float4 b0=currentposeq[bones[0]];
	float4 b1=currentposeq[bones[1]];
	float4 b2=currentposeq[bones[2]];
	float4 b3=currentposeq[bones[3]];
	if(dp4(b0,b1)<0) neg4(b1);
	if(dp4(b0+b1,b2)<0) neg4(b2);
	if(dp4(b0+b1+b2,b3)<0) neg4(b3);
	pose p;
	p.orientation = normalize(
		b0*weights[0] +
		b1*weights[1] +
		b2*weights[2] +
		b3*weights[3]  );
	p.position =   
		   currentposep[bones[0]] * weights[0] +
		   currentposep[bones[1]] * weights[1] +
		   currentposep[bones[2]] * weights[2] +
		   currentposep[bones[3]] * weights[3]  ;
	return p;
}

pose skin_dualquat(int4 bones, float4 weights)  // dual quat skinning (screw motion)
{
	pose p;
	float4 b0=currentposeq[bones[0]];
	float4 b1=currentposeq[bones[1]];
	float4 b2=currentposeq[bones[2]];
	float4 b3=currentposeq[bones[3]];
	if(dp4(b0,b1)<0) b1=neg4(b1);
	if(dp4(b0+b1,b2)<0) b2=neg4(b2);
	if(dp4(b0+b1+b2,b3)<0) b3=neg4(b3);
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
		).xyz /dp4(q,q); // this works since q hasn't been normalized yet
	p.orientation = normalize(q);
	return p;
}



Fragment vertex_shader(const Vertex IN)
{
	Fragment OUT;
	OUT.texcoord    = IN.texcoord;
	float4 q=qmul(meshq,IN.orientation) ; 
	OUT.orientation = q; 
	OUT.tangent  = qrotate(q,float3(1,0,0));
	OUT.binormal = qrotate(q,float3(0,1,0));
	OUT.normal   = qrotate(q,float3(0,0,1));
	float3 p = meshp + qrotate(meshq,IN.position); 
	OUT.position  = p ;   
	OUT.screenpos = mul(float4(p,1),ViewProj); // projected position in clip space
	return OUT;
}


Fragment vertex_shader_skin(const VertexS IN)
{
	pose blended = skin_blendedpose(IN.bones,IN.weights);
	float3 p = blended.position + qrotate(blended.orientation,IN.position);
	Fragment OUT;
	OUT.position = p;  
	OUT.screenpos = mul(float4(p,1),ViewProj);  // vertex position projected in clip space
	float4 q  = qmul(blended.orientation,IN.orientation); 
	OUT.orientation = q; 
	OUT.tangent  = qrotate(q,float3(1,0,0));
	OUT.binormal = qrotate(q,float3(0,1,0));
	OUT.normal   = qrotate(q,float3(0,0,1));
	OUT.texcoord    = IN.texcoord;
	return OUT;
}


Fragment vertex_shader_dualquat(const VertexS IN)
{
	pose blended = skin_dualquat(IN.bones,IN.weights);  
	float3 p = blended.position + qrotate(blended.orientation,IN.position);
	Fragment OUT;
	OUT.position = p;  
	OUT.screenpos = mul(float4(p,1),ViewProj);  // vertex position projected in clip space
	float4 q = qmul(blended.orientation,IN.orientation); 
	OUT.orientation = q; 
	OUT.tangent  = qrotate(q,float3(1,0,0));
	OUT.binormal = qrotate(q,float3(0,1,0));
	OUT.normal   = qrotate(q,float3(0,0,1));
	OUT.texcoord    = IN.texcoord;
	return OUT;
}


Fragment vertex_shader_crapskin(const VertexS IN)  // just used for some comparison testing
{
	int4 bones = IN.bones;
	float4 weights = IN.weights;
	float3 in_position = IN.position;
	float3 p = 
		(qrotate(currentposeq[bones[0]],in_position) + currentposep[bones[0]]) * weights[0] +
		(qrotate(currentposeq[bones[1]],in_position) + currentposep[bones[1]]) * weights[1] +
		(qrotate(currentposeq[bones[2]],in_position) + currentposep[bones[2]]) * weights[2] +
		(qrotate(currentposeq[bones[3]],in_position) + currentposep[bones[3]]) * weights[3]  ;
	pose blended = skin_dualquat(IN.bones,IN.weights);  // still need to rotate orientation
	Fragment OUT;
	OUT.position = p;  
	OUT.screenpos = mul(float4(p,1),ViewProj);  // vertex position projected in clip space
	float4 q = qmul(blended.orientation, IN.orientation); 
	OUT.orientation = q; 
	OUT.tangent  = qrotate(q,float3(1,0,0));
	OUT.binormal = qrotate(q,float3(0,1,0));
	OUT.normal   = qrotate(q,float3(0,0,1));
	OUT.texcoord    = IN.texcoord;
	return OUT;
}


VertexShader vsArray[4] = { compile vs_3_0 vertex_shader(), 
                            compile vs_3_0 vertex_shader_skin(),
                            compile vs_3_0 vertex_shader_dualquat(),
                            compile vs_3_0 vertex_shader_crapskin(),
};


