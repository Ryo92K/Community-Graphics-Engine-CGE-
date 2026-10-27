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
    if(zp>center.z && zm>center.z && abs(zp-zm)<=min(zp-center.z,zm-center.z)*0.25)return (dp+dm)*0.5;
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
float OcclusionIntegral(float cosine,float normalView,float normalTangent) {
    float sine=sqrt(saturate(1-cosine*cosine));
    return 0.5*(normalView*sine*sine+normalTangent*(acos(cosine)-sine*cosine));
}
float OcclusionValue(int2 p,float4 center) {
    if(center.w<=0)return 1;
    float3 origin=Position(p,center.w);
    float3 view=-normalize(origin);
    float3 basisX=normalize(float3(-view.z,0,view.x));
    float3 basisY=cross(basisX,view);
    float normalView=saturate(dot(center.xyz,view));
    float rotation=frac(dot(float2(p&3),float2(0.754877666,0.569840296))+padding)*6.283185307;
    float jitter=frac(dot(float2(p&3),float2(0.438579,0.913719))+padding);
    float depthError=max(0.02,center.w*center.w*((farPlane-nearPlane)/(nearPlane*farPlane))*1.192092896e-7);
    float sum=0;
    [loop]for(uint d=0;d<directions;++d) {
        float angle=(float(d)+0.5)*6.283185307/float(directions)+rotation;
        float sine,cosine;sincos(angle,sine,cosine);
        float3 tangent=basisX*cosine+basisY*sine;
        float normalTangent=0,horizon=0,integral=0;
        bool horizonReady=false,integralReady=false;
        float previousIntegral=0,occlusion=0;
        float2 delta=(tangent.xy-origin.xy/origin.z*tangent.z)*float2(sceneSize)/rays.zw*(radius/center.w);
        float extent=max(abs(delta.x),abs(delta.y));
        if(extent<0.5)continue;
        delta*=min(1.0,128.0/extent);
        int2 previousPixel=p;
        [loop]for(uint s=0;s<=steps;++s) {
            float fraction=(float(s)-0.5+jitter*0.5)/float(steps);
            float2 offset=s==0?normalize(delta):delta*fraction*fraction;
            int2 samplePixel=int2(round(float2(p)+offset));
            if(any(samplePixel<0) || any(samplePixel>=int2(sceneSize)) || all(samplePixel==p))continue;
            if(all(samplePixel==previousPixel))continue;
            previousPixel=samplePixel;
            float z=linearDepth.Load(int3(samplePixel,0));
            if(z<=0 || abs(z-center.w)>=radius)continue;
            float3 v=Position(samplePixel,z)-origin;
            float height=dot(center.xyz,v);
            if(height<=depthError)continue;
            float distance2=dot(v,v);
            if(distance2<=1e-10 || distance2>=radius*radius)continue;
            float inverseDistance=rsqrt(distance2);
            if(height*inverseDistance<=bias)continue;
            [branch]if(!horizonReady) {
                normalTangent=dot(center.xyz,tangent);
                float normalLength=sqrt(normalView*normalView+normalTangent*normalTangent);
                if(normalLength<=bias)break;
                float tangentBias=saturate(bias/normalLength);
                float tangentSine=sqrt(saturate(1-tangentBias*tangentBias));
                if(normalTangent*tangentBias+normalView*tangentSine<=0)break;
                horizon=clamp((normalView*tangentBias-normalTangent*tangentSine)/normalLength,-1,1);
                horizonReady=true;
            }
            float elevation=clamp(dot(v,view)*inverseDistance,-1,1);
            if(elevation>horizon) {
                [branch]if(!integralReady) {
                    integral=OcclusionIntegral(horizon,normalView,normalTangent);
                    integralReady=true;
                }
                float blocked=max(0,integral-OcclusionIntegral(elevation,normalView,normalTangent));
                float falloff=saturate(1-distance2/(radius*radius));
                occlusion+=max(0,blocked-previousIntegral)*falloff*falloff;
                previousIntegral=blocked;horizon=elevation;
            }
        }
        sum+=occlusion;
    }
    float ao=saturate(1-2*sum/float(directions));
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
    float separation=dz/tolerance;
    return exp2(-separation*separation)*pow(saturate(dot(a.xyz,b.xyz)),16);
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

cbuffer TemporalControls:register(b1) {
    float4 previousTransform[3];
    float4 previousRays;
    uint reuseHistory,historyWidth,historyHeight,temporalPadding;
};
Texture2D<float> currentAmbient:register(t0);
Texture2D<float> previousAmbient:register(t6);
Texture2D<float4> previousGeometry:register(t7);
float TemporalPixel(float4 position:SV_Position):SV_Target {
    int2 p=int2(position.xy);float current=currentAmbient.Load(int3(p,0));
    if(reuseHistory==0)return current;
    float4 center=geometry.Load(int3(p,0));if(center.w<=0)return current;
    float4 view=float4(Position(p,center.w),1);
    float3 previous=float3(dot(previousTransform[0],view),dot(previousTransform[1],view),dot(previousTransform[2],view));
    if(!all(isfinite(previous)) || previous.z<=0)return current;
    float2 uv=(previous.xy/previous.z-previousRays.xy)/previousRays.zw;
    if(any(uv<=0) || any(uv>=1))return current;
    float3 normal=float3(dot(previousTransform[0].xyz,center.xyz),dot(previousTransform[1].xyz,center.xyz),dot(previousTransform[2].xyz,center.xyz));
    float2 pixel=uv*float2(historyWidth,historyHeight)-0.5;
    int2 base=int2(floor(pixel));float2 fraction=frac(pixel);float total=0,weights=0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x) {
        int2 q=base+int2(x,y);
        if(any(q<0) || any(q>=int2(historyWidth,historyHeight)))continue;
        float4 old=previousGeometry.Load(int3(q,0));
        if(old.w<=0 || abs(old.w-previous.z)>max(0.5,previous.z*0.005) || dot(old.xyz,normal)<0.95)continue;
        float weight=(x?fraction.x:1-fraction.x)*(y?fraction.y:1-fraction.y);
        total+=previousAmbient.Load(int3(q,0))*weight;weights+=weight;
    }
    if(weights<0.25)return current;
    float low=current,high=current;
    [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
        int2 q=Bounded(p+int2(x,y));float4 guide=geometry.Load(int3(q,0));
        if(guide.w<=0 || abs(guide.w-center.w)>max(0.5,center.w*0.005) || dot(guide.xyz,center.xyz)<0.95)continue;
        float value=currentAmbient.Load(int3(q,0));low=min(low,value);high=max(high,value);
    }
    float history=total/weights;
    float confidence=1-smoothstep(0.015,0.08,abs(history-current));
    history=clamp(history,max(0,low-0.003),min(strength,high+0.003));
    return clamp(lerp(current,history,0.85*confidence*saturate(weights)),0,strength);
}

