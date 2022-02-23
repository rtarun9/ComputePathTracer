RWTexture2D<float3> renderTexture : register(u0);

static const float T_MIN = 0.01f;
static const float T_MAX = 1000000.0f;

static const float RAY_BOUNCES = 400.0f;

static const float SCENE_OBJECTS = 4;

static const float SAMPLES_PER_PIXEL = 16;

// Note : Materials and objects are set seperately but are of same size and the object index corresponds so the material index.
static const int MATERIAL_NONE = -1;
static const int MATERIAL_DIFFUSE = 0;
static const int MATERIAL_METAL = 1;
static const int MATERIAL_DIELECTRIC = 2;

cbuffer GlobalCBuffer : register(b0)
{
    float4 cameraPosition;
    float4 cameraLookAt;
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

// Refract function exist in HLSL, but making my own for learning purposes.
// Return false if refraction is not possible (in case of Total internal refraction).
// refractiveIndexRatio = ni / nt.
bool RefractRay(float3 incomingVector, float3 normal, float refractiveIndexRatio, inout float3 refractedRay)
{
    float3 v = normalize(incomingVector);
    float dt = dot(v, normal);
    float desc = 1.0f - refractiveIndexRatio * refractiveIndexRatio * (1 - dt * dt);
    if (desc > 0.0f)
    {
        refractedRay = refractiveIndexRatio * (v - normal * dt) - normal * sqrt(desc);
        return true;
    }

    return false;
}

// Used for giving the varying effect of reflectivity.
// If angle is steeper, the object behaves like a mirror.
float Schlick(float cosine, float refractiveIndex)
{
    float r0 = (1 - refractiveIndex) / (1 + refractiveIndex);
    r0 = pow(r0, 2);
    return r0 + (1 - r0) * pow((1.0f - cosine), 5);

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
    int materialIndex;
};

Sphere CreateSphere()
{
    Sphere sphere;
    sphere.center = float3(0.0f, 0.0f, 1.0f);
    sphere.radius = 0.25f;
    sphere.materialIndex = -1;

    return sphere;
}

Sphere CreateSphere(float3 center, float radius, int materialIndex)
{
    Sphere sphere;
    sphere.center = center;
    sphere.radius = radius;
    sphere.materialIndex = materialIndex;
   
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

// Value for now is unused in diffuse materials, fuzziness in specular material, and refractive index in di-electric materials.
struct Material
{
    float3 albedo;
    float value;
    int materialType;
};

Material CreateMaterial()
{
    Material material;
    material.albedo = float3(0.0f, 0.0f, 0.0f);
    material.value = -1;
    material.materialType = MATERIAL_NONE;

    return material;
}

Material CreateMaterial(float3 albedo, float value, int materialType)
{
    Material material;
    material.albedo = albedo;
    material.value = value;
    material.materialType = materialType;

    return material;
}


static Material MATERIALS[SCENE_OBJECTS] =
{
    CreateMaterial(float3(0.8f, 0.6f, 0.8f), 0.0f, MATERIAL_DIFFUSE),
    CreateMaterial(float3(1.0f, 0.5f, 0.5f), 0.0f, MATERIAL_METAL),
    CreateMaterial(float3(1.0f, 1.0f, 1.0f), 1.5f, MATERIAL_DIELECTRIC),
    CreateMaterial(float3(0.6f, 0.5f, 0.5f), 0.0f, MATERIAL_DIFFUSE),
};

static Sphere SCENE_SPHERES[SCENE_OBJECTS] =
{ 
    CreateSphere(float3(0.0f, 0.0f, 1.0f), 0.5f, 0),
    CreateSphere(float3(1.0f, 0.0f, 1.0f), 0.5f, 1),
    CreateSphere(float3(-1.0f, 0.0f, 1.0f), 0.5f, 2),
    CreateSphere(float3(0.0f, -100.45f, 1.0f), 100.0f, 3),
};


void ScatterDiffuse(inout Ray ray, inout HitRecord hr, inout float3 color, int materialIndex)
{
    color *= MATERIALS[materialIndex].albedo;
    ray.origin = hr.position;
    ray.direction = hr.position + hr.normal + RandomVectorInSphere(hr.normal.xy);
    ray.direction = ray.direction - hr.position;
    ray.direction = normalize(ray.direction);
}

void ScatterMetal(inout Ray ray, inout HitRecord hr, inout float3 color, int materialIndex)
{
    color *= MATERIALS[hr.objectID].albedo;
    ray.origin = hr.position;
    float3 scatteredDirection = reflect(ray.direction, hr.normal + MATERIALS[materialIndex].value * RandomVectorInSphere(ray.direction.xy));
    
    if (dot(scatteredDirection, hr.normal) > 0)
    {
        ray.direction = scatteredDirection;
    }
}

void ScatterDielectric(inout Ray ray, inout HitRecord hr, inout float3 color, int materialIndex)
{
    color *= MATERIALS[materialIndex].albedo;
    float3 normalOutward = float3(0.0f, 0.0f, 0.0f);
    float3 reflectedRay = reflect(ray.direction, hr.normal);
    float3 refractedRay = float3(0.0f, 0.0f, 0.0f);

    float cosine = 0.0f;
    float refelctionProbability = 0.0f;

    float refractiveIndexRatio = 0.0f;
    if (dot(ray.direction, hr.normal) > 0.0f)
    {
        normalOutward = -hr.normal;
        refractiveIndexRatio = MATERIALS[materialIndex].value;
        cosine = MATERIALS[materialIndex].value * dot(ray.direction, hr.normal) / length(ray.direction);

    }
    else
    {
        normalOutward = hr.normal;
        refractiveIndexRatio = 1.0f / MATERIALS[materialIndex].value;
        cosine = -dot(ray.direction, hr.normal) / length(ray.direction);
    }

    if (RefractRay(ray.direction, normalOutward, refractiveIndexRatio, refractedRay))
    {
        refelctionProbability = Schlick(cosine, MATERIALS[materialIndex].value);
    }
    else
    {
        refelctionProbability = 1.1f;
    }

    if (Random(hr.normal.xy) < refelctionProbability)
    {
        ray = CreateRay(hr.position, reflectedRay);
    }
    else
    {
        ray = CreateRay(hr.position, refractedRay);
    }

}

void Scatter(inout Ray ray, inout HitRecord hr, inout float3 color, int materialIndex)
{
    switch (MATERIALS[materialIndex].materialType)
    {
        case MATERIAL_DIFFUSE:
        {
            ScatterDiffuse(ray, hr, color, materialIndex);
            break;
        }

        case MATERIAL_METAL:
        {
            ScatterMetal(ray, hr, color, materialIndex);
            break;
        }

        case MATERIAL_DIELECTRIC:
        {
            ScatterDielectric(ray, hr, color, materialIndex);
            break;
        }
    }
}

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
            materialID = SCENE_SPHERES[i].materialIndex;
            hitRecord.objectID = i;
        }
    }

    return materialID;
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
        int materialIndex = HitSceneObjects(ray, tMin, tMax, hr);
        if (materialIndex != -1)
        {
            Scatter(ray, hr, color, materialIndex);

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

    Ray ray = CreateRay(cameraPosition.xyz, normalize(cameraLookAt.xyz + float3(pixelCoords.xy, 1.0f) - cameraPosition.xyz));
    return ray;
}

[numthreads(8, 8, 1)]
void CsMain(uint3 dispatchThreadID : SV_DispatchThreadID, uint groupIndex : SV_GroupIndex)
{
    Ray ray = GenerateRay(dispatchThreadID, 0, 0);

    float3 result = float3(0.0f, 0.0f, 0.0f);

    for (int i = 0; i < SAMPLES_PER_PIXEL; i++)
    {
        float randomX = Random(ray.direction.xy);
        float randomY = Random(ray.direction.yx);

        Ray antiAliasingRay = GenerateRay(dispatchThreadID, randomX, randomY);
        result += GetColor(antiAliasingRay);
    }

    result /= SAMPLES_PER_PIXEL;
  
    renderTexture[dispatchThreadID.xy] = result;
}