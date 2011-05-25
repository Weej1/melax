//   alpha effect.
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
float3 diffuse = {0.6,0.6,0.6};
float3 ambient={0.5,0.5,0.5};
float  alpha=0.3;

texture diffusemap : DiffuseMap;

sampler samp = sampler_state
{
	texture = <diffusemap>;
	AddressU = WRAP;
	AddressV = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
};


float4 pixel_shader(const Fragment IN) : COLOR
{
	return float4(tex2Drh(samp, IN.texcoord).xyz,alpha);
}

technique t0
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
		PixelShader  = compile ps_2_0 pixel_shader();
		VertexShader = vsArray[useskin];
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
		PixelShader  = compile ps_2_0 pixel_shader();
		VertexShader = vsArray[useskin];
	}
}

