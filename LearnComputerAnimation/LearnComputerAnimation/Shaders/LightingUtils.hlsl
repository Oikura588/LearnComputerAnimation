struct Light
{
	float3 Strength;
	float FalloffStart;
	float3 Direction;
	float FalloffEnd;
	float3 Position;
	float SpotPower;
};

struct Material
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	
	float Shininess;
};

float CalcAttenuation(float d,float falloffStart,float falloffEnd)
{
	// 物理上的衰减与距离的平方成反比，这里是近似的线性衰减.
	return saturate((falloffEnd - d) / (falloffEnd - falloffEnd));
}

// 近似菲涅尔
// 参见 RTR3 第233页
// R0 = ((n-1)/(n+1))^2 n为折射率
float3 Fresnel(float3 R0,float3 normal,float3 lightVector)
{
	float cosIncidentAngle = saturate(dot(normal, lightVector));
	
	float f0 = 1.0f - cosIncidentAngle;
	float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);
	return reflectPercent;
}

// 计算光量，漫反射光量与镜面反射光亮的总和
float3 BlinnPhong(float3 lightStrength,float3 lightVec,float3 normal,float3 toEye,Material mat)
{
	// m由光泽度推导而来
	const float m = mat.Shininess * 256.f;
	float3 halfVec = normalize(toEye + lightVec);
	
	float roughnessFactor = (m + 8.0f) / 8.0f * pow(max(dot(halfVec, normal), 0.0f), m);
	float3 FresnelFactor = Fresnel(mat.FresnelR0, normal, lightVec);
	
	float3 specAlbedo = FresnelFactor * roughnessFactor;
	
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);
	return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

// 方向光
float3 ComputeDirectionalLight(Light L,Material mat,float3 normal,float3 toEye)
{
	float3 lightVec = -L.Direction;
	// 方向光的光强度直接由朗伯余弦计算
	float ndotl = max(dot(normal, lightVec), 0.0);
	float3 lightStrength = L.Strength * ndotl;
	
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

// 点光源
float3 ComputePointLight(Light L,Material mat,float3 pos, float3 normal,float3 toEye)
{
	float3 lightVec = L.Position - pos;
	float d = length(lightVec);
	if(d>L.FalloffEnd)
		return 0.0f;
	// 光向量规范化
	lightVec /= d;
	// 光强度由朗伯余弦计算
	float ndotl = max(dot(normal, lightVec), 0.0);
	float3 lightStrength = L.Strength * ndotl;
	// 光强的衰减
	float add = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= add;
	
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

// 聚光灯
float3 ComputeSpotLight(Light L,Material mat,float3 pos,float3 normal,float3 toEye)
{
	float3 lightVec = L.Position - pos;
	float d = length(lightVec);
	
	if(d>L.FalloffEnd)
		return 0.f;
	lightVec /= d;
	// 光强度由朗伯余弦计算
	float ndotl = max(dot(normal, lightVec), 0.0);
	float3 lightStrength = L.Strength * ndotl;
	// 距离衰减
	float add = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= add;
	// 聚光灯衰减
	float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
	lightStrength *= spotFactor;
	
	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}


// 计算多个光源

#define MaxLights 16

float4 ComputeLighting(
	Light gLights[MaxLights],Material mat,float3 pos,float3 normal,float3 toEye,float3 shadowFactor)
{
	float3 result = 0.0f;
	int i = 0;
#if(NUM_DIR_LIGHTS>0)
	for(i=0;i<NUM_DIR_LIGHTS;++i)
	{
		result+=ComputeDirectionalLight(gLights[i],mat,normal,toEye);
	}
#endif
#if(NUM_POINT_LIGHTS>0)
	for(i=NUM_DIR_LIGHTS;i<NUM_DIR_LIGHTS+NUM_POINT_LIGHTS;++i)
	{
		result+=ComputePointLight(gLights[i],mat,pos,normal,toEye);
	}
#endif
#if(NUM_SPOT_LIGHTS>0)
	for(i=NUM_DIR_LIGHTS+NUM_POINT_LIGHTS;i<NUM_DIR_LIGHTS+NUM_POINT_LIGHTS+NUM_SPOT_LIGHTS;++i)
	{
		result+=ComputeSpotLight(gLights[i],mat,pos,normal,toEye);
	}
#endif
	return float4(result, 0.0f);
}