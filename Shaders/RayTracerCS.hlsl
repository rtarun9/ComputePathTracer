RWTexture2D<float3> renderTexture : register(u0);

static const float T_MIN = 0.01f;
static const float T_MAX = 1000000.0f;

static const float RAY_BOUNCES = 50.0f;

static const float SCENE_OBJECTS = 4;

static const float SAMPLES_PER_PIXEL = 10;

static const int MATERIAL_DIFFUSE = 0;
static const int MATERIAL_METAL = 1;

cbuffer GlobalCBuffer : register(b0)
{
    float4 cameraPosition;
    float2 screenDimensions;
    uint frameIndex;
    float4 padding;
    float4 padding2;
    float padding3;
};

float Random(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898, 78.233))) * 43758.5453);
}

float3 RandomVectorInSphere(float2 num)
{
    float3 p = float3(0.0f, 0.0f, 0.0f);
    do
    {
        //p = 2.0f * float3(Random(num.xx * frameIndex), Random(num.yy * frameIndex), Random(num.xy * frameIndex));
        p = 2.0f * float3(Random(num.xx), Random(num.yy), Random(num.xy));
    }
    while (length(p * p) >= 1.0f);

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
    float objectID;
};

HitRecord CreateHitRecord()
{
    HitRecord hitRecord;
    hitRecord.position = float3(0.0f, 0.0f, 0.0f);
    hitRecord.normal = float3(0.0f, 0.0f, 0.0f);
    hitRecord.distance = 0.0f;
    hitRecord.objectID = -1;

    return hitRecord;
}

struct Sphere
{
    float3 center;
    float radius;
    int material;
    float3 albedo;
};

Sphere CreateSphere()
{
    Sphere sphere;
    sphere.center = float3(0.0f, 0.0f, 1.0f);
    sphere.radius = 0.25f;
    sphere.material = MATERIAL_DIFFUSE;
    sphere.albedo = float3(1.0f, 1.0f, 1.0f);
    return sphere;
}

Sphere CreateSphere(float3 center, float radius, int material, float3 albedo)
{
    Sphere sphere;
    sphere.center = center;
    sphere.radius = radius;
    sphere.material = material;
    sphere.albedo = albedo;
    return sphere;
}

float SphereIntersection(Ray ray, Sphere sphere, float tMin, inout float tMax, inout HitRecord hitRecord)
{
    float3 sphereCenterToRayOrigin = ray.origin - sphere.center;
    float a = dot(ray.direction, ray.direction);
    float b = dot(sphereCenterToRayOrigin, ray.direction);
    float c = dot(sphereCenterToRayOrigin, sphereCenterToRayOrigin) - sphere.radius * sphere.radius;
    
    float desc = b * b -   a * c;
    if (desc > 0.0f)
    {
        float root = (-b - sqrt(desc)) /  (a);
        if (root > tMin && root < tMax)
        {
            hitRecord.distance = root;
            hitRecord.position = AtParam(ray, root);
            hitRecord.normal = normalize(hitRecord.position - sphere.center);
            
            // objectID set in HitSceneObjects function, default initialized here to -1 to prevent compile errors.
            hitRecord.objectID = -1;

            tMax = root;

            return 1.0f;
        }
     
        root = (-b + sqrt(desc)) / (a);
        if (root > tMin && root < tMax)
        {
            hitRecord.distance = root;
            hitRecord.position = AtParam(ray, root);
            hitRecord.normal = normalize(hitRecord.position - sphere.center);

            // objectID set in HitSceneObjects function, default initialized here to -1 to prevent compile errors.
            hitRecord.objectID = -1;

            tMax = root;

            return 1.0f;
        }
    }

    return -1.0f;
}

static const Sphere SCENE_SPHERES[SCENE_OBJECTS] =
{ 
    CreateSphere(float3(0.0f, 0.0f, 1.0f), 0.5f, MATERIAL_DIFFUSE, float3(0.8f, 0.6f, 0.8f)),
    CreateSphere(float3(1.0f, 0.0f, 1.0f), 0.5f, MATERIAL_METAL, float3(1.0f, 0.5f, 0.5f)),
    CreateSphere(float3(-1.0f, 0.0f, 1.0f), 0.5f, MATERIAL_METAL, float3(0.5f, 0.5f, 1.0f)),
    CreateSphere(float3(0.0f, -100.5f, 1.0f), 100.0f, MATERIAL_DIFFUSE, float3(0.5f, 0.5f, 0.5f)),
};

