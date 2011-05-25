//   
// marker effect, typicall used for drawing lines and crap just to mark stuff 
// we assume input verticies are already in world coord space!!
//

float4x4 ViewProj ;

struct VtoPpc
{
	float4 screenpos : POSITION;
	float4 color    : COLOR;
};

VtoPpc vertex_shader_position_color(const float4 p:POSITION,const float4 c: COLOR)  // assumes vertex is already in world space, passes color through
{
	VtoPpc OUT;
	OUT.screenpos = mul(p, ViewProj);
	OUT.color    = c;
	return OUT;
}


float4 pixel_shader_color(const float4 c:COLOR):COLOR0
{
	return c;
}

technique marker
{
	pass p0
	{
		STENCILENABLE=    FALSE  ;
		ALPHABLENDENABLE= FALSE ;
		SHADEMODE = GOURAUD ;
		ZWRITEENABLE =     TRUE ;
		ZENABLE = TRUE;
		CullMode = CW;
		PixelShader  = compile ps_2_0 pixel_shader_color();
		VertexShader = compile vs_2_0 vertex_shader_position_color();
	}
}

