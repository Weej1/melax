//   font effect.
//

texture diffusemap : DiffuseMap;
float4x4 WorldViewProj ;

sampler samp = sampler_state
{
	texture = <diffusemap>;
	AddressU = WRAP;
	AddressV = WRAP;
	MIPFILTER = LINEAR;
	MINFILTER = LINEAR;
	MAGFILTER = LINEAR;
};

struct VS_INPUT
{
	float4 position : POSITION;
	float4 normal   : NORMAL;
	float4 texcoord : TEXCOORD;
	float4 color    : COLOR;
};

struct VtoP
{
	float4 position : POSITION;
	float4 color    : COLOR;
	float4 tex      : TEXCOORD;
};

VtoP vertex_shader(const VS_INPUT IN)
{
	VtoP OUT;
	OUT.position = IN.position;
	OUT.tex = IN.texcoord;
	OUT.color = IN.color;
	return OUT;
}


VtoP vertex_shader_reg(const VS_INPUT IN)
{
	VtoP OUT;
	float4 p = mul(IN.position, WorldViewProj);
	OUT.position = p;
	OUT.tex = IN.texcoord;
	OUT.tex.y = -IN.texcoord.y;
	OUT.color = float4(1,1,1,1); //IN.color;

	return OUT;
}

struct PS_OUTPUT
{
	float4 color : COLOR0;
};

PS_OUTPUT pixel_shader(const VtoP IN)
{
	PS_OUTPUT OUT;
	OUT.color = tex2D(samp, IN.tex) * IN.color;
	return OUT;
}

technique screenspace
{
	pass p0
	{
		ZENABLE   = FALSE;
		ZFunc = LESSEQUAL ;
		CullMode  = CCW;
        ALPHABLENDENABLE = TRUE ;
        SRCBLEND  = SRCALPHA ;
        DESTBLEND = INVSRCALPHA ;
        ALPHATESTENABLE =  TRUE ;
        ALPHAREF  = 0x08 ;
        ALPHAFUNC = GREATEREQUAL ;
        FILLMODE  = SOLID;
        STENCILENABLE =    FALSE ;
        CLIPPING  = TRUE ;
        CLIPPLANEENABLE=  0 ;
        VERTEXBLEND = DISABLE ;
        INDEXEDVERTEXBLENDENABLE = FALSE ;
        FOGENABLE = FALSE ;
		FVF = XYZRHW|DIFFUSE|TEX1 ;
		PixelShader  = compile ps_2_0 pixel_shader();
	}
}


technique t0
{
	pass p0
	{
		ZFUNC = LESSEQUAL ;
		ZWRITEENABLE =     TRUE ;
		CullMode = NONE;
		STENCILENABLE=    FALSE  ;
		SHADEMODE = GOURAUD ;

        ALPHABLENDENABLE = TRUE ;
        SRCBLEND  = SRCALPHA ;
        DESTBLEND = INVSRCALPHA ;
        ALPHATESTENABLE =  TRUE ;
        ALPHAREF  = 0x08 ;
        ALPHAFUNC = GREATEREQUAL ;
        FILLMODE  = SOLID;
        STENCILENABLE =    FALSE ;
        CLIPPING  = TRUE ;
        CLIPPLANEENABLE=  0 ;
        VERTEXBLEND = DISABLE ;
        INDEXEDVERTEXBLENDENABLE = FALSE ;
        FOGENABLE = FALSE ;
        VertexShader = compile vs_2_0 vertex_shader_reg();
		PixelShader  = compile ps_2_0 pixel_shader();
	}
}
