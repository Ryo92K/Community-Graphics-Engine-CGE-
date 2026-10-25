                                                                
Texture2D<float4> Scene : register(t0);
Texture2D<float> Depth : register(t1);
Texture2D<float> FocusState : register(t2);
cbuffer DepthOfFieldControls : register(b0) {
    float NearPlane;
    float FarPlane;
    float FocusDistance;
    float Strength;
    uint AutofocusEnabled;
    float DeltaSeconds;
    float FocusSpeed;
    float FocusRadius;
};
float4 DofVertex(uint vertex : SV_VertexID) : SV_Position {
    float2 corner=float2((vertex<<1)&2,vertex&2);
    return float4(corner*float2(2,-2)+float2(-1,1),0,1);
}
float Distance(float z) {
    if(z>=1)return FarPlane;
    return (NearPlane/(FarPlane*(1-z)+NearPlane*z))*FarPlane;
}
float FocusPixel(float4 position : SV_Position) : SV_Target {
    float previous=FocusState.Load(int3(0,0,0));
    if(!isfinite(previous) || previous<1 || previous>50000)previous=FocusDistance;
    uint width,height;Depth.GetDimensions(width,height);
    float values[25];int count=0;
    float2 center=float2(width,height)*0.5;
    float spacing=FocusRadius*float(min(width,height))*0.5;
    [loop]for(int y=-2;y<=2;++y) {
        [loop]for(int x=-2;x<=2;++x) {
            int2 samplePixel=int2(clamp(center+float2(x,y)*spacing,0.5,float2(width,height)-0.5));
            float z=Depth.Load(int3(samplePixel,0));
            if(isfinite(z) && z>=0 && z<0.999999)values[count++]=clamp(Distance(z),1.0,50000.0);
        }
    }
    if(count<5)return previous;
    [loop]for(int i=1;i<count;++i) {
        float value=values[i];int j=i;
        [loop]while(j>0) {
            if(values[j-1]<=value)break;
            values[j]=values[j-1];--j;
        }
        values[j]=value;
    }
    float blend=1.0-exp(-DeltaSeconds*FocusSpeed);
    return lerp(previous,values[count/2],blend);
}
float4 DofPixel(float4 position : SV_Position) : SV_Target {
    int2 pixel=int2(position.xy);
    float4 center=Scene.Load(int3(pixel,0));
    float z=Depth.Load(int3(pixel,0));
    if(!isfinite(z) || z<0 || z>1 || !all(isfinite(center.rgb)))return center;
    float focus=AutofocusEnabled!=0?FocusState.Load(int3(0,0,0)):FocusDistance;
    float distance=Distance(z);
    float radius=16.0*Strength*saturate(1.0-focus/distance);
    if(radius<0.5)return center;
    uint width,height;Scene.GetDimensions(width,height);
    float2 size=float2(width,height);
    float3 sum=center.rgb;
    float weight=1;
                                                                        
    [unroll]for(int i=0;i<32;++i) {
        float angle=float(i)*2.39996323;
        float r=sqrt((float(i)+0.5)/32.0);
        float2 samplePixel=clamp(position.xy+float2(cos(angle),sin(angle))*r*radius,0.5,size-0.5);
        float sampleZ=Depth.Load(int3(int2(samplePixel),0));
        if(!isfinite(sampleZ) || sampleZ<0 || sampleZ>1)continue;
        float sampleDistance=Distance(sampleZ);
                                                                            
        if(sampleDistance<=focus || sampleDistance<distance-max(distance*0.05,NearPlane*2))continue;
        float3 color=Scene.Load(int3(int2(samplePixel),0)).rgb;
        if(!all(isfinite(color)))continue;
        sum+=color;weight+=1;
    }
    return float4(sum/weight,center.a);
}
