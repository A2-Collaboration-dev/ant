#pragma once

#include "physics/Physics.h"
#include "plot/PromptRandomHist.h"
#include "utils/fitter/KinFitter.h"
#include "utils/Uncertainties.h"
#include "utils/TriggerSimulation.h"
#include "base/WrapTTree.h"

namespace ant {
namespace analysis {
namespace physics {

class TriggerSimulation : public Physics {
protected:
    PromptRandom::Switch promptrandom;
    utils::TriggerSimulation triggersimu;

    TH1D* steps;

    TH1D* h_CBESum_raw;
    TH1D* h_CBESum_pr;
    TH1D* h_CBTiming;
    TH2D* h_CBTiming_CaloE;

    struct ClusterPlots_t {
        ClusterPlots_t(const HistogramFactory& HistFac);
        void Fill(const TEventData& recon) const;
        void Show(canvas& c) const;
        TH2D* h_CaloE_ClSize;
        TH2D* h_CaloE_nCl;
        TH2D* h_CaloE_Time;
        TH1D* h_Hits_stat;
        TH2D* h_Hits_E_t;
    };

    const ClusterPlots_t Clusters_All;
    const ClusterPlots_t Clusters_Tail;

    TH1D* h_TaggT;
    TH1D* h_TaggT_corr;
    TH2D* h_TaggT_CBTiming;

    struct Tree_t : WrapTTree {
        ADD_BRANCH_T(bool,   IsMC)

        ADD_BRANCH_T(double,   TaggW)
        ADD_BRANCH_T(double,   TaggT)
        ADD_BRANCH_T(double,   TaggE)
        ADD_BRANCH_T(unsigned, TaggCh)

        ADD_BRANCH_T(unsigned, nPhotons)
        ADD_BRANCH_T(double,   FitProb)

        ADD_BRANCH_T(std::vector<double>, IM_Combs)
    };

    Tree_t t;

    utils::UncertaintyModelPtr fit_model;
    std::vector<std::unique_ptr<utils::KinFitter>> fitters;
    utils::KinFitter& GetFitter(unsigned nPhotons);

public:
    TriggerSimulation(const std::string& name, OptionsPtr opts);

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override;
    virtual void ShowResult() override;
};


}}}