#include "../common.sh"

// Per-entity color ID, set once per draw by the EntityPicker. Written verbatim
// (no gamma/tonemap) into the RGBA8 pick target so the CPU reads back the exact
// bytes it encoded.
uniform vec4 u_id;

void main()
{
	gl_FragColor = u_id;
}