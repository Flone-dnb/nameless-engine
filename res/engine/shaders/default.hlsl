#ifdef TEXTURE_FILTERING_POINT
    SamplerState samplerPointWrap       : register(s0);
#elif TEXTURE_FILTERING_LINEAR
    SamplerState samplerLinearWrap      : register(s0);
#elif TEXTURE_FILTERING_ANISOTROPIC
    SamplerState samplerAnisotropicWrap : register(s0);
#endif

// TODO: wrap with something like RECEIVE_SHADOWS
SamplerComparisonState samplerShadowMap : register(s1);

#ifdef USE_DIFFUSE_TEXTURE
    Texture2D diffuseTexture : register(t0);
#endif

cbuffer frameData : register(b0)
{
    float4x4 mView;
    float4x4 mInvView;
    float4x4 mProjection;
    float4x4 mInvProjection;
    float4x4 mViewProjection;
    float4x4 mInvViewProjection;

    float fNearZ;
    float fFarZ;
    float fTotalTime;
    float fTimeSincePrevFrame;

    int iRenderTargetWidth;
    int iRenderTargetHeight;

    float2 _framePad;

    // remember to add padding to 4 floats
}

cbuffer objectData : register(b1)
{
    float4x4 vWorld; 
    float4x4 vTexTransform;
	uint iCustomProperty;
	
	float3 _objectPad;

    // remember to add padding to 4 floats
};

struct VertexIn
{
    float3 vPos   : POSITION;
    float3 vNormal: NORMAL;
    float2 vUV    : UV;
	float4 vCustomVec4 : CUSTOM;
};

struct VertexOut
{
    float4 vPosViewSpace  : SV_POSITION;
	float4 vPosWorldSpace : POSITION;
    float3 vNormal : NORMAL;
    float2 vUV     : UV;
	float4 vCustomVec4 : CUSTOM;
};

VertexOut vsDefault(VertexIn vertexIn)
{
    VertexOut vertexOut;

    vertexOut.vCustomVec4 = vertexIn.vCustomVec4;

    // Apply world matrix.
    vertexOut.vPosWorldSpace = mul(float4(vertexIn.vPos, 1.0f), vWorld);
    vertexOut.vNormal = mul(vertexIn.vNormal, (float3x3)vWorld);

    // Transform to homogeneous clip space.
    vertexOut.vPosViewSpace = mul(vertexOut.vPosWorldSpace, mViewProjection);

    return vertexOut;
}

float4 psDefault(VertexOut pin) : SV_Target
{
    float4 vDiffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);

#ifdef USE_DIFFUSE_TEXTURE
    // Apply texture filtering.
    #ifdef TEXTURE_FILTERING_POINT
        vDiffuse = diffuseTexture.Sample(samplerPointWrap, pin.vUV);
    #elif TEXTURE_FILTERING_LINEAR
        vDiffuse = diffuseTexture.Sample(samplerLinearWrap, pin.vUV);
    #elif TEXTURE_FILTERING_ANISOTROPIC
        vDiffuse = diffuseTexture.Sample(samplerAnisotropicWrap, pin.vUV);
    #endif
#endif

    // TODO: should we filter other textures?

    return vDiffuse;
}