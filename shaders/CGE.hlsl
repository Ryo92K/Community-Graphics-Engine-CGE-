// CGE native SDR post-processing. Authored for this project.
// Slots: source color t0; color controls b0. No external shader framework.
Texture2D<float4> CgeInput : register(t0);
cbuffer CgeColorControls : register(b0) {
    float CgeExposureStops;
    float CgeContrast;
    float CgeSaturation;
    float CgeSharpen;
};
float4 FullscreenVertex(uint vertex : SV_VertexID) : SV_Position {
    float2 corner=float2((vertex<<1)&2,vertex&2);
    return float4(corner*float2(2,-2)+float2(-1,1),0,1);
}
float3 ReadNeighbor(int2 pixel,int2 size) {
    return CgeInput.Load(int3(clamp(pixel,int2(0,0),size-1),0)).rgb;
}
float4 ColorPixel(float4 position : SV_Position) : SV_Target {
    uint width,height;CgeInput.GetDimensions(width,height);
    int2 pixel=int2(position.xy),size=int2(width,height);
    float4 source=CgeInput.Load(int3(pixel,0));
    float3 color=source.rgb;
    if(CgeSharpen>0) {
        float3 neighbors=ReadNeighbor(pixel+int2(-1,0),size)+ReadNeighbor(pixel+int2(1,0),size)
            +ReadNeighbor(pixel+int2(0,-1),size)+ReadNeighbor(pixel+int2(0,1),size);
        color+=CgeSharpen*(color-neighbors*0.25);
    }
    color*=exp2(CgeExposureStops);
    color=(color-0.5)*CgeContrast+0.5;
    float luminance=dot(color,float3(0.2126,0.7152,0.0722));
    color=lerp(luminance.xxx,color,CgeSaturation);
    return float4(saturate(color),source.a);
}