Texture2D<float> temporalBounds:register(t8);
float TemporalCachedPixel(float4 position:SV_Position):SV_Target {
    int2 p=int2(position.xy);float current=currentAmbient.Load(int3(p,0));
    if(reuseHistory==0)return current;
    float4 center=geometry.Load(int3(p,0));if(center.w<=0)return current;
    float4 view=float4(Position(p,center.w),1);
    float3 previous=float3(dot(previousTransform[0],view),dot(previousTransform[1],view),dot(previousTransform[2],view));
    if(!all(isfinite(previous)) || previous.z<=0)return current;
    float2 uv=(previous.xy/previous.z-previousRays.xy)/previousRays.zw;
    if(any(uv<=0) || any(uv>=1))return current;
    float3 normal=float3(dot(previousTransform[0].xyz,center.xyz),dot(previousTransform[1].xyz,center.xyz),dot(previousTransform[2].xyz,center.xyz));
    float2 pixel=uv*float2(historyWidth,historyHeight)-0.5;
    int2 base=int2(floor(pixel));float2 fraction=frac(pixel);float total=0,weights=0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x) {
        int2 q=base+int2(x,y);
        if(any(q<0) || any(q>=int2(historyWidth,historyHeight)))continue;
        float4 old=previousGeometry.Load(int3(q,0));
        if(old.w<=0 || abs(old.w-previous.z)>max(0.5,previous.z*0.005) || dot(old.xyz,normal)<0.95)continue;
        float weight=(x?fraction.x:1-fraction.x)*(y?fraction.y:1-fraction.y);
        total+=previousAmbient.Load(int3(q,0))*weight;weights+=weight;
    }
    if(weights<0.25)return current;
    uint packed=asuint(temporalBounds.Load(int3(p,0)))^0x40000000u;
    float low=f16tof32(packed&65535u),high=f16tof32(packed>>16);
    float history=total/weights;
    float confidence=1-smoothstep(0.015,0.08,abs(history-current));
    history=clamp(history,max(0,low-0.003),min(strength,high+0.003));
    return clamp(lerp(current,history,0.85*confidence*saturate(weights)),0,strength);
}

#ifdef CGE_TEMPORAL_COMPUTE
RWTexture2D<float> temporalBoundsOutput:register(u0);
groupshared float4 temporalGeometryTile[180];
groupshared float temporalAmbientTile[180];
[numthreads(16,8,1)]void TemporalCompute(uint3 group:SV_GroupID,uint3 local:SV_GroupThreadID,uint index:SV_GroupIndex,uint3 pixel:SV_DispatchThreadID) {
    int2 origin=int2(group.xy)*int2(16,8)-1;
    for(uint i=index;i<180;i+=128) {
        int2 p=Bounded(origin+int2(i%18,i/18));
        temporalGeometryTile[i]=geometry.Load(int3(p,0));temporalAmbientTile[i]=currentAmbient.Load(int3(p,0));
    }
    GroupMemoryBarrierWithGroupSync();
    if(any(pixel.xy>=sceneSize))return;
    uint centerIndex=(local.y+1)*18+local.x+1;float4 center=temporalGeometryTile[centerIndex];
    if(center.w<=0)return;
    float current=temporalAmbientTile[centerIndex],low=current,high=current;
    [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
        if(x==0 && y==0)continue;
        uint i=centerIndex+y*18+x;float4 guide=temporalGeometryTile[i];
        if(guide.w<=0 || abs(guide.w-center.w)>max(0.5,center.w*0.005) || dot(guide.xyz,center.xyz)<0.95)continue;
        float value=temporalAmbientTile[i];low=min(low,value);high=max(high,value);
    }
    uint packed=f32tof16(low)|(f32tof16(high)<<16);
    temporalBoundsOutput[pixel.xy]=asfloat(packed^0x40000000u);
}
#endif
