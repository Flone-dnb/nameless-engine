/** Describes vertex shader input data. */
struct VertexIn {
    /** Position in local space. */
    float3 localPosition : POSITION;
    
    /** Normal in local space. */
    float3 localNormal   : NORMAL;
    
    /** UVs. */
    float2 uv            : UV;
};

/** Describes pixel shader input data. */
struct VertexOut {
    /**
     * In vertex shader: it should be position in clip space.
     * In pixel shader: it's position in screen space.
     */
    float4 position       : SV_POSITION;
    
    /** Position in world space. */
    float4 worldPosition  : POSITION;
    
    /** Normal in world space. */
    float3 worldNormal    : NORMAL;
    
    /** UVs. */
    float2 uv             : UV;
};