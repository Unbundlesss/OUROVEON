
#include "lib/all.glsl"

// https://www.shadertoy.com/view/ss2Szm
// https://www.shadertoy.com/user/mrange

// License CC0: Sunday threads
// Result after a bit of random coding on sunday afternoon

#define PI              3.141592654
#define TAU             (2.0*PI)
#define TIME            iTime
#define RESOLUTION      iResolution
#define ROT(a)          mat2(cos(a), sin(a), -sin(a), cos(a))
#define PCOS(a)         (0.5+0.5*cos(a))

#define TOLERANCE       0.00001
#define MAX_RAY_LENGTH  10.0
#define MAX_RAY_MARCHES 64
#define NORM_OFF        0.001

#define PATHA vec2(0.1147, 0.2093)
#define PATHB vec2(13.0, 3.0)

const mat2 rot0             = ROT(0.0);
const vec3 std_gamma        = vec3(2.2);

mat2  g_rot  = rot0;
float g_hit  = 0.0;

float hash(float x) {
  return fract(sin(x*12.9898) * 13758.5453);
}

// From https://www.shadertoy.com/view/XdcfR8
vec3 cam_path(float z) {
  return vec3(sin(z*PATHA)*PATHB, z);
}

vec3 dcam_path(float z) {
  return vec3(PATHA*PATHB*cos(PATHA*z), 1.0);
}

vec3 ddcam_path(float z) {
  return vec3(-PATHA*PATHA*PATHB*sin(PATHA*z), 0.0);
}

float tanh_approx(float x) {
//  return tanh(x);
  float x2 = x*x;
  return clamp(x*(27.0 + x2)/(27.0+9.0*x2), -1.0, 1.0);
}

// From: http://mercury.sexy/hg_sdf/
vec2 mod2(inout vec2 p, vec2 size) {
  vec2 c = floor((p + size*0.5)/size);
  p = mod(p + size*0.5,size) - size*0.5;
  return c;
}

