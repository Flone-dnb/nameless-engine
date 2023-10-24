/** Stores frame-global constants. */
struct FrameData
{
    /** Camera's view matrix multiplied by camera's projection matrix. */
    float4x4 viewProjectionMatrix;

    /** Camera's world location. */
    float3 cameraPosition;

    /** Time that has passed since the last frame in seconds (i.e. delta time). */
    float timeSincePrevFrameInSec;

    /** Time since the first window was created (in seconds). */
    float totalTimeInSec;
};
ConstantBuffer<FrameData> frameData : register(b0, space5);

/** Describes vertex shader input data. */
struct VertexIn
{
    float3 localPosition : POSITION;    // position in local space
    float3 localNormal   : NORMAL;      // normal in local space
    float2 uv            : UV;
};

/** Describes pixel shader input data. */
struct VertexOut
{
    float4 viewPosition            : SV_POSITION; // position in view space
    float4 worldPosition           : POSITION;    // position in world space
    float3 worldNormal             : NORMAL;      // normal in world space
    float2 uv                      : UV;
};
