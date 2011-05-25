//
//
//   parallax mapping
//
//  to give credit where credit is due...
//  the technique and some of the terms and coefficients 
//  that are used here are Originally from relief_mapping .fx File by Fabio Policarpo.
//  I believe his files are available on his site or on the nvidia developer site.
//
//  I've only implemented the parallax technique, I have yet to implement the other pixel shaders for shadow relief mapping.
//  
//  Everything here is quat based.  Camera, mesh, and bone poses are position,quat pairs.
//  Input vertices include an orientation instead of the usual tangent space basis.
//  i.e. no normal, binormal, tangent.
//  
//  Lighting equations are all done in world space.  imho quite easy to follow
//
//  The setup assumes there is an ambient pass followed by n light passes for n point lights.
//  the technique pass assumes stencil, but could easily be changed to shadowbuffers.
//  
//  Skinning and non-skinning versions.
//
//
//
//
// Notice the att term used in the following line of code:
//     float att=saturate(dot(l,IN.normal.xyz));
//
//  This additional coeficient in the lighting equations isn't commonly used.
//  It does greatly reduce specular reflecting at low angles.  Imagine a light at the other end of a long hallway.
//  In fact, according to snell's law and Fresnel's equations, you would get more reflection in this case.
//  More importantly, for deferred shading/lighing, including this term means that 
//  you also have to store the original geometry (unpreturbed) normal.  
//  Fabio's other examples dont use this att term.  
//  Actually they have an att term too but its used for typical attenuation.
//  The benefit from having this term is that it does help with self occlusion.  

#include <common.fxh>

float  alpha; 
float3 ambient  = {0.2,0.2,0.2};
float3 lightcolor = {0.5,0.5,0.5} ;
float  gloss = 0.5f;
float  phongexp  = 64.0;  // phong's specular power

float3 lightposn  = { 0,0,5.0 };
float  lightradius;
float  lightradiusinv;



//--------

texture diffusemap : DIFFUSE ;

texture normalmap : NORMAL;

sampler2D diffusesamp = sampler_state {
	Texture = <diffusemap>;
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = WRAP;
	AddressV = WRAP;
};

sampler2D normalsamp = sampler_state {
	Texture = <normalmap>;
	MinFilter = Linear;
	MagFilter = Linear;
	MipFilter = Linear;
	AddressU = WRAP;
	AddressV = WRAP;
};


//--------------------------------------


float4 pixel_shader_unlit( Fragment IN ) : COLOR
{
	float height = tex2Drh(normalsamp,IN.texcoord).w * 0.06 - 0.03;
	float3x3 m = float3x3(normalize(IN.tangent),normalize(IN.binormal),normalize(IN.normal));
	float2 offset = height * mul(m,normalize(camerap-IN.position)) ; // rotate eye vector into tangent space
	return float4(ambient,1) * tex2Drh(diffusesamp,IN.texcoord+offset);   // diffuse texture lookup using offset
}




float4 pixel_shader_light(Fragment IN) : COLOR
{
	float3x3 m = float3x3(normalize(IN.tangent),normalize(IN.binormal),normalize(IN.normal));
   	// view and light directions
	float3 lightv = normalize(lightposn-IN.position);
	float  lightdistance = length(lightposn-IN.position);
	float falloff = 1 -lightdistance*lightradiusinv;
	clip(falloff);
	falloff=saturate(falloff);

	float  att= (8* (mul(m,lightv).z));   // check z value of lightvector rotated into tangent space
	clip(att);          // self occlusion due to light behind poly
	att=saturate(att);  // this term suggested by fabio which is also where the coefficient '8' came from

	float height = tex2Drh(normalsamp,IN.texcoord).w * 0.06 - 0.03;   // displacement from surface  Fabio's coefficients
	float2 offset = height * mul(m,normalize(camerap-IN.position)) ; 	// compute parallax offset 
	
	float4 color =tex2Drh(diffusesamp,IN.texcoord + offset);   // diffuse color
	float3 normal=mul(((tex2Drh(normalsamp ,IN.texcoord + offset)-0.5)*2.0).xyz,m); // transform preturbed normal to world space
	float diffuse  = saturate(dot(lightv,normal));  // classic N dot L
	float specular = saturate(dot(normalize(lightv + normalize(camerap-IN.position)),normal));   // N dot H the "half-angle" vector

	return float4( falloff*att*(color.xyz*lightcolor.xyz*diffuse+lightcolor*(gloss*pow(specular,phongexp))) , 1.0); 
}


