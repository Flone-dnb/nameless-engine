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

    float2 _pad;

    // remember to add padding to 4 floats
}

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

VertexOut VS(VertexIn vertexIn)
{
    return vertexIn;
}

float4 PS(VertexOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}