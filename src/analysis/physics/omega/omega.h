#pragma once

#include "analysis/physics/Physics.h"
#include "analysis/utils/A2GeoAcceptance.h"
#include "analysis/utils/particle_tools.h"
#include "analysis/utils/Fitter.h"
#include "analysis/utils/Uncertainties.h"
#include "analysis/plot/PromptRandomHist.h"
#include "base/Tree.h"
#include "base/interval.h"
#include "base/std_ext/math.h"
#include "base/WrapTTree.h"

#include "TTree.h"
#include "TLorentzVector.h"
#include "Rtypes.h"

#include <map>


class TH1D;
class TH2D;
class TH3D;


namespace ant {

namespace analysis {
namespace physics {

class OmegaMCTruePlots: public Physics {
public:
    struct PerChannel_t {
        std::string title;
        TH2D* proton_E_theta = nullptr;
        TH2D* photons_E_theta = nullptr;

        PerChannel_t(const std::string& Title, HistogramFactory& hf);

        void Show();
        void Fill(const TEventData& d);
    };

    std::map<std::string,PerChannel_t> channels;

    OmegaMCTruePlots(const std::string& name, OptionsPtr opts);

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override;
    virtual void Finish() override;
    virtual void ShowResult() override;
};

class OmegaBase: public Physics {

public:
    enum class DataMode {
        MCTrue,
        Reconstructed
    };

protected:
    utils::A2SimpleGeometry geo;
    double calcEnergySum(const TParticleList& particles) const;
    TParticleList getGeoAccepted(const TParticleList& p) const;
    unsigned geoAccepted(const TCandidateList& cands) const;

    DataMode mode = DataMode::Reconstructed;

    virtual void Analyse(const TEventData& data, const TEvent& event, manager_t& manager) =0;

public:
    OmegaBase(const std::string &name, OptionsPtr opts);
    virtual ~OmegaBase() = default;

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override;
    virtual void Finish() override;
    virtual void ShowResult() override;

    static double getTime(const TParticlePtr& p) {
        return p->Candidate != nullptr ? p->Candidate->Time : std_ext::NaN;
    }

    template <typename it_type>
    static LorentzVec LVSum(it_type begin, it_type end) {
        LorentzVec v;

        while(begin!=end) {
            v += **begin;
            ++begin;
        }

        return v;
    }

    template <typename it_type>
    static LorentzVec LVSumL(it_type begin, it_type end) {
        LorentzVec v;

        while(begin!=end) {
            v += *begin;
            ++begin;
        }

        return v;
    }

};

class OmegaMCTree : public Physics {
protected:
    TTree* tree = nullptr;
    TLorentzVector proton_vector;
    TLorentzVector omega_vector;
    TLorentzVector eta_vector;
    TLorentzVector gamma1_vector;
    TLorentzVector gamma2_vector;
    TLorentzVector gamma3_vector;

public:

    OmegaMCTree(const std::string& name, OptionsPtr opts);
    virtual ~OmegaMCTree();

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override;
    virtual void ShowResult() override;
    LorentzVec getGamma1() const;
    void setGamma1(const LorentzVec& value);
};

struct TagChMultiplicity {
    TH1D* hTagChMult;
    unsigned nchannels;

    TagChMultiplicity(HistogramFactory& hf);

    void Fill(const std::vector<TTaggerHit>& t);
};

class OmegaEtaG2 : public OmegaBase {
public:
    struct OmegaTree_t : WrapTTree {
        OmegaTree_t();

        ADD_BRANCH_T(unsigned, nCandsInput)
        ADD_BRANCH_T(double, CandsUsedE)
        ADD_BRANCH_T(double, CandsunUsedE)

        ADD_BRANCH_T(std::vector<TLorentzVector>, photons, 3)
        ADD_BRANCH_T(std::vector<TLorentzVector>, photons_fitted, 3)
        ADD_BRANCH_T(std::vector<double>,         photons_Time, 3)
        ADD_BRANCH_T(std::vector<TVector2>,       photons_PSA, 3)
        ADD_BRANCH_T(std::vector<int>,            photons_detector,3)
        ADD_BRANCH_T(std::vector<double>,         photons_vetoE, 3)

        ADD_BRANCH_T(std::vector<double>,         photon_E_pulls, 3)
        ADD_BRANCH_T(std::vector<double>,         photon_theta_pulls, 3)
        ADD_BRANCH_T(std::vector<double>,         photon_phi_pulls, 3)


        ADD_BRANCH_T(TLorentzVector,              p)
        ADD_BRANCH_T(TLorentzVector,              p_fitted)
        ADD_BRANCH_T(double,                      p_Time)
        ADD_BRANCH_T(TVector2,                    p_PSA)
        ADD_BRANCH_T(int,                         p_detector)
        ADD_BRANCH_T(double,                      p_vetoE)