float3 BackgroundColor(float3 rayDirection)
{
    float yHeight = 0.5f * (-rayDirection.y + 1.0f);
    return (1.0f - yHeight) * float3(1.0f, 1.0f, 1.0f) + yHeight * float3(0.5f, 0.7f, 1.0f);
}

// Return material id (if hit any - else returns -1).
int HitSceneObjects(Ray ray, float tMin, inout float tMax, inout HitRecord hitRecord)
{
    int materialID = -1;

    bool hit = false;
    for (int i = 0; i < SCENE_OBJECTS; i++)
    {
        float t = SphereIntersection(ray, SCENE_SPHERES[i], tMin, tMax, hitRecord);
        if (t > 0.0f)
        {
            materialID = SCENE_SPHERES[i].material;
            hitRecord.objectID = i;
        }
    }

    return materialID;
}

void ScatterDiffuse(inout Ray ray, inout HitRecord hr, inout float3 color)
{
    color *= SCENE_SPHERES[hr.objectID].albedo;
    ray.origin = hr.position;
    ray.direction = hr.position + hr.normal + RandomVectorInSphere(hr.normal.xy);
    ray.direction = ray.direction - hr.position;
    ray.direction = normalize(ray.direction);
}

void ScatterMetal(inout Ray ray, inout HitRecord hr, inout float3 color)
{
    color *= SCENE_SPHERES[hr.objectID].albedo;
    ray.origin = hr.position;
    ray.direction = reflect(ray.direction, hr.normal);
}
float3 GetColor(Ray ray)
{
    int rayBounces = RAY_BOUNCES;

    float3 color = float3(1.0f, 1.0f, 1.0f);

    float tMin = T_MIN;
    float tMax = T_MAX;
    HitRecord hr;

    while (rayBounces >= 0)
    {
        int id = HitSceneObjects(ray, tMin, tMax, hr);
        if (id != -1)
        {
            switch (id)
            {
                case MATERIAL_DIFFUSE:
                {
                    ScatterDiffuse(ray, hr, color);
                    break;
                }

                case MATERIAL_METAL:
                {
                    ScatterMetal(ray, hr, color);
                    break;
                }
            }

            rayBounces--;
        }
        else
        {
            color *= BackgroundColor(ray.direction);
            return color;
        }
    }

    return color;
}

Ray GenerateRay(uint3 dispatchThreadID, float x, float y)
{
     // PixelCoords are in the range of 0..1 with (0, 0) being the top left corner.
    float2 pixelCoords = (dispatchThreadID.xy) / screenDimensions;
    
    pixelCoords += float2(x, y) / screenDimensions;

    // Centering pixelCoords so that (0, 0) appears in the middle of the screen and extents are -2..2 on x and -1..1 on x. 
    // This is assuming that window dimesions will have aspect ratio of 0.5f, which is default behavior.
    pixelCoords = pixelCoords * 2;
    pixelCoords = pixelCoords - float2(1.0f, 1.0f);

    pixelCoords.x = pixelCoords.x * 2.0f;

    // Invert pixelCoord.y so that window up direction is the positive y axis.
    pixelCoords.y *= -1.0f;

    // Main Render logic.

    Ray ray = CreateRay(cameraPosition.xyz, normalize(float3(pixelCoords.xy, 1.0f) - cameraPosition.xyz));
    return ray;
}

[numthreads(8, 8, 1)]
void CsMain(uint3 dispatchThreadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    Ray ray = GenerateRay(dispatchThreadID, 0, 0);

    float3 result = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < SAMPLES_PER_PIXEL; i++)
    {
        float randomX = 2.0f * Random(ray.direction.yx) - 1.0f;
        float randomY = 2.0f * Random(ray.direction.xy) - 1.0f;

        Ray antiAliasingRay = GenerateRay(dispatchThreadID, randomX * 0.2f, randomY * 0.2f);
        result += GetColor(antiAliasingRay);
    }

    result /= SAMPLES_PER_PIXEL;
  
    renderTexture[dispatchThreadID.xy] = result;
}