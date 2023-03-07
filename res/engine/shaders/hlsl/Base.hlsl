cbuffer frameData : register(b0, space5)
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
