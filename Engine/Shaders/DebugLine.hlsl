// DebugLine.hlsl — vertex + pixel shader for debug primitive (line) rendering.
// Coordinate system: world or screen space (top-left origin, pixels),
// identical to Sprite.hlsl. World lines bind the camera CB region;
// screen-space lines bind the identity region.

cbuffer FrameCB : register(b0) {
    float2 screenSize;
    float2 camScale;   // uniform zoom (x == y); float2 keeps the multiply simple
    float2 camOffset;
    float2 _pad;
};

struct VSInput {
    float2 position : POSITION;
    float4 color    : COLOR0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float4 color    : COLOR0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    // View transform: world → screen (identity for screen-space lines).
    float2 p = input.position * camScale + camOffset;
    // Screen [0,w]x[0,h] → NDC [-1,1]x[1,-1]
    output.position = float4(
        (p.x / screenSize.x) * 2.0 - 1.0,
        1.0 - (p.y / screenSize.y) * 2.0,
        0.0, 1.0);
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
}