        ADD_BRANCH_T(double,                      p_theta_pull)
        ADD_BRANCH_T(double,                      p_phi_pull)


        ADD_BRANCH_T(TLorentzVector,              ggg)
        ADD_BRANCH_T(TLorentzVector,              ggg_fitted)
        ADD_BRANCH_T(TLorentzVector,              mm)
        ADD_BRANCH_T(double,                      copl_angle)
        ADD_BRANCH_T(double,                      p_mm_angle)

        ADD_BRANCH_T(std::vector<double>,         ggIM, 3)
        ADD_BRANCH_T(std::vector<double>,         ggIM_fitted, 3)

        ADD_BRANCH_T(std::vector<double>,         BachelorE, 3)
        ADD_BRANCH_T(std::vector<double>,         BachelorE_fitted, 3)

        ADD_BRANCH_T(double,                      TaggW)
        ADD_BRANCH_T(double,                      TaggE)
        ADD_BRANCH_T(double,                      TaggT)
        ADD_BRANCH_T(unsigned,                    TaggCh)
        ADD_BRANCH_T(double,                      beam_E_fitted)
        ADD_BRANCH_T(double,                      beam_E_pull)
        ADD_BRANCH_T(double,                      zVertex)

        ADD_BRANCH_T(double,                      KinFitChi2)
        ADD_BRANCH_T(double,                      KinFitProb)
        ADD_BRANCH_T(int,                         KinFitIterations)

        ADD_BRANCH_T(double,                      CBESum)
        ADD_BRANCH_T(double,                      CBAvgTime)

        ADD_BRANCH_T(bool,                        p_matched)

        ADD_BRANCH_T(unsigned,                    Channel)

        ADD_BRANCH_T(std::vector<double>,         pi0chi2, 3)
        ADD_BRANCH_T(std::vector<double>,         pi0prob, 3)
        ADD_BRANCH_T(int,                         iBestPi0)
        ADD_BRANCH_T(std::vector<double>,         etachi2, 3)
        ADD_BRANCH_T(std::vector<double>,         etaprob, 3)
        ADD_BRANCH_T(int,                         iBestEta)

        ADD_BRANCH_T(int,                         bestHyp)

        ADD_BRANCH_T(std::vector<double>,         pi0_im,3)
        ADD_BRANCH_T(std::vector<double>,         eta_im,3)
        ADD_BRANCH_T(std::vector<double>,         eta_omega_im,3)
        ADD_BRANCH_T(std::vector<double>,         pi0_omega_im,3)

        ADD_BRANCH_T(std::vector<TLorentzVector>, LostGammas)

    };

protected:
    void Analyse(const TEventData &data, const TEvent& event, manager_t& manager) override;

    static utils::UncertaintyModelPtr getModel(const std::string& modelname);

    TH1D* missed_channels = nullptr;
    TH1D* found_channels  = nullptr;


    //======== Tree ===============================================================

    TTree*  tree = nullptr;
    OmegaTree_t t;

    using combs_t = std::vector<std::vector<std::size_t>>;
    static const combs_t combs;




    //======== Settings ===========================================================

    const double cut_ESum;
    const double cut_Copl;
    const interval<double> photon_E_cb;
    const interval<double> photon_E_taps;
    const interval<double> proton_theta;
    const interval<double> cut_missing_mass;
    const double opt_kinfit_chi2cut;
    const bool   opt_FitZVertex;

    const unsigned nphotons    = 3;
    const unsigned nCandsMin   = nphotons  + 1;
    const unsigned nExtraCands = 0;
    const unsigned nCandsMax   = nCandsMin + nExtraCands;


    ant::analysis::PromptRandom::Switch promptrandom;

    utils::UncertaintyModelPtr model;

    utils::KinFitter fitter;

    using doubles = std::vector<double>;

    struct MyTreeFitter_t {
        utils::TreeFitter treefitter;
        utils::TreeFitter::tree_t fitted_Omega;
        utils::TreeFitter::tree_t fitted_g_Omega;
        utils::TreeFitter::tree_t fitted_X;
        utils::TreeFitter::tree_t fitted_g1_X;
        utils::TreeFitter::tree_t fitted_g2_X;

        MyTreeFitter_t(const ParticleTypeTree& ttree, const ParticleTypeDatabase::Type& mesonT, utils::UncertaintyModelPtr model, const bool fix_z_vertex);

        void HypTestCombis(const TParticleList& photons, doubles& chi2s, doubles& probs, doubles& ggims, doubles& gggims, int& bestIndex);
        ~MyTreeFitter_t();
    };
    static size_t CombIndex(const ant::TParticleList& orig, const MyTreeFitter_t&);

    MyTreeFitter_t fitter_pi0;
    MyTreeFitter_t fitter_eta;

    bool ProtonCheck(const TCandidatePtr& c) const;
    bool PhotonCheck(const TCandidatePtr& c) const;

