#pragma once
#include <array>
#include <atomic>
namespace cge {
class EditorKeys {
public:
    enum Key : unsigned { F10, ShiftEnter, Escape, Save, Defaults, Compare, Count };
    using Snapshot=std::array<bool,Count>;
    static constexpr unsigned LostFocus=1u<<Count;
    bool Press(Key key) {return !down_[key].exchange(true);}
    void Release(Key key) {down_[key]=false;}
    unsigned Poll(bool focused,const Snapshot& keys) {
        unsigned actions=0;
        if(!focused) {
            if(focused_)actions=LostFocus;
            focused_=false;
            for(auto& key:down_)key=false;
            return actions;
        }
        if(!focused_) {
            focused_=true;
            for(unsigned i=0;i<Count;++i)down_[i]=keys[i];
            return 0; // Do not treat a held key on Alt+Tab as a new press.
        }
        for(unsigned i=0;i<Count;++i) {
            if(keys[i]){if(Press(static_cast<Key>(i)))actions|=1u<<i;}
            else Release(static_cast<Key>(i));
        }
        return actions;
    }
    void Reset() {focused_=false;for(auto& key:down_)key=false;}
private:
    std::array<std::atomic<bool>,Count> down_{};
    bool focused_{}; // Poll runs only on the rendering thread.
};
}
