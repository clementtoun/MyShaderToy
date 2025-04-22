#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec3 iResolution;
    float iTime;
    float iTimeDelta;
    float iFrameRate;
    int iFrame;
    float padding0;       // alignement pour vec4
    vec4 iMouse;
    vec4 iDate;
};

float inv_smoothstep( float x )
{
  float a = pow(    x,1.0/3.0);
  float b = pow(1.0-x,1.0/3.0);
  return a/(a+b);
}

float sdFlowerOfLife(vec2 p, float r, float t)
{
    vec4 cstH = vec4(1.73205080757, 0.86602540378, -0.5, 0.) * r; 
    
    vec2 s = vec2(cstH.x, r);
    
    p = p - s*round(p/s);
    
    p = abs(p);
    
    float d = length(p - cstH.yz);
    
    d = max(d, length(p - vec2(0., r)));
    
    d = min(d, length(p - cstH.xw));
    
    d = min(d, length(p + cstH.yz));
    
    return d - r + mix(0.08, -0.09, inv_smoothstep(t));
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    vec3 col = vec3(0.);

    //Draw a red cross where the mouse button was last down.
    if(abs(iMouse.x-fragCoord.x) < 4.) {
        col = vec3(1.,0.,0.);
    }
    if(abs(iMouse.y-fragCoord.y) < 4.) {
        col = vec3(1.,0.,0.);
    }
    
    //If the button is currently up, (iMouse.z, iMouse.w) is where the mouse
    //was when the button last went down.
    if(abs(iMouse.z-fragCoord.x) < 2.) {
        col = vec3(0.,0.,1.);
    }
    if(abs(iMouse.w-fragCoord.y) < 2.) {
        col = vec3(0.,0.,1.);
    }
    
    //If the button is currently down, (-iMouse.z, -iMouse.w) is where
    //the button was when the click occurred.
    if(abs(-iMouse.z-fragCoord.x) < 2.) {
        col = vec3(0.,1.,0.);
    }
    if(abs(-iMouse.w-fragCoord.y) < 2.) {
        col = vec3(0.,1.,0.);
    }
    
    fragColor = vec4(col, 1.0);
}

void main() {
    vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y-gl_FragCoord.y);

    vec4 fragColor = vec4(0.);

    mainImage(fragColor, fragCoord);

    outColor = fragColor;
}