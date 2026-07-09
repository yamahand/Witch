// Sprite.hlsl — vertex + pixel shader for 2D sprite rendering.
// Coordinate system: world or screen space (top-left origin, pixels).
// World sprites bind the camera CB region (camScale/camOffset = view transform);
// screen-space (HUD) sprites bind the identity region (scale=1, offset=0).

cbuffer FrameCB : register(b0) {
    float2 screenSize;
    float2 camScale;   // uniform zoom (x == y); float2 keeps the multiply simple
    float2 camOffset;
    float2 _pad;
};

Texture2D    spriteTex     : register(t0);
SamplerState spriteSampler : register(s0);

struct VSInput {
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    // View transform: world → screen (identity for HUD sprites).
    float2 p = input.position * camScale + camOffset;
    // Screen [0,w]x[0,h] → NDC [-1,1]x[1,-1]
    output.position = float4(
        (p.x / screenSize.x) * 2.0 - 1.0,
        1.0 - (p.y / screenSize.y) * 2.0,
        0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    // Tint + alpha: vertex color multiplies the texel (white = unmodified).
    return spriteTex.Sample(spriteSampler, input.uv) * input.color;
}
