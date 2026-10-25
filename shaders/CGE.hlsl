                                                             
                                                                     
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
    float2 CgePadding;
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
    color*=exp2(CgeExposureStops);
    color*=float3(exp2(CgeTemperature*0.25),1.0,exp2(-CgeTemperature*0.25));
    color=(color-0.5)*CgeContrast+0.5;
    float luminance=dot(color,float3(0.2126,0.7152,0.0722));
    color=lerp(luminance.xxx,color,CgeSaturation);
    color=pow(saturate(color),1.0/CgeGamma);
    return float4(color,source.a);
}


                                                                   
Texture2D<float2> CgeVelocity : register(t1);
Texture2D<float> CgeDepth : register(t2);
float4 MotionPixel(float4 position : SV_Position) : SV_Target {
    uint width,height;CgeInput.GetDimensions(width,height);
    int2 pixel=int2(position.xy),size=int2(width,height);
    float4 center=CgeInput.Load(int3(pixel,0));
    float2 movement=CgeVelocity.Load(int3(pixel,0));
    if(!all(isfinite(movement)) || dot(movement,movement)>0.09)return center;
    float2 extent=movement*float2(size)*CgeMotionScale;
    float distance=length(extent);
    if(distance<0.5)return center;
    extent*=min(1.0,2.0*CgeMotionRadius/max(distance,0.001));
    float z=CgeDepth.Load(int3(pixel,0));
    if(!isfinite(z) || z<0 || z>1)return center;
                                                                                     
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
    return float4(sum/weight,center.a);
}
