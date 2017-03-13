#pragma once

#include "analysis/physics/Physics.h"
#include "plot/PromptRandomHist.h"
#include <vector>

class TH1D;
class TTree;

namespace ant {
namespace analysis {
namespace physics {

class scratch_sobotzik_Pi0Calib : public Physics {
protected:

    struct hist_t {
        std::string prefix;

        TH1D* h_IM_All;

        TH2D* h_IM_CB_all;
        TH2D* h_IM_CB_interval;
        TH2D* h_IM_CB_Angle_Energy;

        TH1D* h_IM_CB_corr;
        TH1D* h_IM_TAPS;

        TH1D* h_Angle_CB;
        TH1D* h_Angle_TAPS;

        TH2D* h_ClusterHitTiming_CB;
        TH2D* h_ClusterHitTiming_TAPS;

        using range_t = interval<int>;
        const range_t n_CB;
        const range_t n_TAPS;
        const interval<double> CaloEnergy_Window;
        static const range_t any;
        hist_t(const HistogramFactory& HistFac,
               const range_t& cb, const range_t& taps,
               const interval<double>& CaloEnergy_Window);
        void Fill(const TCandidatePtrList& c_CB, const TCandidatePtrList& c_TAPS) const;
        void ShowResult() const;


    };

    std::vector<hist_t> hists;

    TH1D* h_Mult_All;
    TH1D* h_Mult_CB;
    TH1D* h_Mult_TAPS;

public:
    scratch_sobotzik_Pi0Calib(const std::string& name, OptionsPtr opts);
    virtual ~scratch_sobotzik_Pi0Calib();

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override;
    virtual void ShowResult() override;
};


}
}
}
