#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec3 iResolution;
    float iTime;
    float iTimeDelta;
    float iFrameRate;
    uint iFrame;
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
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = (2.*fragCoord - iResolution.xy) /iResolution.y;
    
    float tr = iTime * 0.1;
    
    vec2 cs = vec2(cos(tr), sin(tr));
    mat2 rot = mat2(vec2(cs.x, -cs.y), cs.yx);
    
    uv *= rot;

    float d0 = length(uv);
    
    vec3 mc = 0.5 + 0.5*cos(3.*d0-iTime*1.5+vec3(0,2,4));
    
    float t = cos(d0-iTime) * 0.5 + 0.5;
    
    float d = sdFlowerOfLife(uv, mix(0.25, 1.25, t), t);
    
    vec3 col = (d>0.0) ? vec3(1.) : vec3(1.);
    //vec3 col = (d<0.0) ? vec3(0.5) : vec3(1.0);
    col *= 1.0 - exp(-50.*abs(d));
	col = mix( col, mc, 1.0-step(mix(0.01, 0.02, t), abs(d)) );

    // Output to screen
    fragColor = vec4(col,1.0);
}

void main() {
    vec2 fragCoord = vec2(gl_FragCoord.x, iResolution.y-gl_FragCoord.y);

    vec4 fragColor = vec4(0.);

    mainImage(fragColor, fragCoord);

    outColor = fragColor;
}