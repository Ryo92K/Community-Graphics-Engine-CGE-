cbuffer Controls:register(b0) {
    float nearPlane,farPlane,strength,radius;
    float bias,power,maxDistance,padding;
    float4 rays;
    uint2 sceneSize,aoSize;
    uint directions,steps;
    int2 axis;
};
Texture2D<float> depth:register(t1);
Texture2D<float4> geometry:register(t2);
Texture2D<float> visibility:register(t3);
Texture2D<float4> cachedGuide:register(t4);
Texture2D<float> linearDepth:register(t5);
float4 SsaoVertex(uint id:SV_VertexID):SV_Position {
    float2 uv=float2((id<<1)&2,id&2);
    return float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
int2 Bounded(int2 p) {return clamp(p,int2(0,0),int2(sceneSize)-1);}
float LinearDepth(int2 p) {
    float z=depth.Load(int3(Bounded(p),0));
    if(!isfinite(z) || z<0 || z>=1)return 0;
    return nearPlane/(farPlane*(1-z)+nearPlane*z)*farPlane;
}
float3 Position(int2 p,float z) {
    float2 uv=(float2(p)+0.5)/float2(sceneSize);
    return float3((rays.xy+uv*rays.zw)*z,z);
}
float3 Derivative(int2 p,float3 center,int2 direction) {
    int2 plus=Bounded(p+direction),minus=Bounded(p-direction);
    float zp=LinearDepth(plus),zm=LinearDepth(minus);
    float3 dp=Position(plus,zp)-center,dm=center-Position(minus,zm);
    float eplus=zp>0?abs(zp-center.z):1e30;
    float eminus=zm>0?abs(zm-center.z):1e30;
    if(all(plus==p))eplus=1e30;
    if(all(minus==p))eminus=1e30;
    return eplus<eminus?dp:dm;
}
struct GeometryOutput {float4 surface:SV_Target0;float linearZ:SV_Target1;};
GeometryOutput GeometryPixel(float4 position:SV_Position) {
    GeometryOutput output=(GeometryOutput)0;
    int2 p=int2(position.xy);
    float z=LinearDepth(p);
    if(z<=0 || z>=maxDistance)return output;
    float3 center=Position(p,z);
    float3 normal=cross(Derivative(p,center,int2(1,0)),Derivative(p,center,int2(0,1)));
    float length2=dot(normal,normal);
    normal=length2>1e-16?normal*rsqrt(length2):float3(0,0,-1);
    if(dot(normal,center)>0)normal=-normal;
    output.surface=float4(normal,z);output.linearZ=z;return output;
}
int2 Representative(int2 p) {
    int2 origin=min(int2((float2(p)+0.5)*float2(sceneSize)/float2(aoSize)),int2(sceneSize)-1);
    if(all(sceneSize==aoSize))return origin;
    int2 first=min(p*2,int2(sceneSize)-1),best=first;
    float nearest=1e30;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x) {
        int2 q=min(first+int2(x,y),int2(sceneSize)-1);
        float z=geometry.Load(int3(q,0)).w;
        if(z>0 && z<nearest){nearest=z;best=q;}
    }
    return best;
}
float4 Guide(int2 p) {
    if(all(sceneSize==aoSize))return geometry.Load(int3(p,0));
    return cachedGuide.Load(int3(p,0));
}
float OcclusionValue(int2 p,float4 center) {
    if(center.w<=0)return 1;
    float3 origin=Position(p,center.w);
    float2 pixelRadius=radius*float2(sceneSize)/abs(rays.zw)/center.w;
    pixelRadius*=min(1.0,128.0/max(pixelRadius.x,pixelRadius.y));
    if(max(pixelRadius.x,pixelRadius.y)<0.5)return 1;
    float rotation=frac(dot(float2(p&3),float2(0.754877666,0.569840296)))*6.283185307;
    float jitter=frac(dot(float2(p&3),float2(0.438579,0.913719)));
    float sum=0;
    [loop]for(uint d=0;d<directions;++d) {
        float angle=(float(d)+0.5)*6.283185307/float(directions)+rotation;
        float2 delta=float2(cos(angle),sin(angle))*pixelRadius;
        float horizon=bias,occlusion=0;
        int2 previousPixel=p;
        [loop]for(uint s=0;s<steps;++s) {
            float fraction=(float(s)+0.5+jitter*0.5)/float(steps);
            fraction=fraction*fraction;
            int2 samplePixel=int2(round(float2(p)+delta*fraction));
            if(any(samplePixel<0) || any(samplePixel>=int2(sceneSize)) || all(samplePixel==p))continue;
            if(all(samplePixel==previousPixel))continue;
            previousPixel=samplePixel;
            float z=linearDepth.Load(int3(samplePixel,0));
            if(z<=0 || abs(z-center.w)>=radius)continue;
            float3 v=Position(samplePixel,z)-origin;
            float distance2=dot(v,v);
            if(distance2<=1e-10 || distance2>=radius*radius)continue;
            float height=dot(center.xyz,v);
            if(height<=max(radius*0.02,center.w*0.0001))continue;
            float elevation=height*rsqrt(distance2);
            if(elevation>horizon) {
                float falloff=saturate(1-distance2/(radius*radius));
                occlusion+=(elevation-horizon)*falloff*falloff;
                horizon=elevation;
            }
        }
        sum+=occlusion;
    }
    float ao=saturate(1-sum/(float(directions)*(1-bias)));
    float fade=1-smoothstep(maxDistance*0.8,maxDistance,center.w);
    float2 edge=min(float2(p)+0.5,float2(sceneSize)-float2(p)-0.5);
    fade*=saturate(min(edge.x,edge.y)/8.0);
    return lerp(1,pow(ao,power),fade);
}
float OcclusionPixel(float4 position:SV_Position):SV_Target {
    int2 p=int2(position.xy);
    return OcclusionValue(p,geometry.Load(int3(p,0)));
}
struct ReducedOutput {float visibility:SV_Target0;float4 guide:SV_Target1;};
ReducedOutput OcclusionPixelReduced(float4 position:SV_Position) {
    int2 p=Representative(int2(position.xy));ReducedOutput output;
    output.guide=geometry.Load(int3(p,0));output.visibility=OcclusionValue(p,output.guide);return output;
}
float Agreement(float4 a,float4 b) {
    if(a.w<=0 || b.w<=0)return 0;
    float tolerance=max(0.25,min(radius*0.06,a.w*0.005));
    float dz=abs(a.w-b.w);
    return exp2(-dz/tolerance)*pow(saturate(dot(a.xyz,b.xyz)),16);
}
float FilterPixel(float4 position:SV_Position):SV_Target {
    int2 p=int2(position.xy);
    float4 center=Guide(p);
    if(center.w<=0)return 1;
    float total=0,weights=0;
    [unroll]for(int s=-3;s<=3;++s) {
        int2 q=p+s*axis;
        if(any(q<0) || any(q>=int2(aoSize)))continue;
        float weight=exp2(-float(s*s)*0.22)*Agreement(center,Guide(q));
        total+=visibility.Load(int3(q,0))*weight;weights+=weight;
    }
    return weights>1e-6?saturate(total/weights+1e-6):visibility.Load(int3(p,0));
}
float ResolveVisibility(int2 p) {
    float4 center=geometry.Load(int3(p,0));
    if(center.w<=0)return 1;
    float ao=1;
    if(all(sceneSize==aoSize))ao=visibility.Load(int3(p,0));
    else {
        float2 pixel=(float2(p)+0.5)*float2(aoSize)/float2(sceneSize)-0.5;
        int2 base=int2(floor(pixel));float2 f=frac(pixel);
        float total=0,weights=0;
        [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x) {
            int2 q=clamp(base+int2(x,y),int2(0,0),int2(aoSize)-1);
            float weight=(x?f.x:1-f.x)*(y?f.y:1-f.y)*Agreement(center,Guide(q));
            total+=visibility.Load(int3(q,0))*weight;weights+=weight;
        }
        if(weights>1e-5)ao=total/weights;
    }
    return saturate(ao);
}
float AmbientPixel(float4 position:SV_Position):SV_Target {
    return (1-ResolveVisibility(int2(position.xy)))*strength;
}
float FilterAmbientPixel(float4 position:SV_Position):SV_Target {
    float filtered=f16tof32(f32tof16(FilterPixel(position)));
    return (1-filtered)*strength;
}
