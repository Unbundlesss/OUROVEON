in vec2 iPosition;
in vec2 iTexcoord;

out vec2 oUV;

void main(void)
{
    gl_Position = vec4(iPosition, 0, 1);
    oUV = iTexcoord;
}