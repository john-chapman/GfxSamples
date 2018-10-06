#include "shaders/def.glsl" // framework common

#ifdef VERTEX_SHADER
	#define ATTRIBUTE(_location, _type, _name) layout(location=_location) in _type _name;
	#define VARYING(_interp, _type, _name) _interp out _type _name
#endif

#ifdef FRAGMENT_SHADER
	#define ATTRIBUTE(_location, _type, _name) layout(location=_location) in _type _name;
	#define VARYING(_interp, _type, _name) _interp in _type _name
#endif
