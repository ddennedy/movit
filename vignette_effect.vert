uniform vec2 PREFIX(aspect_correction);
varying vec2 PREFIX(normalized_pos);

vec2 FUNCNAME()
{
	vec2 temp = LAST_INPUT();
	PREFIX(normalized_pos) = (temp - PREFIX(center)) * PREFIX(aspect_correction);
	return temp;
}
