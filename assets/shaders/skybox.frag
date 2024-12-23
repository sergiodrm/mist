#version 460

layout (location = 0) in vec3 inTexCoords;
layout (location = 1) in mat4 inView;

#if !defined(SKYBOX_GBUFFER)
layout (location = 0) out vec4 outFragColor;
#else
layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out vec4 outEmissive;
#endif

layout (set = 1, binding = 0) uniform samplerCube u_cubemap;

vec3 Sky(in vec3 ro, in vec3 rd)
{
    const float SC = 1e5;

 	// Calculate sky plane
    float dist = (SC - ro.y) / rd.y; 
    vec2 p = (ro + dist * rd).xz;
    p *= 1.2 / SC;
    
    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    vec3 lightDir = normalize(vec3(-.8, .15, -.3));
    float sundot = clamp(dot(rd, lightDir), 0.0, 1.0);
    
    vec3 cloudCol = vec3(1.);
    //vec3 skyCol = vec3(.6, .71, .85) - rd.y * .2 * vec3(1., .5, 1.) + .15 * .5;
    vec3 skyCol = vec3(0.3,0.5,0.85) - rd.y*rd.y*0.5;
    skyCol = mix( skyCol, 0.85 * vec3(0.7,0.75,0.85), pow( 1.0 - max(rd.y, 0.0), 4.0 ) );
    
    // sun
    vec3 sun = 0.25 * vec3(1.0,0.7,0.4) * pow( sundot,5.0 );
    sun += 0.25 * vec3(1.0,0.8,0.6) * pow( sundot,64.0 );
    sun += 0.2 * vec3(1.0,0.8,0.6) * pow( sundot,512.0 );
    skyCol += sun;
    
    // clouds
    //float t = iTime * 0.1;
    //float den = fbm(vec2(p.x - t, p.y - t));
    //skyCol = mix( skyCol, cloudCol, smoothstep(.4, .8, den));
    
    // horizon
    skyCol = mix( skyCol, 0.68 * vec3(.418, .394, .372), pow( 1.0 - max(rd.y, 0.0), 16.0 ) );
    
    return skyCol;
}

void main()
{
#if !defined(SKYBOX_GBUFFER)
    outFragColor = texture(u_cubemap, inTexCoords);
#else
    outAlbedo = texture(u_cubemap, inTexCoords);
    //outEmissive = 0.5f*texture(u_cubemap, inTexCoords);
#endif

#if 0
    vec2 res = vec2(1920.f, 1080.f);
    vec2 uv = (gl_FragCoord.xy) / res;
    uv.y = 1.f - uv.y;
    uv -= 0.5f;
    uv.x *= res.x/res.y;
    vec3 r = normalize(mat3(inView) * vec3(uv, 1.f));
    vec3 skycol = Sky(vec3(0.f), r);
    outFragColor = vec4(skycol, 1.f);
#endif
}