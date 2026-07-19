#include "../common.sh"

// Depth-only pass: the framebuffer has no color attachment, so nothing is
// written here — only the fixed-function depth. Kept minimal.
void main()
{
	gl_FragColor = vec4_splat(0.0);
}
