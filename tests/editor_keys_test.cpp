#include "editor_keys.h"
#include <cstdio>
#include <cstdlib>
using cge::EditorKeys;
void Check(bool ok,const char* text){if(!ok){std::printf("FAIL: %s\n",text);std::exit(1);}}
int main(){
    EditorKeys keys;EditorKeys::Snapshot input{};
    Check(keys.Poll(true,input)==0,"Initial focus does not trigger action");
    input[EditorKeys::F10]=true;
    Check(keys.Poll(true,input)==(1u<<EditorKeys::F10),"F10 works without any Windows keyboard message");
    Check(keys.Poll(true,input)==0,"Held physical key does not repeat");
    Check(!keys.Press(EditorKeys::F10),"A later window message cannot toggle a second time");
    input[EditorKeys::F10]=false;keys.Release(EditorKeys::F10);keys.Poll(true,input);
    Check(keys.Press(EditorKeys::F10),"Window message can arrive first");
    input[EditorKeys::F10]=true;Check(keys.Poll(true,input)==0,"Poll cannot duplicate an earlier message");
    input[EditorKeys::F10]=false;keys.Poll(true,input);
    input[EditorKeys::ShiftEnter]=true;
    Check(keys.Poll(true,input)==(1u<<EditorKeys::ShiftEnter),"Shift+Enter physical chord");
    Check(keys.Poll(false,input)==EditorKeys::LostFocus,"Focus loss requests close");
    Check(keys.Poll(false,input)==0,"Background never triggers");
    Check(keys.Poll(true,input)==0,"Held chord at focus return cannot reopen");
    input={};keys.Poll(true,input);
    input[EditorKeys::Escape]=true;input[EditorKeys::Save]=true;input[EditorKeys::Defaults]=true;
    Check(keys.Poll(true,input)==((1u<<EditorKeys::Escape)|(1u<<EditorKeys::Save)|(1u<<EditorKeys::Defaults)),"Escape/save/reset physical fallback");
    keys.Reset();Check(keys.Poll(true,input)==0,"New session primes key state");
    std::puts("PASS: physical shortcuts, no Windows messages, deduplication, focus gating and held-key protection.");
}