//--------------------------------------

technique unlit 
{
	pass p0
	{
		DEPTHBIAS = 0;
		SLOPESCALEDEPTHBIAS	= 0;	
		TWOSIDEDSTENCILMODE = FALSE;
		ZENABLE = TRUE;
		ZFUNC = LESSEQUAL ;
		ZWRITEENABLE =     TRUE ;
		CullMode = CW;
		STENCILENABLE=    FALSE  ;
		ALPHABLENDENABLE= FALSE ;
		SHADEMODE = GOURAUD ;
		PixelShader  = compile ps_3_0 pixel_shader_unlit();
		VertexShader = vsArray[useskin]; //	VertexShader = compile vs_3_0 vertex_shader();
	}
}

technique lightpass  // assumes stencil shadow
{
	pass p0
	{
		DepthBias = -0.00001f;  // nvidia 6200go and ati x800 doesn't need this , but other ati cards may need it if clipped polys aren't coplanar 
		//DEPTHBIAS = 0;
		//SLOPESCALEDEPTHBIAS	= 0;	
		STENCILENABLE=    TRUE  ;
		TWOSIDEDSTENCILMODE = FALSE;
		CCW_STENCILPASS=KEEP;
		STENCILPASS=     KEEP;
		STENCILFAIL=     KEEP;
		STENCILZFAIL=     KEEP;
		ZWRITEENABLE=     FALSE ;
		ZENABLE = TRUE;
		ZFUNC = LESSEQUAL ;
		STENCILREF=  0x01 ;
		STENCILFUNC= GREATER; // since unlike shadowpoly we want to draw where shadow isn't  // LESSEQUAL ;
		FOGENABLE=        FALSE ;
		ALPHABLENDENABLE= TRUE  ;
		SHADEMODE = GOURAUD ;
		SRCBLEND=  ONE ;
		DESTBLEND= ONE ;
		CullMode = CW;
		FILLMODE=SOLID;
		PixelShader  = compile ps_3_0 pixel_shader_light();
		VertexShader = vsArray[useskin];
	}
}


//-------------------------------------------
// alpha used for highlighting stuff mostly:

struct Fragmentalpha
{
	float4 position : POSITION;
	float2 texcoord : TEXCOORD;
};

Fragmentalpha vertex_shader_alpha(const Vertex IN)
{
	Fragmentalpha OUT;
	OUT.position  = mul(float4(meshp+qrotate(meshq,IN.position),1.0) , ViewProj);
	OUT.texcoord = IN.texcoord;
	return OUT;
}

float4 pixel_shader_alpha(const Fragmentalpha IN):COLOR
{
	float4 color = tex2Drh(diffusesamp, IN.texcoord);
	color.w = alpha;
	return color;
}

technique alpha_technique
{
	pass p0
	{
		DEPTHBIAS = 0;
		TWOSIDEDSTENCILMODE = FALSE;
		ZENABLE = TRUE;
		CullMode = CW;
		STENCILENABLE=    FALSE  ;
		ALPHABLENDENABLE= TRUE ;
		SHADEMODE = GOURAUD ;
		ZWRITEENABLE =     FALSE ;
        SRCBLEND  = SRCALPHA ;
        DESTBLEND = INVSRCALPHA ;
		PixelShader  = compile ps_3_0 pixel_shader_alpha();
		VertexShader = compile vs_3_0 vertex_shader_alpha();
	}
	pass p1
	{
		DEPTHBIAS = 0;
		TWOSIDEDSTENCILMODE = FALSE;
		ZENABLE = TRUE;
		CullMode = CCW;
		STENCILENABLE=    FALSE  ;
		ALPHABLENDENABLE= TRUE ;
		SHADEMODE = GOURAUD ;
		ZWRITEENABLE =     FALSE ;
        SRCBLEND  = SRCALPHA ;
        DESTBLEND = INVSRCALPHA ;
		PixelShader  = compile ps_3_0 pixel_shader_alpha();
		VertexShader = compile vs_3_0 vertex_shader_alpha();
	}
}



