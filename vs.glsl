#version 120
varying vec2 tc;

void main()
{
        tc = gl_MultiTexCoord0.st;
        gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}