    const LorentzVec target = LorentzVec({0, 0, 0}, ParticleTypeDatabase::Proton.Mass());

    TagChMultiplicity tagChMult;

    class DebugCounter {
    protected:
        TH1D*     hist    = nullptr;

    public:

        class Handle {
        protected:
            unsigned count = 0;
            DebugCounter& parent;

            friend class DebugCounter;
            Handle(DebugCounter& p) noexcept : parent(p) {}

        public:

            Handle(const Handle& other ) noexcept : count(other.count), parent(other.parent) {}

            ~Handle() {
                parent.hist->Fill(count);
            }

            void Count() noexcept { ++count; }
        };

        Handle getHandle() noexcept { return Handle(*this); }

        DebugCounter(TH1D* h) noexcept : hist(h) {}

    };

    DebugCounter dTaggerHitsAccepted;

    struct DebugCounters {

        struct Values {
            unsigned nTaggerLoops         = 0;
            unsigned nTaggerHitAccepted   = 0;
            unsigned nPIDLoopsPerTaggerHit = 0;
            unsigned nKinfitsPerTaggerHit = 0;
            unsigned nKinfitsPerEvent     = 0;
            unsigned nKinFitsOKPerEvent   = 0;
            unsigned nTreeFitsPerEvent    = 0;
            unsigned nHighScoresPerPIDLoop =0;
            unsigned nCoplanarityOKPerPIDLoop = 0;
            unsigned nMissingMassOKPerPIDLoop = 0;
            unsigned nParticlesFoundPerTH = 0;
        };

        Values v;

        void Reset()           noexcept { v = Values(); }
        void EventStart()      noexcept { Reset(); }
        void TaggerLoopBegin() noexcept { ++v.nTaggerLoops; v.nKinfitsPerTaggerHit = 0; v.nPIDLoopsPerTaggerHit = 0; v.nParticlesFoundPerTH = 0; }
        void TaggerHitAccepted() noexcept { ++v.nTaggerHitAccepted; }
        void KinfitHighscore()   noexcept { ++v.nHighScoresPerPIDLoop; }
        void TaggerLoopEnd();
        void PIDLoopBegin()    noexcept { ++v.nPIDLoopsPerTaggerHit;
                                          v.nHighScoresPerPIDLoop = 0;
                                          v.nCoplanarityOKPerPIDLoop = 0;
                                          v.nMissingMassOKPerPIDLoop = 0; }
        void CoplanarityOK()   noexcept { ++v.nCoplanarityOKPerPIDLoop; }
        void MissingMassOK()   noexcept { ++v.nMissingMassOKPerPIDLoop; }
        void KinfitLoopBegin() noexcept { ++v.nKinfitsPerTaggerHit; ++v.nKinfitsPerEvent; }
        void AfterKinfitLoop();
        void TreeFit()         noexcept { ++v.nTreeFitsPerEvent; }
        void ParticlesFound()  noexcept { ++v.nParticlesFoundPerTH; }
        void EventEnd();

        DebugCounters(HistogramFactory& hf);

        TH1D* hTaggerLoops;
        TH1D* hPIDLoopsPerTaggerHit;
        TH1D* hKinfitsPerTaggerHit;
        TH1D* hKinfitsPerEvent;
        TH1D* hKinFitsOKPerEvent;
        TH1D* hTreeFitsPerEvent;
        TH1D* hHighScoresPerPIDLoop;
        TH1D* hCoplanarityOKPerPIDLoop;
        TH1D* hMissingMassOKPerPIDLoop;
        TH1D* hParticlesFoundPerTH;
        TH1D* hTaggerHitsAccepted;
    };

    DebugCounters dCounters;

public:

    OmegaEtaG2(const std::string& name, OptionsPtr opts);
    virtual ~OmegaEtaG2();

    void Finish() override;

    using decaytree_t = ant::Tree<const ParticleTypeDatabase::Type&>;

    struct ReactionChannel_t {
        std::string name="";
        std::shared_ptr<decaytree_t> tree=nullptr;
        int color=kBlack;
        ReactionChannel_t(const std::shared_ptr<decaytree_t>& t, const std::string& n, const int c);
        ReactionChannel_t(const std::shared_ptr<decaytree_t>& t, const int c);
        ReactionChannel_t(const std::string& n);
        ReactionChannel_t() = default;
        ~ReactionChannel_t();
    };

    struct ReactionChannelList_t {
        static const unsigned other_index;
        std::map<unsigned, ReactionChannel_t> channels;
        unsigned identify(const TParticleTree_t &tree) const;
    };

    static ReactionChannelList_t makeChannels();
    static const ReactionChannelList_t reaction_channels;

    std::map<unsigned, TH1D*> stephists;

};

}
}
}

std::string to_string(const ant::analysis::physics::OmegaBase::DataMode& m);
