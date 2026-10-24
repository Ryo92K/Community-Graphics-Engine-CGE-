                                                               
Texture2D<float4> Scene : register(t0);
Texture2D<float> Depth : register(t1);
cbuffer FogControls : register(b0) {
    float NearPlane;
    float FarPlane;
    float Density;
    float StartDistance;
    float3 FogColor;
    float Padding;
};
float4 FogVertex(uint vertex : SV_VertexID) : SV_Position {
    float2 corner=float2((vertex<<1)&2,vertex&2);
    return float4(corner*float2(2,-2)+float2(-1,1),0,1);
}
float4 FogPixel(float4 position : SV_Position) : SV_Target {
    int2 pixel=int2(position.xy);
    float4 source=Scene.Load(int3(pixel,0));
    float z=Depth.Load(int3(pixel,0));
                                                                                  
    if(!isfinite(z) || z<0 || z>=1 || !all(isfinite(source.rgb)))return source;
    float denominator=FarPlane*(1-z)+NearPlane*z;
    float distance=(NearPlane/denominator)*FarPlane;
    float path=max(0,distance-StartDistance);
    float opacity=1-exp(-min(80.0,path*Density/5000.0));
    return float4(lerp(source.rgb,FogColor,opacity),source.a);
}
