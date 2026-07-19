#ifndef __SERAPH_LIGHTS_SH__
#define __SERAPH_LIGHTS_SH__

// Per-frame light + camera uniforms staged by SceneRenderer::UploadLightUniforms.
// MAX_LIGHTS MUST match Seraph::c_MaxLights (Graphics/SceneRenderer.h): the arrays
// are created and sized to it on the C++ side.
//
// Packing (all vec4):
//   u_lightCount.x              active light count (loop bound)
//   u_ambient.rgb               flat ambient radiance (color * intensity)
//   u_cameraPos.xyz             camera world position
//   u_lightPosRange[i]          xyz world position, w range (point/spot)
//   u_lightColorIntensity[i]    rgb color, w intensity
//   u_lightDirType[i]           xyz direction of travel, w type (0 dir/1 point/2 spot)
//   u_lightSpot[i]              x spotScale, y spotOffset (cone: saturate(cosA*x + y))

#define MAX_LIGHTS 16

uniform vec4 u_lightCount;
uniform vec4 u_ambient;
uniform vec4 u_cameraPos;
uniform vec4 u_lightPosRange[MAX_LIGHTS];
uniform vec4 u_lightColorIntensity[MAX_LIGHTS];
uniform vec4 u_lightDirType[MAX_LIGHTS];
uniform vec4 u_lightSpot[MAX_LIGHTS];

#endif // __SERAPH_LIGHTS_SH__
