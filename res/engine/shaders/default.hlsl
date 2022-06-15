SamplerState samplerPointWrap       : register(s0);
SamplerState samplerLinearWrap      : register(s1);
SamplerState samplerAnisotropicWrap : register(s2);
SamplerComparisonState samplerShadowMap : register(s3);

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
    return float4(1.0f, 0.0f, 0.0f, 1.0f);
}