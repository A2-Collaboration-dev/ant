#include "TwoPi0_MCSmearing.h"

#include "utils/ParticleTools.h"
#include "expconfig/ExpConfig.h"
#include "utils/Combinatorics.h"


using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::physics;
using namespace ant::std_ext;
using namespace std;


TwoPi0_MCSmearing::TwoPi0_MCSmearing(const string& name, OptionsPtr opts) :
    Physics(name, opts),
    promptrandom(*ExpConfig::Setup::GetLastFound())
{
    steps = HistFac.makeTH1D("Steps","","#",BinSettings(15),"steps");

    const BinSettings pi0bins(120,80,200);
    const BinSettings thetabins_cb  (35, cos(degree_to_radian(160.0)), cos(degree_to_radian(20.0)));
    const BinSettings thetabins_taps(10, cos(degree_to_radian( 20.0)), cos(degree_to_radian( 0.0)));
    const BinSettings Ebins_theta  (16,0,1600);
    const BinSettings Ebins_element(16,0,1600);

    const auto& cb   = ExpConfig::Setup::GetDetector(Detector_t::Type_t::CB);
    cb_pi0_channel   = HistFac.makeTH2D("CB Pi0",       "m(2#gamma) [MeV]", "Element", pi0bins, BinSettings(cb->GetNChannels()),   "cb_pi0");
    cb_pi0_thetaE    = HistFac.makeTH3D("CB E Theta",   "m(2#gamma) [MeV]", "E_{#gamma} [MeV]", "#cos(#theta)", pi0bins, Ebins_theta, thetabins_cb, "cb_pi0_ETheta");
    cb_pi0_EElement  = HistFac.makeTH3D("CB E element", "m(2#gamma) [MeV]", "E_{#gamma} [MeV]", "Element", pi0bins, Ebins_element, BinSettings(cb->GetNChannels()), "cb_pi0_E_Element");
    cb_channel_correlation = HistFac.makeTH2D("CB Element Correlation",   "Element", "Element", BinSettings(cb->GetNChannels()), BinSettings(cb->GetNChannels()),   "cb_corr");

    const auto& taps = ExpConfig::Setup::GetDetector(Detector_t::Type_t::TAPS);
    taps_pi0_channel  = HistFac.makeTH2D("TAPS Pi0",       "m(2#gamma) [MeV]", "", pi0bins, BinSettings(taps->GetNChannels()), "taps_pi0");
    taps_pi0_thetaE   = HistFac.makeTH3D("TAPS E Theta",   "m(2#gamma) [MeV]", "E_{#gamma} [MeV]", "#cos(#theta)", pi0bins, Ebins_theta, thetabins_taps, "taps_pi0_ETheta");
    taps_pi0_EElement = HistFac.makeTH3D("TAPS E element", "m(2#gamma) [MeV]", "E_{#gamma} [MeV]", "Element", pi0bins, Ebins_element, BinSettings(taps->GetNChannels()), "taps_pi0_E_Element");
    taps_channel_correlation = HistFac.makeTH2D("TAPS Element Correlation",   "Element", "Element", BinSettings(taps->GetNChannels()), BinSettings(taps->GetNChannels()),   "taps_corr");


}

void TwoPi0_MCSmearing::ProcessEvent(const TEvent& event, manager_t&)
{
    triggersimu.ProcessEvent(event);

    const auto& data = event.Reconstructed();

    steps->Fill("Seen",1);

    // cut on energy sum and number of candidates

    if(!triggersimu.HasTriggered())
        return;
    steps->Fill("Triggered",1);

    const auto& cands = data.Candidates;


    for(auto& c : cands) {
        const auto& cl = c.FindCaloCluster();
        if(cl->HasFlag(TCluster::Flags_t::TouchesHoleCentral))
            return;
    }

    // prompt-random subtraction is easy
    double sum_tagger_weight = 0;
    for(const TTaggerHit& taggerhit : data.TaggerHits) {

        steps->Fill("Seen taggerhits",1.0);

        promptrandom.SetTaggerTime(triggersimu.GetCorrectedTaggerTime(taggerhit));
        if(promptrandom.State() == PromptRandom::Case::Outside)
            continue;

        sum_tagger_weight += promptrandom.FillWeight();

    } // Loop taggerhits

    if(sum_tagger_weight==0)
        return;

    for( auto comb = analysis::utils::makeCombination(cands.get_ptr_list(),2); !comb.Done(); ++comb ) {
        const auto& c1 = comb.at(0);
        const auto& c2 = comb.at(1);

        const TParticle g1(ParticleTypeDatabase::Photon, c1);
        const TParticle g2(ParticleTypeDatabase::Photon, c2);
        const auto ggIM = (g1 + g2).M();

        FillIM(*c1, ggIM, sum_tagger_weight);
        FillIM(*c2, ggIM, sum_tagger_weight);

        FillCorrelation(*c1, *c2, sum_tagger_weight);
    }
}

void TwoPi0_MCSmearing::ShowResult()
{
    HistogramFactory::DirStackPush dir(HistFac);

    canvas(GetName())
            << steps
            << drawoption("colz")
            << cb_pi0_channel
            << taps_pi0_channel
            << endc;
}

void TwoPi0_MCSmearing::FillIM(const TCandidate& c, double IM, double w)
{
    const auto& cluster = c.FindCaloCluster();


    if(c.Detector & Detector_t::Type_t::CB) {
        cb_pi0_channel->Fill( IM, cluster->CentralElement, w);
        cb_pi0_thetaE->Fill(  IM, c.CaloEnergy, cos(c.Theta), w);
        cb_pi0_EElement->Fill(IM, c.CaloEnergy, cluster->CentralElement, w);
    } else if(c.Detector & Detector_t::Type_t::TAPS) {
        taps_pi0_channel->Fill( IM, cluster->CentralElement, w);
        taps_pi0_thetaE->Fill(  IM, c.CaloEnergy, cos(c.Theta), w);
        taps_pi0_EElement->Fill(IM, c.CaloEnergy, cluster->CentralElement, w);
    }
}

void TwoPi0_MCSmearing::FillCorrelation(const TCandidate& c1, const TCandidate& c2, double w)
{
    if((c1.Detector & Detector_t::Any_t::Calo) != (c2.Detector & Detector_t::Any_t::Calo))
        return;

    const auto& cluster1 = c1.FindCaloCluster();
    const auto& cluster2 = c2.FindCaloCluster();

    if(c1.Detector & Detector_t::Type_t::CB) {
        cb_channel_correlation->Fill(cluster1->CentralElement, cluster2->CentralElement, w);
        cb_channel_correlation->Fill(cluster2->CentralElement, cluster1->CentralElement, w);
    } else  if(c1.Detector & Detector_t::Type_t::TAPS) {
        taps_channel_correlation->Fill(cluster1->CentralElement, cluster2->CentralElement, w);
        taps_channel_correlation->Fill(cluster2->CentralElement, cluster1->CentralElement, w);
    }
}

AUTO_REGISTER_PHYSICS(TwoPi0_MCSmearing)
