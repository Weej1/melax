//
//  Volume extrusion shader and stencil shadow technique
//  aka stencil shadows
//  Shader assumes quaternion representation for vertex orientation.
//
// For the current light this fx computes which pixels are in shadow and which aren't
// 
// This technique is not as commonly used in games since the performance  
// can be signficantly impacted by silhouette complexity (lots of overdraw).
// Also, the input geometry has to work with the system.  Triangle soup isn't so good.
//
// For some applications, stencil shadows are still useful since its just so easy,
// pixel perfect, and no tweaking hassles to get it right.  
//
// I do things the easy way and just assume manifold and c1 (parametric) continuity.
// This does not require meshes to g1 (geometric) continuity. 
// Simply put zero area polys along creases.
// 
//
//
//

#include "common.fxh"

float3 lightcolor={0.2,1.0,0.2};
float3 lightposn={0.0f , 0.0f, 2.0f };
float  lightradius = 20.0f;



float4 vertex_shader_extend(const float3 p,const float3 n):POSITION
{
	float3 l = normalize(lightposn-p);  // inputs are all in 'world' space
	float d = dot(l,n);
	float extend;
	if(d<0) 
	{
		extend = lightradius*5;  // vertex faces away from the light
	}
	else
	{
		extend = 0.01; 
	}
	return mul(float4(p-(l*extend),1),ViewProj);
}

float4 vertex_shader_extend_noskin(const Vertex v_in):POSITION
{
	float3 p = meshp + qrotate(meshq,v_in.position); 
	float3 n = qrotate(qmul(meshq,v_in.orientation),float3(0,0,1));
	return vertex_shader_extend(p,n);
}


float4 vertex_shader_extend_skin(const VertexS v_in):POSITION
{
	pose blended = skin_blendedpose(v_in.bones,v_in.weights);
	float3 p = blended.position + qrotate(blended.orientation,v_in.position);
	float3 n = qrotate(qmul(blended.orientation,v_in.orientation),float3(0,0,1));
	return vertex_shader_extend(p,n);
}


float4 vertex_shader_extend_dualquat(const VertexS v_in):POSITION
{
	pose blended = skin_dualquat(v_in.bones,v_in.weights);  // or use:  skin_blendedpose(IN.bones,IN.weights);
	float3 p = blended.position + qrotate(blended.orientation,v_in.position);
	float3 n = qrotate(qmul(blended.orientation,v_in.orientation),float3(0,0,1));
	return vertex_shader_extend(p,n);
}

float4 vertex_shader_extend_crapskin(const VertexS v_in):POSITION
{
	int4 bones = v_in.bones;
	float4 weights = v_in.weights;
	float3 p = 
		(qrotate(currentposeq[bones[0]],v_in.position) + currentposep[bones[0]]) * weights[0] +
		(qrotate(currentposeq[bones[1]],v_in.position) + currentposep[bones[1]]) * weights[1] +
		(qrotate(currentposeq[bones[2]],v_in.position) + currentposep[bones[2]]) * weights[2] +
		(qrotate(currentposeq[bones[3]],v_in.position) + currentposep[bones[3]]) * weights[3]  ;
	pose blended = skin_dualquat(v_in.bones,v_in.weights);  // or use:  skin_blendedpose(IN.bones,IN.weights);
	float3 n = qrotate(qmul(blended.orientation,v_in.orientation),float3(0,0,1));
	return vertex_shader_extend(p,n);
}

VertexShader vsArrayExtend[4] = { 
	compile vs_3_0 vertex_shader_extend_noskin(), 
	compile vs_3_0 vertex_shader_extend_skin(),
	compile vs_3_0 vertex_shader_extend_dualquat(),
	compile vs_3_0 vertex_shader_extend_crapskin(),
};


float4 pixel_shader():COLOR0
{
	float4 color;
	color.xyz = lightcolor ;
	color.w=1.0;
	return color;
}

// see sample `insert reference here` for description on how to
// set up the d3d state to do stencil increments and decrements in a single pass version 
// 
technique t0
{
	pass p0
	{
		ZENABLE = TRUE;
		ZFunc = GREATER;
		CullMode = NONE;
		FILLMODE=SOLID;
		ZWRITEENABLE= FALSE ;  // we use the depth (z values) but dont want to write into it.
		STENCILENABLE=TRUE ;

		SHADEMODE=   FLAT ;  // no color interpolation required
		DEPTHBIAS = 0;  // 
		SLOPESCALEDEPTHBIAS	= 0;	
		STENCILFUNC= ALWAYS ;
		STENCILZFAIL=KEEP ;
		STENCILFAIL= KEEP ;

		// If ztest passes, inc/decrement stencil buffer value
		STENCILREF=      0x1 ;
		STENCILMASK=     0xffffffff ;
		STENCILWRITEMASK=0xffffffff ;
		STENCILPASS=     INCR ;

		ALPHABLENDENABLE=TRUE ;  // these lines suggested to make sure no writing to colorbuffer
		SRCBLEND= ZERO ;
		DESTBLEND=ONE ;

        TWOSIDEDSTENCILMODE= TRUE ;
        CCW_STENCILFUNC=  ALWAYS ;
        CCW_STENCILZFAIL= KEEP ;
        CCW_STENCILFAIL=  KEEP ;
        CCW_STENCILPASS=  DECR;
 
		PixelShader  = compile ps_2_0 pixel_shader();
		VertexShader = vsArrayExtend[useskin]; // compile vs_2_0 vertex_shader();
	}
}


technique debug
{
	pass p0
	{
		ZENABLE = TRUE;
		CullMode = NONE;
		FILLMODE=WIREFRAME;
		PixelShader  = compile ps_2_0 pixel_shader();
		VertexShader = vsArrayExtend[useskin]; 
	}
}
