static float2 positions[3] = float2[]
(
	float2(0.0, -0.5),
	float2(0.5, 0.5),
	float2(-0.5, -0.5),
);

static float3 colors[3] = float3[]
(
	float3(1.0, 0.0, 0.0),
	float3(0.0, 1.0, 0.0),
	float3(0.0, 0.0, 1.0),
);

struct VertexOutput
{
	float3 color;
	float4 sv_position: SV_Position;
};

[shader("vertex")]
VertexOutput vert_main(uint vid : SV_VertexID)
{
	return VertexOutput(colors[vid], float4(positions[vid], 0.0, 1.0));
};

[shader("fragment")]
float4 frag_main(VertexOutput in) : SV_Target
{
	return float4(in.color, 1.0);
};

