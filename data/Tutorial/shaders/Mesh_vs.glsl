#include "shaders/def.glsl"    // common helper functions and macros
#include "shaders/Camera.glsl" // defines cbCamera and Camera_ helper functions

layout(location=0) in vec3  aPosition;
layout(location=1) in vec3  aNormal;
layout(location=2) in vec3  aTangent;
layout(location=3) in vec2  aTexcoord;

uniform mat4 uWorldMatrix;

smooth out vec2 vUv;
smooth out vec3 vNormalW;
smooth out vec3 vPositionW;

void main() 
{
	vUv         = aTexcoord;
	vPositionW  = TransformPosition(uWorldMatrix, aPosition); 
	vNormalW    = TransformDirection(uWorldMatrix, aNormal);
	gl_Position = bfCamera.m_viewProj * vec4(vPositionW, 1.0);
}
