RWTexture2D<float4> outputTexture : register(u0);
Texture2D<float4> offscreenTexture : register(t0);
SamplerState linearClampSampler : register(s0);

struct Sphere
{
    float3 center;
    float radius;
    float3 color;
    float padding;
};

cbuffer globalConstantBuffer : register(b0)
{
    float2 screenDimensions;
    float2 padding; 
    Sphere sphere;
};

// Ray has a origin and a direction and is of the form R(t) = A + tB (A : origin, t : a parameter / scale, B : direction).
struct Ray
{
    float3 origin;
    float3 direction;
};

Ray createRay(float3 origin, float3 direction)
{
    Ray ray;
    ray.origin = origin;
    ray.direction = normalize(direction);
    
    return ray;
}

float3 pointAtParameter(Ray ray, float t)
{
    return ray.origin + ray.direction * t;
}

// For a ray to hit a sphere, the ray must hit a point on the circumference of the sphere.
// Let the center of sphere by c = (cx, cy) with radius r, and ray have direction, origin, and parameter t.
// Now, distance((ray.origin + ray.direction * t) - c) == r^2 implies that ray at parameter 't' will intersect the sphere.
// If the equation has 0 roots, ray does not intersect the sphere. Else, it intersects.
bool rayHitSphere(Ray ray, Sphere sphere)
{
    // Equation : t*t*dot(B,B) + 2*t*dot(B,A-C) + dot(A-C,A-C) - R*R
    // Note : This is taken from the ray tracing in a weekend book.
    const float a = dot(ray.direction, ray.direction);
    const float b = 2 * dot(ray.direction, ray.origin - sphere.center);
    const float c = dot(ray.origin - sphere.center, ray.origin - sphere.center) - sphere.radius * sphere.radius;
    
    const float descriminant = b * b - 4 * a * c;
    return descriminant > 0.0f;
}

float3 getColor(Ray ray)
{
    if (rayHitSphere(ray, sphere))
    {
        return sphere.color;
    }
    
    ray.direction = normalize(ray.direction);
    float t = 0.5f * (ray.direction.y + 1);
    return lerp(float3(1.0f, 1.0f, 1.0f), float3(0.5f, 0.7f, 1.0f), t);
}

[numthreads(12, 8, 1)]
void CsMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // UV Coordinate goes from screen left top to bottom right.
    const float2 uv = dispatchThreadID.xy / screenDimensions;
   
    // Unlike the ray tracing in a weekend book, we take the extents of the 'imaging plane' to be -1, 1 on X and Y axis
    // and 0 to 1 on the Z axis, as we assume all objects to be in NDC coordinates.
    // To traverse the screen therefore, we just need to convert the [0, 1] of uv to [-1, 1].
    // Imaging plane is assumed to be at z = 0, and camera will be at z = -1. V will be inverted to it goes from bottom to top.
    
    const float3 ORIGIN = float3(0.0f, 0.0f, -1.0f);
    const float3 imagePlaneCoords = float3(float2(uv.x, 1.0f - uv.y) * 2.0f - 1.0f, 0.0f);
    
    Ray ray = createRay(ORIGIN, imagePlaneCoords - ORIGIN);
    
    outputTexture[dispatchThreadID.xy] = float4(getColor(ray), 1.0f) + offscreenTexture.SampleLevel(linearClampSampler, uv, 0u);
}