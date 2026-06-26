// Sprite.hlsl — vertex + pixel shader for 2D sprite rendering.
// Coordinate system: screen space (top-left origin, pixels).

cbuffer ScreenCB : register(b0) {
    float2 screenSize;
    float2 _pad;
};

Texture2D    spriteTex     : register(t0);
SamplerState spriteSampler : register(s0);

struct VSInput {
    float2 position : POSITION;
    float2 uv       : TEXCOORD0;
};

struct PSInput {
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

PSInput VSMain(VSInput input) {
    PSInput output;
    // Screen [0,w]x[0,h] → NDC [-1,1]x[1,-1]
    output.position = float4(
        (input.position.x / screenSize.x) * 2.0 - 1.0,
        1.0 - (input.position.y / screenSize.y) * 2.0,
        0.0, 1.0);
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET {
    return spriteTex.Sample(spriteSampler, input.uv);
}
