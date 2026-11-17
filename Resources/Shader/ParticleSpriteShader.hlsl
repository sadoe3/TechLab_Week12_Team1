#pragma pack_matrix(row_major)

cbuffer constants : register(b0)
{
    matrix VP;
};

// 파티클 정점은 이미 월드 좌표라 World가 필요 없어, 이 칸에 카메라 축(BuildBillboardMatrix)을 받는다
// World[1] = 화면 오른쪽, World[2] = 화면 위쪽
cbuffer Worldconstants : register(b2)
{
    matrix World;
};

Texture2D SpriteTexture : register(t0);
SamplerState SpriteSampler : register(s0);

// Vertex.h의 FParticleSpriteVertex와 필드 순서·크기가 같아야 한다 (Input Layout을 리플렉션으로 만든다)
struct VS_INPUT
{
    float3 position : POSITION;   // 0
    float  rotation : TEXCOORD0;  // 12
    float2 size     : TEXCOORD1;  // 16
    float2 uv       : TEXCOORD2;  // 24
    float4 color    : COLOR;      // 32
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output = (PS_INPUT)0;
    // 코너 UV로 카메라 Right/Up 방향으로 펼치고 VP로 변환한다
    
    float U = input.uv.x - 0.5f;
    float V = 0.5f - input.uv.y;
    
    float3 Right = World[1].xyz;
    float3 Up = World[2].xyz;
    
    float3 X = (U * Right) * input.size.x;
    float3 Y = (V * Up) * input.size.y;
    float3 P = input.position + X + Y;
    
    output.position = mul(float4(P, 1), VP);
    output.color = input.color;
    output.uv = input.uv;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // 텍스처 × 파티클 색. 가장자리 알파는 버리지 않고 블렌드로 섞는다
    float4 C = SpriteTexture.Sample(SpriteSampler, input.uv);
    C *= input.color;
    return C;
}
