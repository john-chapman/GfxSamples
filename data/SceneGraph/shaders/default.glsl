#include "shaders/common.glsl"
#include "shaders/GBuffer.glsl"
#include "shaders/Camera.glsl"

ATTRIBUTE(0, vec3, aPosition);
ATTRIBUTE(1, vec3, aNormal);
ATTRIBUTE(2, vec3, aTangent);
ATTRIBUTE(3, vec2, aTexcoord);

#if PASS_gbuffer
	VARYING(smooth, vec2, vUv);
	VARYING(smooth, vec3, vNormalV);
	VARYING(smooth, vec3, vTangentV);
	VARYING(smooth, vec3, vBitangentV);
#endif

#ifdef VERTEX_SHADER
	uniform mat4 uWorld;

	void main()
	{
		#if PASS_gbuffer
		{
			vUv         = aTexcoord;
			vNormalV    = TransformDirection(bfCamera.m_view, TransformDirection(uWorld, aNormal));
			vTangentV   = TransformDirection(bfCamera.m_view, TransformDirection(uWorld, aTangent));
			vBitangentV = cross(vNormalV, vTangentV);
		}
		#endif

		vec3 posW = TransformPosition(uWorld, aPosition);
		gl_Position = bfCamera.m_viewProj * vec4(posW, 1.0);
	}

#endif

#ifdef FRAGMENT_SHADER

	#if PASS_gbuffer
		uniform sampler2D txAlbedo;
		uniform sampler2D txRough;
		uniform sampler2D txNormal;
	#endif

	void main()
	{
		#if PASS_gbuffer
		{
			GBuffer_ExportAlbedo(texture(txAlbedo, vUv).rgb);
			GBuffer_ExportRoughnessMetalAo(texture(txRough, vUv).x, 0.0, 1.0);
			vec3 normal = texture(txNormal, vUv).xyz * 2.0 - 1.0;
			normal = normalize(vTangentV) * normal.x + normalize(vBitangentV) * normal.y + normalize(vNormalV) * normal.z;
			GBuffer_ExportNormal(normal);
		}
		#endif

		#if PASS_shadow
		{
		}
		#endif
	}

#endif