                                                             
                                                                     
Texture2D<float4> CgeInput : register(t0);
cbuffer CgeColorControls : register(b0) {
    float CgeExposureStops;
    float CgeContrast;
    float CgeSaturation;
    float CgeSharpen;
    float CgeGamma;
    float CgeTemperature;
    float CgeMotionScale;
    float CgeMotionRadius;
    uint CgeMotionSamples;
    float CgeClarity;
    float CgeVibrance;
    uint CgeGrainFrame;
    float4 CgeVignette;
    float4 CgeGrain;
    float4 CgeSplit;
    float4 CgeShadows;
    float4 CgeHighlights;
    float4 CgeAberration;
};
float4 FullscreenVertex(uint vertex : SV_VertexID) : SV_Position {
    float2 corner=float2((vertex<<1)&2,vertex&2);
    return float4(corner*float2(2,-2)+float2(-1,1),0,1);
}
float3 ReadNeighbor(int2 pixel,int2 size) {
    return CgeInput.Load(int3(clamp(pixel,int2(0,0),size-1),0)).rgb;
}
float3 ApplyClarity(float3 color,int2 pixel,int2 size) {
    const float3 lumaWeights=float3(0.2126,0.7152,0.0722);
    float center=dot(color,lumaWeights);
    float sum=0,weight=0;
    int spacing=max(1,int(round(float(size.y)*4.0/1080.0)));
    [unroll]for(int y=-2;y<=2;++y) {
        [unroll]for(int x=-2;x<=2;++x) {
            float sampleLuma=dot(ReadNeighbor(pixel+int2(x,y)*spacing,size),lumaWeights);
            float delta=sampleLuma-center;
            float spatial=float((3-abs(x))*(3-abs(y)));
            float w=spatial/(1.0+64.0*delta*delta);
            sum+=sampleLuma*w;weight+=w;
        }
    }
    float detail=center-sum/weight;
    float midtones=4.0*saturate(center)*(1.0-saturate(center));
    float adjustment=clamp(detail*0.8,-0.08,0.08)*midtones*CgeClarity;
    float3 direction=adjustment>=0?1.0-color:color;
    float available=adjustment>=0?1.0-center:center;
    return color+direction*(adjustment/max(available,0.0001));
}
float3 ApplyVibrance(float3 color) {
    float high=max(color.r,max(color.g,color.b));
    float low=min(color.r,min(color.g,color.b));
    if(high-low<0.000001)return color;
    float luminance=dot(color,float3(0.2126,0.7152,0.0722));
    float3 chroma=color-luminance;
    float muted=1.0-(high-low)/max(high,0.000001);
    float scale=1.0+CgeVibrance*muted*muted;
    [unroll]for(int channel=0;channel<3;++channel) {
        if(chroma[channel]>0.000001)scale=min(scale,(1.0-luminance)/chroma[channel]);
        else if(chroma[channel]<-0.000001)scale=min(scale,-luminance/chroma[channel]);
    }
    return luminance+chroma*max(1.0,scale);
}
float RadialDistance(float2 position,int2 size) {
    float2 radialPoint=(position*2.0-float2(size))/float(size.y);
    return length(radialPoint)/length(float2(size)/float(size.y));
}
float3 BilinearSource(float2 position,int2 size) {
    float2 p=clamp(position-0.5,0,float2(size)-1);
    int2 at=int2(floor(p));float2 fraction=frac(p);
    return lerp(lerp(ReadNeighbor(at,size),ReadNeighbor(at+int2(1,0),size),fraction.x),
        lerp(ReadNeighbor(at+int2(0,1),size),ReadNeighbor(at+int2(1,1),size),fraction.x),fraction.y);
}
float3 AberratedSource(float3 color,float2 position,int2 size) {
    float radial=RadialDistance(position,size);
    float mask=saturate((radial-CgeAberration.y)/max(0.001,1.0-CgeAberration.y));
    float2 axis=position-float2(size)*0.5;
    float2 shift=axis/max(length(axis),1.0)*mask*mask*CgeAberration.x*max(1.0,float(size.y)*8.0/1080.0);
    if(mask>0) {
        color.r=BilinearSource(position+shift,size).r;
        color.b=BilinearSource(position-shift,size).b;
    }
    return color;
}
float3 ApplySplitToning(float3 color) {
    float luminance=dot(color,float3(0.2126,0.7152,0.0722));
    float high=smoothstep(CgeSplit.y-0.25,CgeSplit.y+0.25,luminance);
    float3 tint=lerp(CgeShadows.rgb,CgeHighlights.rgb,high);
    float3 delta=(tint-dot(tint,float3(0.2126,0.7152,0.0722)))*CgeSplit.x*0.35*4.0*luminance*(1.0-luminance);
    float scale=1;
    [unroll]for(int channel=0;channel<3;++channel) {
        if(delta[channel]>0.000001)scale=min(scale,(1-color[channel])/delta[channel]);
        else if(delta[channel]<-0.000001)scale=min(scale,-color[channel]/delta[channel]);
    }
    return color+delta*saturate(scale);
}
float3 ApplyFilmGrain(float3 color,float2 position) {
    uint2 cell=uint2(floor(position/CgeGrain.y));
    uint seed=cell.x*747796405u+cell.y*2891336453u+CgeGrainFrame*277803737u+1013904223u;
    seed^=seed>>16;seed*=2246822519u;seed^=seed>>13;seed*=3266489917u;seed^=seed>>16;
    float noise=float(seed&65535u)/65535.0-0.5;
    float luminance=saturate(dot(color,float3(0.2126,0.7152,0.0722)));
    float weight=4*luminance*(1-luminance);
    return saturate(color+noise*CgeGrain.x*0.12*weight);
}
float4 FinishMotion(float4 color,int2 pixel) {
    if(CgeGrain.x>0)color.rgb=ApplyFilmGrain(color.rgb,float2(pixel)+0.5);
    return color;
}
float4 ColorPixel(float4 position : SV_Position) : SV_Target {
    uint width,height;CgeInput.GetDimensions(width,height);
    int2 pixel=int2(position.xy),size=int2(width,height);
    float4 source=CgeInput.Load(int3(pixel,0));
    float3 color=source.rgb;
    if(CgeClarity>0)color=ApplyClarity(color,pixel,size);
    if(CgeSharpen>0) {
        float3 neighbors=ReadNeighbor(pixel+int2(-1,0),size)+ReadNeighbor(pixel+int2(1,0),size)
            +ReadNeighbor(pixel+int2(0,-1),size)+ReadNeighbor(pixel+int2(0,1),size);
        color+=CgeSharpen*(color-neighbors*0.25);
    }
    if(CgeAberration.x>0 && CgeAberration.y<1)color+=AberratedSource(source.rgb,position.xy,size)-source.rgb;
    color*=exp2(CgeExposureStops);
    color*=float3(exp2(CgeTemperature*0.25),1.0,exp2(-CgeTemperature*0.25));
    color=(color-0.5)*CgeContrast+0.5;
    float luminance=dot(color,float3(0.2126,0.7152,0.0722));
    color=lerp(luminance.xxx,color,CgeSaturation);
    if(CgeVibrance>0)color=ApplyVibrance(saturate(color));
    if(CgeSplit.x>0)color=ApplySplitToning(saturate(color));
    color=pow(saturate(color),1.0/CgeGamma);
    if(CgeVignette.x>0 && CgeVignette.y<1) {
        float edge=smoothstep(CgeVignette.y,max(CgeVignette.y+0.001,min(1.0,CgeVignette.y+CgeVignette.z)),RadialDistance(position.xy,size));
        color*=1.0-CgeVignette.x*edge;
    }
    if(CgeGrain.x>0 && CgeMotionScale<=0)color=ApplyFilmGrain(color,position.xy);
    return float4(color,source.a);
}


                                                                   
Texture2D<float2> CgeVelocity : register(t1);
Texture2D<float> CgeDepth : register(t2);
float4 MotionPixel(float4 position : SV_Position) : SV_Target {
    uint width,height;CgeInput.GetDimensions(width,height);
    int2 pixel=int2(position.xy),size=int2(width,height);
    float4 center=CgeInput.Load(int3(pixel,0));
    float2 movement=CgeVelocity.Load(int3(pixel,0));
    if(!all(isfinite(movement)) || dot(movement,movement)>0.09)return FinishMotion(center,pixel);
    float2 extent=movement*float2(size)*CgeMotionScale;
    float distance=length(extent);
    if(distance<0.5)return FinishMotion(center,pixel);
    extent*=min(1.0,2.0*CgeMotionRadius/max(distance,0.001));
    float z=CgeDepth.Load(int3(pixel,0));
    if(!isfinite(z) || z<0 || z>1)return FinishMotion(center,pixel);
                                                                                     
    float edgeTolerance=max(0.000001,0.05*max(1.0-z,0.000001));
    float3 sum=center.rgb;float weight=1;
    uint taps=clamp(CgeMotionSamples,8u,16u);
    [loop]for(uint i=0;i<taps;++i) {
        float t=(float(i)+0.5)/float(taps)-0.5;
        float2 samplePixel=clamp(float2(pixel)+0.5+extent*t,0.5,float2(size)-0.5);
        float sampleZ=CgeDepth.Load(int3(int2(samplePixel),0));
        if(!isfinite(sampleZ) || abs(sampleZ-z)>edgeTolerance)continue;
        float w=1.0-abs(t);
        sum+=CgeInput.Load(int3(int2(samplePixel),0)).rgb*w;
        weight+=w;
    }
    return FinishMotion(float4(sum/weight,center.a),pixel);
}
