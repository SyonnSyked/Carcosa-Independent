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

float4 main(float4 pos : SV_POSITION, float3 nrm : NORMAL,
            float3 posW : WORLD, float3 uvw : TEXCOORD, uint index : INDEX) : SV_TARGET
{
    float4 diffuse = float4(SceneData[index].material.Kd, 1);
    float4 specular = float4(SceneData[index].material.Ks, 1);
    float4 ambient = float4(SceneData[index].material.Ka, 1);
    float4 emissive = float4(SceneData[index].material.Ke, 1);
    float lightRatio = saturate(dot(-sunDirection, normalize(nrm)));
    float3 viewDir = normalize(camPos.xyz - posW);
    float3 bounce = reflect(sunDirection, normalize(nrm));
    float intense = max(pow(saturate(dot(viewDir, bounce)), SceneData[index].material.Ns), 0);
    float4 reflected = sunColor * specular * intense;
    return saturate(lightRatio * sunColor + sunAmbient * ambient) * diffuse + reflected + emissive;
}
