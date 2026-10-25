#pragma once
#include "settings.h"
#include <cmath>
namespace cge {
template<class Profile,class Visitor>void FinishingEffects(Profile& p,Visitor&& visit) {
    visit("SSAO",p.ssao.enabled);
    visit("Sharpness",p.sharpnessEnabled);
    visit("LensDistortion",p.lensDistortion.enabled);
    visit("LensDirt",p.lensDirt.enabled);
    visit("LUT",p.lut.enabled);
    visit("LiftGammaGain",p.liftGammaGain.enabled);
    visit("Debanding",p.debanding.enabled);
    visit("Vignette",p.vignette.enabled);
    visit("FilmGrain",p.filmGrain.enabled);
    visit("SplitToning",p.splitToning.enabled);
    visit("ChromaticAberration",p.chromaticAberration.enabled);
}
template<class Profile,class Visitor>void FinishingParameters(Profile& p,Visitor&& visit) {
    visit("SSAO","LightProtection","Light Protection",p.ssao.lightProtection,0.0f,1.0f,1.0f);
    visit("SSAO","Strength","Strength",p.ssao.strength,0.0f,1.0f,0.65f);
    visit("SSAO","Radius","Radius (units)",p.ssao.radius,1.0f,250.0f,60.0f);
    visit("SSAO","Bias","Bias",p.ssao.bias,0.01f,0.3f,0.05f);
    visit("SSAO","Power","Power",p.ssao.power,1.0f,4.0f,1.5f);
    visit("SSAO","MaxDistance","Max Distance (units)",p.ssao.maxDistance,500.0f,50000.0f,8000.0f);
    visit("LensDistortion","Amount","Amount",p.lensDistortion.amount,-1.0f,1.0f,0.2f);
    visit("LensDirt","Strength","Strength",p.lensDirt.strength,0.0f,1.0f,0.35f);
    visit("LensDirt","Threshold","Light Threshold",p.lensDirt.threshold,0.01f,8.0f,0.25f);
    visit("LUT","Strength","Strength",p.lut.strength,0.0f,1.0f,1.0f);
    visit("LiftGammaGain","Strength","Strength",p.liftGammaGain.strength,0.0f,1.0f,1.0f);
    visit("LiftGammaGain","LiftRed","Lift Red",p.liftGammaGain.liftRed,-0.25f,0.25f,0.0f);
    visit("LiftGammaGain","LiftGreen","Lift Green",p.liftGammaGain.liftGreen,-0.25f,0.25f,0.0f);
    visit("LiftGammaGain","LiftBlue","Lift Blue",p.liftGammaGain.liftBlue,-0.25f,0.25f,0.0f);
    visit("LiftGammaGain","GammaRed","Gamma Red",p.liftGammaGain.gammaRed,0.5f,2.5f,1.0f);
    visit("LiftGammaGain","GammaGreen","Gamma Green",p.liftGammaGain.gammaGreen,0.5f,2.5f,1.0f);
    visit("LiftGammaGain","GammaBlue","Gamma Blue",p.liftGammaGain.gammaBlue,0.5f,2.5f,1.0f);
    visit("LiftGammaGain","GainRed","Gain Red",p.liftGammaGain.gainRed,0.0f,2.0f,1.0f);
    visit("LiftGammaGain","GainGreen","Gain Green",p.liftGammaGain.gainGreen,0.0f,2.0f,1.0f);
    visit("LiftGammaGain","GainBlue","Gain Blue",p.liftGammaGain.gainBlue,0.0f,2.0f,1.0f);
    visit("Debanding","Strength","Strength",p.debanding.strength,0.0f,1.0f,0.5f);
    visit("Debanding","Radius","Radius (px)",p.debanding.radius,1.0f,16.0f,4.0f);
    visit("Debanding","Threshold","Threshold",p.debanding.threshold,0.005f,0.1f,0.025f);
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
