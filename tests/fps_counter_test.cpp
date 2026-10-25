#include "fps_counter.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
void Check(bool value,const char* message){if(!value){std::printf("FAIL: %s\n",message);std::exit(1);}}
int main(){
    cge::FpsCounter counter;Check(counter.Value()==0,"No fabricated initial FPS");
    counter.Tick(0);for(int i=1;i<=61;++i)counter.Tick(double(i)/60);
    Check(std::abs(counter.Value()-60)<0.01f,"60 FPS measured from frame intervals");
    counter.Tick(10);Check(counter.Value()==0,"Loading stall resets old result");
    for(int i=1;i<=121;++i)counter.Tick(10+double(i)/120);
    Check(std::abs(counter.Value()-120)<0.01f,"120 FPS measured after restart");
    counter.Tick(std::numeric_limits<double>::quiet_NaN());Check(counter.Value()==0,"Invalid time resets measurement");
    counter.Tick(20);counter.Tick(19);Check(counter.Value()==0,"Clock reversal bypassed");
    std::puts("PASS: frame-rate measurement, refresh window, stalls and invalid clocks.");
}
