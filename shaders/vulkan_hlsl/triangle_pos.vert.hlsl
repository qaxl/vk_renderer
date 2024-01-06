float2 positions[3] = {
    float2(0.0, -0.5),
    float2(0.5, 0.5),
    float2(-0.5, 0.5)
};

struct VS_OUTPUT {
    float4 Pos : SV_POSITION;
};

VS_OUTPUT main(uint VertexID : SV_VertexID) {
    VS_OUTPUT output;
    output.Pos = float4(positions[VertexID], 0.0, 1.0);
    return output;
}