// From: https://stackoverflow.com/a/17897228/418488
vec3 hsv2rgb(vec3 c) {
  const vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 postProcess(vec3 col, vec2 q) {
  col = clamp(col, 0.0, 1.0);
  col = pow(col, 1.0/std_gamma);
  col = col*0.6+0.4*col*col*(3.0-2.0*col);
  col = mix(col, vec3(dot(col, vec3(0.33))), -0.4);
  col *=0.5+0.5*pow(19.0*q.x*q.y*(1.0-q.x)*(1.0-q.y),0.7);
  return col;
}

float df(vec3 p) {
  vec3 cam = cam_path(p.z);
  vec3 dcam = normalize(dcam_path(p.z));
  p.xy -= cam.xy;
  p -= dcam*dot(vec3(p.xy, 0), dcam)*0.5*vec3(1,1,-1);

  float dc = length(p.xy) - 0.5;
  vec2 p2 = p.xy;
  mat2 rr = ROT(p.z*0.5);
  p2      *= rr;
  rr      *= g_rot;
  
  float d = 1E6;

  const float ss = 0.45;
  const float oo = 0.125;
  float s = 1.0;

  vec2 np = mod2(p2, vec2(0.75));
  float hit = 0.0;
  float qs  = 0.5; 
  const int max_iter = 3;
  for (int i = 0; i < max_iter; ++i) {
    vec2 sp2 = sign(p2);
    hit += qs*(3.0 + sp2.x + 2.0*sp2.y)/8.0;
    p2 = abs(p2);
    p2 -= oo*s;
    float dp = length(p2) - 0.75*ss*oo*s;
//    d = max(d, -(dp-0.1*s));
    if (dp < d) {
      d = dp;
      g_hit = hit+np.x+10.0*np.y;
    }
    s  *= ss;
    rr  = transpose(rr);
    p2 *= rr;
    qs *= 0.5;
  }
  return max(d, -dc);
}

float rayMarch(vec3 ro, vec3 rd, out int iter) {
  float t = 0.0;
  int i = 0;
  for (i = 0; i < MAX_RAY_MARCHES; i++) {
    float d = df(ro + rd*t);
    if (d < TOLERANCE || t > MAX_RAY_LENGTH) break;
    t += d;
  }
  iter = i;
  return t;
}

vec3 normal(vec3 pos) {
  vec2  eps = vec2(NORM_OFF,0.0);
  vec3 nor;
  nor.x = df(pos+eps.xyy) - df(pos-eps.xyy);
  nor.y = df(pos+eps.yxy) - df(pos-eps.yxy);
  nor.z = df(pos+eps.yyx) - df(pos-eps.yyx);
  return normalize(nor);
}

float softShadow(vec3 pos, vec3 ld, float ll, float mint, float k) {
  const float minShadow = 0.25;
  float res = 1.0;
  float t = mint;
  for (int i=0; i<24; i++) {
    float d = df(pos + ld*t);
    res = min(res, k*d/t);
    if (ll <= t) break;
    if(res <= minShadow) break;
    t += max(mint*0.2, d);
  }
  return clamp(res,minShadow,1.0);
}

vec3 render(vec3 ro, vec3 rd) {
  vec3 lightPos = cam_path(ro.z+0.5);
  float alpha   = 0.05*TIME;
  
  int iter    = 0;
  g_hit       = 0.0;
  float t     = rayMarch(ro, rd, iter);
  float hit   = g_hit;
  float r     = hash(hit+123.4);

  if (t >= MAX_RAY_LENGTH) {
    return vec3(0.0);
  }

  vec3 pos    = ro + t*rd;
  vec3 nor    = normal(pos);
  vec3 refl   = reflect(rd, nor);

  float tzp =  ( clamp( t - 0.5, 0.0, MAX_RAY_LENGTH ) * 0.1 );
  vec4 audio1 = texture(iAudio, vec2(r, tzp * (0.1 + r * 0.1) ) );
  vec4 audio3 = texture(iAudio, vec2(r, 1.0 - tzp));
  vec4 audio2 = texture(iAudio, vec2(r, 1.0 ));

  float blendx = audio1.x;
  float blendy = audio1.y;


  float ifade= 1.0-tanh_approx(1.25*float(iter)/float(MAX_RAY_MARCHES));
  float aa   = (blendx * blendx);// + (10.0*pos.z-6.0*TIME*fract(113.0*r)) + (audio2.x * ( r > 0.5 ? 1.0 : -2.0 ) );
  float band = audio1.x + smoothstep(0.95, 1.0, blendx ) * 3.0;
  vec3 hsv   = (vec3(fract(-0.25+0.15*r+0.025*pos.z), (1.0-ifade), mix(0.02 + (r * iBeat.x * iBeat.x * iBeat.x) * 2.8 * ifade, 1.0 + (blendy * 5.0 * ifade), band * blendy )));
  vec3 color = hsv2rgb(hsv);

  vec3 lv   = lightPos - pos;
  float ll2 = dot(lv, lv);
  float ll  = sqrt(ll2);
  vec3 ld   = lv / ll;
  float sha = softShadow(pos, ld, ll*0.95, 0.01, 24.0);

  float dm  = .5/ll2;
  float dif = max(dot(nor,ld),0.0)*(dm+0.05);
  float spe = pow(max(dot(refl, ld), 0.), 20.);
  float l   = dif*sha;

  vec3 col = l*color + spe*sha;
  col *= 0.8;


  hsv.g = 1.0;
  hsv.b = 3.0 * audio3.z * audio3.z * audio3.z;
  col += hsv2rgb(hsv) * ( smoothstep(0.0, 0.3, l) );

  return col*ifade;
}

vec3 effect3d(vec2 p, vec2 q) {
  float z   = TIME;
  g_rot     = ROT(0.3*TIME); 
  vec3 cam  = cam_path(z);
  vec3 dcam = dcam_path(z);
  vec3 ddcam= ddcam_path(z);
  
  vec3 ro = cam;
  vec3 ww = normalize(dcam);
  vec3 uu = normalize(cross(vec3(0.0,1.0,0.0)+ddcam*2.0, ww ));
  vec3 vv = normalize(cross(ww,uu));
  vec3 rd = normalize( p.x*uu + p.y*vv + 2.5*ww );

  return render(ro, rd);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  vec2 q = fragCoord/RESOLUTION.xy;
  vec2 p = -1. + 2. * q;
  p.x *= RESOLUTION.x/RESOLUTION.y;

  vec3 col = effect3d(p, q);

  col = postProcess(col, q);

  fragColor = vec4(col, 1.0);
}

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return (x*(a*x+b))/(x*(c*x+d)+e);
}

// --------------------------------------------------------------------------------------------
void main(void)
{
    vec2 sampleUV = oUV;
    vec2 cUV = ( (sampleUV) * iResolution.xy ) / max( iResolution.x, iResolution.y );

    vec4 resultColour;
    mainImage( resultColour, (sampleUV) * iResolution.xy );

    float grain = filmGrain( oUV, 1.0f, iTime ) * 24.0;

    vec4 inputCol = texture(iInputBufferA, inputBufferUVMap(oUV));


    glOutColour = vec4( ACESFilm( resultColour.rgb * (1.0 + grain) ) , 1.0 );
}
