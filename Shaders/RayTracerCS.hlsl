RWTexture2D<float3> renderTexture : register(u0);

cbuffer GlobalCBuffer : register(b0)
{
    float4 cameraPosition;
    float2 screenDimensions;
    float2 padding;
};

float3 BackgroundColor(float2 pixelCoords)
{
    float yHeight = 0.5f * (pixelCoords.y + 1.0f);
    return (1.0f - yHeight) * float3(1.0f, 1.0f, 1.0f) + yHeight * float3(0.5f, 0.7f, 1.0f);
}

float SphereRayIntersection(float3 sphereCenter, float radius, float3 rayDirection)
{
    float a = dot(rayDirection, rayDirection);
    float b = 2.0f * dot(sphereCenter, rayDirection);
    float c = dot(sphereCenter, sphereCenter) - radius * radius;

    float desc = b * b - 4 * a * c;
    if (desc >= 0)
    {
        float root = (-b - sqrt(desc)) / (2.0f * a);
        if (root > 0.0001f)
        {
            return root;
        }
    }

    return -1.0f;
}

[numthreads(8, 4, 1)]
void CsMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // PixelCoords are in the range of 0..1 with (0, 0) being the top left corner.
    float2 pixelCoords = dispatchThreadID.xy / screenDimensions;
    
    // Centering pixelCoords so that (0, 0) appears in the middle of the screen and extents are -2..2 on x and -1..1 on x. 
    // This is assuming that window dimesions will have aspect ratio of 0.5f, which is default behavior.
    pixelCoords = pixelCoords * 2;
    pixelCoords = pixelCoords - float2(1.0f, 1.0f);

    pixelCoords.x = pixelCoords.x * 2.0f;

    float3 rayDirection = float3(pixelCoords.xy, 0.0f) - cameraPosition.xyz;
    rayDirection = normalize(rayDirection);

    float3 result = BackgroundColor(pixelCoords);

    const float3 SPHERE_CENTER = float3(0.0f, 0.0f, 1.0f);
    const float SPHERE_RADIUS = 0.5f;

    const float3 GROUND_SPHERE_CENTER = float3(0.0f, -100.5f, 1.0);
    const float GROUND_SPHERE_RADIUS = 100.0;

    float intersectionResult = SphereRayIntersection(SPHERE_CENTER, SPHERE_RADIUS, rayDirection);
    if (intersectionResult != -1.0f)
    {
        float3 N = -normalize(rayDirection * intersectionResult - SPHERE_CENTER);
        result = 0.5f * (N + float3(1.0f, 1.0f, 1.0f));
    }
    else
    {
        float groundSphereIntersectionResult = SphereRayIntersection(GROUND_SPHERE_CENTER, GROUND_SPHERE_RADIUS, rayDirection);
        if (groundSphereIntersectionResult != -1.0f)
        {
            float3 N = normalize(rayDirection * groundSphereIntersectionResult - GROUND_SPHERE_CENTER);
            result = 0.5f * (N + float3(1.0f, 1.0f, 1.0f));
        }
    }

    renderTexture[dispatchThreadID.xy] = result;
}