                                                                               
Texture2D<float4> Input : register(t0);
Texture2D<float4> Detail : register(t1);
Texture2D<float> Dirt : register(t2);
Texture2D<float4> LightingContext : register(t3);
SamplerState LinearClamp : register(s0);
cbuffer BloomControls : register(b0) {
    float2 OutputSize;
    float Threshold;
    float Intensity;
    float Blend;
    float DirtStrength;
    float DirtThreshold;
    float Padding;
};
float4 BloomVertex(uint vertex : SV_VertexID) : SV_Position {
    float2 corner=float2((vertex<<1)&2,vertex&2);
    return float4(corner*float2(2,-2)+float2(-1,1),0,1);
}
float3 SafeLight(float3 value) {
    return all(isfinite(value))?clamp(value,0.0,60000.0):0;
}
float3 Extract(float3 color) {
    color=SafeLight(color);
    float brightness=max(color.r,max(color.g,color.b));
    float knee=max(Threshold*0.5,0.0001);
    float soft=clamp(brightness-Threshold+knee,0.0,2.0*knee);
    soft=soft*soft/(4.0*knee);
    return color*(max(brightness-Threshold,soft)/max(brightness,0.0001));
}
float4 BrightPixel(float4 p : SV_Position) : SV_Target {
                                                                                   
    uint width,height;Input.GetDimensions(width,height);
    int2 start=int2(p.xy)*2;
    float3 result=0;float light=0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x)
    {
        float3 sample=SafeLight(Input.Load(int3(min(start+int2(x,y),int2(width,height)-1),0)).rgb);
        result+=Extract(sample);light+=dot(sample,float3(0.2126,0.7152,0.0722));
    }
    return float4(result*0.25,light*0.25);
}
float4 DownPixel(float4 p : SV_Position) : SV_Target {
    uint width,height;Input.GetDimensions(width,height);
    int2 start=int2(p.xy)*2;float4 result=0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x)
        result+=Input.Load(int3(min(start+int2(x,y),int2(width,height)-1),0));
    return result*0.25;
}
float4 Tent(float2 uv) {
    uint width,height;Input.GetDimensions(width,height);
    float2 texel=1.0/float2(width,height);
    float4 sum=0;
    [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
        float w=(x==0?2.0:1.0)*(y==0?2.0:1.0);
        sum+=Input.SampleLevel(LinearClamp,uv+float2(x,y)*texel,0)*w;
    }
    return sum*(1.0/16.0);
}
float4 UpPixel(float4 p : SV_Position) : SV_Target {
    float2 uv=p.xy/OutputSize;
                                                                           
    return lerp(Detail.Load(int3(int2(p.xy),0)),Tent(uv),Blend);
}
float4 CompositePixel(float4 p : SV_Position) : SV_Target {
    float4 source=Detail.Load(int3(int2(p.xy),0));
    float4 filtered=Tent(p.xy/OutputSize);
    float3 halo=max(filtered.rgb,0)*Intensity;
    [branch]if(DirtStrength>0) {
        float2 uv=(p.xy-OutputSize*0.5)/max(OutputSize.x,OutputSize.y)+0.5;
        float ambient=LightingContext.SampleLevel(LinearClamp,p.xy/OutputSize,0).a;
        float light=max(filtered.a-ambient,0);
        float response=smoothstep(DirtThreshold,DirtThreshold*2,light);
        halo*=1+12*DirtStrength*response*Dirt.SampleLevel(LinearClamp,uv,0);
    }
    return float4(min(SafeLight(source.rgb)+halo,65000.0),source.a);
}
