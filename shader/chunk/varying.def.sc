vec3 a_position     : POSITION;
vec2 a_texcoord0    : TEXCOORD0; // Base UV
vec2 a_texcoord1    : TEXCOORD1; // Overlay UV
float a_texcoord2   : TEXCOORD2; // AO weight

vec2 v_texcoord0    : TEXCOORD0; // Base UV
vec2 v_texcoord1    : TEXCOORD1; // Overlay UV
float v_ao          : TEXCOORD2; // AO weight