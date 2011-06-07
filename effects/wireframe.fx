



#include "common.fxh"  // for constant data: meshq meshp ViewProj   and function: qrotate() 

float3 wirecolor={0.2,1.0,0.2};


float4 vertex_shader_positionoffsetn(const float4 position:POSITION , const float4 normal :NORMAL):POSITION
{
	float4 p = mul(float4(meshp + qrotate(meshq,position),1),ViewProj)  + normal*0.01;
	return p;
}


float4 pixel_shader_wirecolor():COLOR0
{
	float4 c;
	c.xyz = wirecolor;
	c.w = 1.0;
	return c;
}

technique wireframe
{
	pass p0
	{
		ZENABLE = TRUE;
		CullMode = NONE;
		FILLMODE=WIREFRAME;
		PixelShader  = compile ps_2_0 pixel_shader_wirecolor();
		VertexShader = compile vs_2_0 vertex_shader_positionoffsetn();
	}
}

