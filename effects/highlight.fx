//   highlight effect.
//



#include "common.fxh"  // for constant data: meshq meshp ViewProj   and function: qrotate() 

float3 highlight={0.2f,0.2f,0.2f};

float4 vertex_shader_positionoffset(const float4 position : POSITION): POSITION
{
	float4 p = mul(float4(meshp + qrotate(meshq,position),1),ViewProj); // local to world and then projected position in clip space
	p.z = p.z - 0.0001;
	return p;
}

float4 pixel_shader_highlight():COLOR0
{
	float4 c;
	c.xyz = highlight;
	c.w = 1.0;
	return c;
}

technique highlight_technique
{
	pass p0
	{
		DEPTHBIAS = 0;
		SLOPESCALEDEPTHBIAS = 0;
		ZENABLE = TRUE;
		ZWRITEENABLE = FALSE;
		CullMode = CW;
		PixelShader  = compile ps_2_0 pixel_shader_highlight();
		VertexShader = compile vs_2_0 vertex_shader_positionoffset();
        ALPHABLENDENABLE = TRUE ;
        SRCBLEND  = ONE ;
        DESTBLEND = ONE ;
		STENCILENABLE=    FALSE  ;

	}
}

