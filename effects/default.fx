//   default effect.
//
//  This is where the shaders are (vertex shader and pixel shader)
//  Edit this file so we use the texture when rendering.
// 
//
//  Programming Tip:
//  When using the function D3DXCreateEffectFromFile() to load
//  an effect, you are compiling the shaders on the fly and you
//  dont seem to get as good error messages, or there not in a convenient spot.
//  A useful thing to do is to add shader compilation to the build/compile process.
//  to do this, add your .fx file to your visual studio project.  
//  Since its not a .cpp file, msdev doesn't recognize how to compile it.
//  Right-click on the file and select "settings".
//  Under "Custom Build" add the following commands:
//
//       set FXC="C:\DXSDK\Bin\DXUtils\FXC.EXE"
//       echo compiling vertex_shader
//       set EFFECT=$(InputName)
//       set command=%FXC% %EFFECT%.fx /T vs_2_0 /E vertex_shader /Fc DEBUG\%EFFECT%_vs.txt
//       echo %command%
//       %command%
//       echo Compiling pixel_shader
//       set command=%FXC% %EFFECT%.fx /T ps_2_0 /E pixel_shader /Fc DEBUG\%EFFECT%_ps.txt
//       echo %command%
//       %command%
//
//  I did it this way so it doesn't depend on the name of the effect file.
//  but you will have to change the entry points within this if you didn't call
//  your effects file's functions "vertex_shader" and "pixel_shader".
// 
//  Still under "Custom Build" add
//       DEBUG\$(InputName)_vs.txt
//       DEBUG\$(InputName)_ps.txt
//  to the outputs.
//  Now after you edit your effect .fx file (shaders), you will test compile them
//  the next time you build your application.  Any warning or errors will show
//  up in the build output subwindow, and you can simply double click on  
//  them (or hit function key F4) to go directly to your error.
//


#include "common.fxh"


float3 lightposn={0.4,0.4,0.8};  // in world space
float  lightradius;
float3 lightcolor = {0.6,0.6,0.6};
float3 ambient={0.5,0.5,0.5};
float  alpha=0.3;
float  diffusecoef = 1.0;
float3 emissive={0.0,0.0,0.0};
float  gloss = 0.5;   // affects how much specular is reflected.




texture diffusemap : DiffuseMap;
sampler diffusesamp = sampler_state
{
	texture = <diffusemap>;
	AddressU = WRAP;
	AddressV = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
};


float4 pixel_shader_light(const Fragment IN) :COLOR
{
	float  a = saturate(1-length(lightposn-IN.position)/lightradius);  // falloff attenuation
	float3 E = normalize(camerap-IN.position);    // eye direction
	float3 L = normalize(lightposn-IN.position);  // light direction
	float3 N = normalize(IN.normal); //  surface normal
	float3 H = normalize(E+L);                    // half angle vector
	float  s = pow(saturate(dot(N,H)),64);        // specular intensity
	float  d = saturate(abs(dot(N,L)));  // abs makes this thing diffuse lightable from backside!!!!!!!!!!!!!!
	float4 c;                                     // output color
	c.xyz = ((diffusecoef * lightcolor * d)  * tex2Drh(diffusesamp, IN.texcoord) + lightcolor * gloss * s) * a;
	c.w = 1.0;
	return c;
}

float4 pixel_shader_unlit(const Fragment IN) :COLOR
{
	return  float4( saturate(emissive+ambient) * tex2Drh(diffusesamp, IN.texcoord) , 1.0);
}


technique unlit 
{
	pass p0
	{
		DEPTHBIAS = 0;
		TWOSIDEDSTENCILMODE = FALSE;
		ZENABLE = TRUE;
		ZFUNC = LESSEQUAL ;
		ZWRITEENABLE =     TRUE ;
		CullMode = CW;
		STENCILENABLE=    FALSE  ;
		ALPHABLENDENABLE= FALSE ;
		SHADEMODE = GOURAUD ;
		PixelShader  = compile ps_2_0 pixel_shader_unlit();
		VertexShader = vsArray[useskin];//compile vs_2_0 vertex_shader();
	}
}

technique lightpass
{
	pass p0
	{
		DepthBias = -0.00001f;  // nvidia 6200go and ati x800 doesn't need this , but other ati cards may need it if clipped polys aren't coplanar 
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
		CullMode =  CW;
		FILLMODE=SOLID;
		PixelShader  = compile ps_2_0 pixel_shader_light();
		VertexShader = vsArray[useskin];// compile vs_2_0 vertex_shader();
	}
}



Fragment vertex_shader_alpha(const Vertex IN)
{
	Fragment OUT = (Fragment)0;
	float4 p = mul(qrotate(meshq,IN.position)+meshp, ViewProj);
	OUT.screenpos = p;
	OUT.texcoord.x = IN.texcoord.x;
	OUT.texcoord.y = 1.0-IN.texcoord.y; 
	return OUT;
}

float4 pixel_shader_alpha(const Fragment IN) :COLOR
{
	return float4(1,1,1,alpha) * tex2Drh(diffusesamp, IN.texcoord);
}

technique alpha_technique  // used mostly for highlighting stuff
{
	pass p0
	{
		DEPTHBIAS = 0;
		TWOSIDEDSTENCILMODE = FALSE;
		ZENABLE = TRUE;
		ZFUNC   = LESSEQUAL ;
		CullMode = CW;
		STENCILENABLE=    FALSE  ;
		ALPHABLENDENABLE= TRUE ;
		SHADEMODE = GOURAUD ;
		ZWRITEENABLE =     FALSE ;
        SRCBLEND  = SRCALPHA ;
        DESTBLEND = INVSRCALPHA ;
		PixelShader  = compile ps_2_0 pixel_shader_alpha();
		VertexShader = compile vs_2_0 vertex_shader_alpha();
	}
	pass p1
	{
		DEPTHBIAS = 0;
		TWOSIDEDSTENCILMODE = FALSE;
		ZENABLE = TRUE;
		ZFUNC   = LESSEQUAL ;
		CullMode = CCW;
		STENCILENABLE=    FALSE  ;
		ALPHABLENDENABLE= TRUE ;
		SHADEMODE = GOURAUD ;
		ZWRITEENABLE =     FALSE ;
        SRCBLEND  = SRCALPHA ;
        DESTBLEND = INVSRCALPHA ;
		PixelShader  = compile ps_2_0 pixel_shader_alpha();
		VertexShader = compile vs_2_0 vertex_shader_alpha();
	}
}

