#pragma once

#include "analysis/physics/Physics.h"

namespace ant {

namespace expconfig {
namespace detector {
struct TAPS;
}
}

namespace analysis {
namespace physics {

class Time : public Physics {

protected:

    TH2D* hTime;
    TH2D* hTimeToF;
    TH2D* hTimeToTagger;
    TH2D* hTimeMultiplicity;
    TH1D* hCBTriggerTiming;

    std::shared_ptr<Detector_t> Detector;
    bool isTagger;
    std::shared_ptr<expconfig::detector::TAPS> taps_detector;

public:


    Time(const Detector_t::Type_t& detectorType,
         const std::string& name, OptionsPtr opts);

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override;
    virtual void ShowResult() override;
};

}}} // namespace ant::analysis::physics
