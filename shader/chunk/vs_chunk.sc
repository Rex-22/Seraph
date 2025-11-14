$input a_position, a_texcoord0, a_texcoord1, a_texcoord2
$output v_texcoord0, v_texcoord1, v_ao

#include "../common.sh"

void main()
{
    v_texcoord0 = a_texcoord0;
    v_texcoord1 = a_texcoord1;
    v_ao = a_texcoord2;

    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );
}