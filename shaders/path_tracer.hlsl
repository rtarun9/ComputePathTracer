RWTexture2D<float4> outputTexture : register(u0);

cbuffer GlobalConstantBuffer: register(b0)
{
    float2 screenDimensions;
};

[numthreads(12, 8, 1)]
void CsMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{   
    const float2 uv = dispatchThreadID.xy / screenDimensions;
    outputTexture[dispatchThreadID.xy] = float4(uv, 0.0f, 1.0f);
}