#pragma pack_matrix(row_major)

struct OBJ_ATTRIBUTES
{
    float3 Kd; float d;
    float3 Ks; float Ns;
    float3 Ka; float sharpness;
    float3 Tf; float Ni;
    float3 Ke; float illum;
};

struct SHADER_MODEL_DATA { float4x4 worldMatrix; OBJ_ATTRIBUTES material; };
StructuredBuffer<SHADER_MODEL_DATA> SceneData : register(b1);

cbuffer SHADER_SCENE_DATA : register(b0)
{
    float4 sunDirection, sunColor, sunAmbient, camPos;
    float4x4 viewMatrix, projectionMatrix;
};

struct V_OUT
{
    float4 pos : SV_POSITION;
    float3 nrm : NORMAL;
    float3 posW : WORLD;
    float3 uvw : TEXCOORD;
    nointerpolation uint index : INDEX;
};

V_OUT main(float3 pos : POSITION, float3 uvw : TEXCOORD,
           float3 nrm : NORMAL, uint index : SV_InstanceID)
{
    float4 posW = mul(float4(pos, 1), SceneData[index].worldMatrix);
    float4 posH = mul(mul(posW, viewMatrix), projectionMatrix);
    V_OUT output = { posH, mul(nrm, SceneData[index].worldMatrix).xyz, posW.xyz, uvw, index };
    return output;
}
