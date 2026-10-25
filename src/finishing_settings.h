#pragma once
#include "settings.h"
#include <cmath>
namespace cge {
template<class Profile,class Visitor>void FinishingEffects(Profile& p,Visitor&& visit) {
    visit("Vignette",p.vignette.enabled);
    visit("FilmGrain",p.filmGrain.enabled);
    visit("SplitToning",p.splitToning.enabled);
    visit("ChromaticAberration",p.chromaticAberration.enabled);
}
template<class Profile,class Visitor>void FinishingParameters(Profile& p,Visitor&& visit) {
    visit("Vignette","Strength","Strength",p.vignette.strength,0.0f,1.0f,0.25f);
    visit("Vignette","Radius","Radius",p.vignette.radius,0.0f,1.0f,0.35f);
    visit("Vignette","Softness","Softness",p.vignette.softness,0.01f,1.0f,0.5f);
    visit("FilmGrain","Strength","Strength",p.filmGrain.strength,0.0f,1.0f,0.1f);
    visit("FilmGrain","Size","Size (px)",p.filmGrain.size,1.0f,4.0f,1.0f);
    visit("SplitToning","Strength","Strength",p.splitToning.strength,0.0f,1.0f,0.25f);
    visit("SplitToning","Balance","Balance",p.splitToning.balance,0.0f,1.0f,0.5f);
    visit("SplitToning","ShadowRed","Shadows Red",p.splitToning.shadowRed,0.0f,1.0f,0.25f);
    visit("SplitToning","ShadowGreen","Shadows Green",p.splitToning.shadowGreen,0.0f,1.0f,0.4f);
    visit("SplitToning","ShadowBlue","Shadows Blue",p.splitToning.shadowBlue,0.0f,1.0f,0.65f);
    visit("SplitToning","HighlightRed","Highlights Red",p.splitToning.highlightRed,0.0f,1.0f,1.0f);
    visit("SplitToning","HighlightGreen","Highlights Green",p.splitToning.highlightGreen,0.0f,1.0f,0.72f);
    visit("SplitToning","HighlightBlue","Highlights Blue",p.splitToning.highlightBlue,0.0f,1.0f,0.45f);
    visit("ChromaticAberration","Strength","Strength",p.chromaticAberration.strength,0.0f,1.0f,0.25f);
    visit("ChromaticAberration","Radius","Center Radius",p.chromaticAberration.radius,0.0f,1.0f,0.5f);
}
inline bool FiniteFinishing(const ColorSettings& p) {
    bool valid=true;
    FinishingParameters(p,[&](auto,auto,auto,float value,auto,auto,auto){valid=valid && std::isfinite(value);});
    return valid;
}
}
