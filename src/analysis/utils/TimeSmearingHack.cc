#include "TimeSmearingHack.h"

#include "TRandom.h"
#include "base/Logger.h"

using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::data;


double utils::TimeSmearingHack::CalculateTimeDifference(const CandidatePtr& p1, const CandidatePtr& p2) const
{
    auto time_diff = p1->Time() - p2->Time();

    if(smearing_enabled) {
        if(   (p1->Detector() & Detector_t::Any_t::CB   && p2->Detector() & Detector_t::Any_t::TAPS)
              || (p1->Detector() & Detector_t::Any_t::TAPS && p2->Detector() & Detector_t::Any_t::CB  )) {

            return gRandom->Gaus(time_diff, cb_taps_sigma);
        }

        if(   p1->Detector() & Detector_t::Any_t::CB
              && p2->Detector() & Detector_t::Any_t::CB) {
            return gRandom->Gaus(time_diff, cb_cb_sigma);
        }

        if(   p1->Detector() & Detector_t::Any_t::TAPS
              && p2->Detector() & Detector_t::Any_t::TAPS) {
            return gRandom->Gaus(time_diff, taps_taps_sigma);
        }
    }

    return time_diff;

}

double utils::TimeSmearingHack::CalculateTimeDifference(const CandidatePtr& p, const TaggerHitPtr& taggerhit) const
{
    auto time_diff = p->Time() - taggerhit->Time();

    if(smearing_enabled) {
        if(p->Detector() & Detector_t::Any_t::CB) {
            return gRandom->Gaus(time_diff, tagg_cb_sigma);
        }

        if(p->Detector() & Detector_t::Any_t::TAPS) {
            return gRandom->Gaus(time_diff, tagg_taps_sigma);
        }
    }

    return time_diff;

}

double utils::TimeSmearingHack::GetTime(const CandidatePtr& p) const
{
    const auto time = p->Time();

    if(p->Detector() & Detector_t::Any_t::CB) {
        return gRandom->Gaus(time, cb_sigma);
    }

    if(p->Detector() & Detector_t::Any_t::TAPS) {
        return gRandom->Gaus(time, taps_sigma);
    }

    return time;
}

utils::TimeSmearingHack::TimeSmearingHack() {
    LOG(WARNING) << "Ugly Time Smearing Hack in use!";
}
