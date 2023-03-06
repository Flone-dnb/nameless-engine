#ifdef TEXTURE_FILTERING_POINT
    SamplerState samplerPointWrap       : register(s0);
#elif TEXTURE_FILTERING_LINEAR
    SamplerState samplerLinearWrap      : register(s0);
#elif TEXTURE_FILTERING_ANISOTROPIC
    SamplerState samplerAnisotropicWrap : register(s0);
#endif

#ifdef RECEIVE_DYNAMIC_SHADOWS
    SamplerComparisonState samplerShadowMap : register(s1);
#endif

// will be used later
// #ifdef USE_DIFFUSE_TEXTURE
//     Texture2D diffuseTexture : register(t0);
// #endif

cbuffer frameData : register(b0)
{
    float4x4 viewMatrix;
    float4x4 inverseViewMatrix;
    float4x4 projectionMatrix;
    float4x4 inverseProjectionMatrix;
    float4x4 viewProjectionMatrix;
    float4x4 inverseViewProjectionMatrix;

    float nearZ;
    float farZ;
    float totalTimeInSec;
    float timeSincePrevFrameInSec;

    int iRenderTargetWidth;
    int iRenderTargetHeight;

    float2 _framePad;

    // remember to add padding to 4 floats
}

cbuffer meshData : register(b1)
{
    float4x4 worldMatrix; 
    
    // remember to add padding to 4 floats
};

struct VertexIn
{
    float3 localPosition : POSITION;    // position in local space
    float3 localNormal   : NORMAL;      // normal in local space
    float2 uv            : UV;
};

struct VertexOut
{
    float4 viewPosition            : SV_POSITION; // position in view space
	float4 worldPosition           : POSITION;    // position in world space
    float3 worldNormal             : NORMAL;      // normal in world space
    float2 uv                      : UV;
};

VertexOut vsDefault(VertexIn vertexIn)
{
    VertexOut vertexOut;

    // Calculate world position and normal.
    vertexOut.worldPosition = mul(float4(vertexIn.localPosition, 1.0f), worldMatrix);
    vertexOut.worldNormal = mul(vertexIn.localNormal, (float3x3)worldMatrix);

    // Transform position to homogeneous clip space.
    vertexOut.viewPosition = mul(vertexOut.worldPosition, viewProjectionMatrix);

    // Copy UV.
    vertexOut.uv = vertexIn.uv;

    return vertexOut;
}

float4 psDefault(VertexOut pin) : SV_Target
{
    float4 diffuseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

// will be used later
// #ifdef USE_DIFFUSE_TEXTURE
//     // Apply texture filtering.
//     #ifdef TEXTURE_FILTERING_POINT
//         diffuseColor = diffuseTexture.Sample(samplerPointWrap, pin.uv);
//     #elif TEXTURE_FILTERING_LINEAR
//         diffuseColor = diffuseTexture.Sample(samplerLinearWrap, pin.uv);
//     #elif TEXTURE_FILTERING_ANISOTROPIC
//         diffuseColor = diffuseTexture.Sample(samplerAnisotropicWrap, pin.uv);
//     #endif
// #endif

    return diffuseColor;
}