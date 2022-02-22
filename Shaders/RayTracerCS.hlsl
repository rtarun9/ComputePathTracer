RWTexture2D<float3> renderTexture : register(u0);

static const float T_MIN = 0.0001f;
static const float T_MAX = 1000000.0f;

static const float RAY_BOUNCES = 50.0f;

cbuffer GlobalCBuffer : register(b0)
{
    float4 cameraPosition;
    float2 screenDimensions;
    float3 randomNumbers;
    uint rngState;
    float4 padding2;
    float2 padding3;
};

// From : https://www.reedbeta.com/blog/quick-and-easy-gpu-random-numbers-in-d3d11/
uint randXorshift(uint rngState)
{
    // Xorshift algorithm from George Marsaglia's paper
    rngState ^= (rngState << 13);
    rngState ^= (rngState >> 17);
    rngState ^= (rngState << 5);
    return rngState;
}


float Random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453123);

}

float3 RandomVectorInSphere(float3 rand, uint rngState)
{
    float3 p = float3(0.0f, 0.0f, 0.0f);
    
    do
    {
        p = 2.0f * float3(float(randXorshift(rngState)) * (1.0 / 4294967296.0), float(randXorshift(rngState)) * (1.0 / 4294967296.0), float(randXorshift(rngState)) * (1.0 / 4294967296.0));
    }
    while (dot(p, p) >= 1.0f);
    
    return p;
}

struct Ray
{
    float3 origin;
    float3 direction;
};

Ray CreateRay()
{
    Ray ray;
    ray.origin = float3(0.0f, 0.0f, 0.0f);
    ray.direction = float3(0.0f, 0.0f, 1.0f);

    return ray;
}

Ray CreateRay(float3 origin, float3 direction)
{
    Ray ray;
    ray.origin = origin;
    ray.direction = direction;

    return ray;
}

float3 AtParam(Ray ray, float temp)
{
    return ray.origin + ray.direction * temp;
}

struct HitRecord
{
    float3 position;
    float3 normal;
    float distance;
};

HitRecord CreateHitRecord()
{
    HitRecord hitRecord;
    hitRecord.position = float3(0.0f, 0.0f, 0.0f);
    hitRecord.normal = float3(0.0f, 0.0f, 0.0f);
    hitRecord.distance = 0.0f;

    return hitRecord;
}

struct Sphere
{
    float3 center;
    float radius;
};

Sphere CreateSphere()
{
    Sphere sphere;
    sphere.center = float3(0.0f, 0.0f, 1.0f);
    sphere.radius = 0.25f;

    return sphere;
}

Sphere CreateSphere(float3 center, float radius)
{
    Sphere sphere;
    sphere.center = center;
    sphere.radius = radius;

    return sphere;
}

float SphereIntersection(Ray ray, Sphere sphere, float tMin, inout float tMax, inout HitRecord hitRecord)
{
    float3 sphereCenterToRayOrigin = ray.direction - sphere.center;
    float a = dot(ray.direction, ray.direction);
    float b = 2.0f * dot(sphereCenterToRayOrigin, ray.direction);
    float c = dot(sphereCenterToRayOrigin, sphereCenterToRayOrigin) - sphere.radius * sphere.radius;
    
    float desc = b * b -  4 * a * c;
    if (desc > 0.0f)
    {
        float root = (-b - sqrt(desc)) / (2.0f * a);
        if (root > tMin && root < tMax)
        {
            hitRecord.distance = root;
            hitRecord.position = AtParam(ray, root);
            hitRecord.normal = (hitRecord.position - sphere.center) / sphere.radius;

            tMax = root;

            return 1.0f;
        }
     
        root = (-b + sqrt(desc)) / (2.0f * a);
        if (root > tMin && root < tMax)
        {
            hitRecord.distance = root;
            hitRecord.position = AtParam(ray, root);
            hitRecord.normal = (hitRecord.position - sphere.center) / sphere.radius;

            tMax = root;

            return 1.0f;
        }
    }

    return -1.0f;
}

static const Sphere SCENE_SPHERES[2] = 
{ 
    CreateSphere(float3(0.0f, -100.5f, 10.0f), 100.0f), 
    CreateSphere(float3(0.0f, 0.0f, 1.0f), 0.25f)
};

float3 BackgroundColor(float3 rayDirection)
{
    float yHeight = 0.5f * (-rayDirection.y + 1.0f);
    return (1.0f - yHeight) * float3(1.0f, 1.0f, 1.0f) + yHeight * float3(0.5f, 0.7f, 1.0f);
}

float TraceThroughScene(Ray ray, HitRecord hitRecord, float tMin, inout float tMax)
{
    float result = -1.0f;

    for (float i = 0.0f; i < 2; i++)
    {
        float t = SphereIntersection(ray, SCENE_SPHERES[i], T_MIN, tMax, hitRecord);
        if (t > 0.0f)
        {
            result = 1.0f;
        }
    }

    return result;
}

float3 Trace(Ray ray, HitRecord hitRecord, float tMin, inout float tMax, float2 uv, uint rngState)
{
    float3 result = float3(1.0f, 1.0f, 1.0f);
    float attenuation = 1.0f;

    for (float k = 0; k < RAY_BOUNCES; k++)
    {
        HitRecord hr = hitRecord;

        float t = TraceThroughScene(ray, hr, tMin, tMax);
        if (t > 0.0f)
        {
            float3 target = hr.normal + RandomVectorInSphere(ray.direction, rngState);
            ray.origin = hr.position;
            ray.direction = normalize(target - hr.position);

            attenuation *= 0.5f;
        }
        else
        {
            return BackgroundColor(normalize(ray.direction)) * attenuation;
        }
    }

    return float3(0.0f, 0.0f, 0.0f);
}

[numthreads(8, 8, 1)]
void CsMain(uint3 dispatchThreadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    uint state = groupIndex * dispatchThreadID.x;

    // PixelCoords are in the range of 0..1 with (0, 0) being the top left corner.
    float2 pixelCoords = dispatchThreadID.xy / screenDimensions;
    
    // Centering pixelCoords so that (0, 0) appears in the middle of the screen and extents are -2..2 on x and -1..1 on x. 
    // This is assuming that window dimesions will have aspect ratio of 0.5f, which is default behavior.
    pixelCoords = pixelCoords * 2;
    pixelCoords = pixelCoords - float2(1.0f, 1.0f);

    pixelCoords.x = pixelCoords.x * 2.0f;

    // Invert pixelCoord.y so that window up direction is the positive y axis.
    pixelCoords.y *= -1.0f;

    state *= pixelCoords.x * screenDimensions.y * screenDimensions.x;

    float3 rayDirection = float3(pixelCoords.xy, 1.0f) - cameraPosition.xyz;
    rayDirection = normalize(rayDirection);

    Ray ray = CreateRay(cameraPosition.xyz, rayDirection);

    HitRecord hitRecord = CreateHitRecord();

    float tMax = T_MAX;

    float3 result = Trace(ray, hitRecord, T_MIN, tMax, pixelCoords, state);
    result = pow(result, 1 / 2.2f);

    renderTexture[dispatchThreadID.xy] = result;
}