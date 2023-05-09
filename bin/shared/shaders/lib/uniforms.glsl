
uniform vec2        iViewportUV;
uniform vec2        iViewportOffset;

uniform vec2        iResolution;
uniform float       iTime;

uniform vec4        iBeat;
uniform vec4        iRiff;
uniform sampler2D   iAudio;

uniform sampler2D   iInputBufferA;


in vec2 oUV;

layout (location = 0) out vec4 glOutColour;
