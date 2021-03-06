#include "sigmaPlus.h"

#include "expconfig/ExpConfig.h"

#include "utils/Combinatorics.h"
#include "utils/ParticleTools.h"
#include "base/vec/LorentzVec.h"
#include "base/Logger.h"

#include "utils/uncertainties/Interpolated.h"

#include "analysis/physics/Plotter.h"
#include "plot/CutTree.h"

#include "slowcontrol/SlowControlVariables.h"
#include "analysis/utils/ParticleTools.h"

using namespace std;
using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::physics;

// define static data members outside class to prevent linker errors
constexpr unsigned sigmaPlus::settings_t::Index_Data;
constexpr unsigned sigmaPlus::settings_t::Index_Signal;
constexpr unsigned sigmaPlus::settings_t::Index_MainBkg;
constexpr unsigned sigmaPlus::settings_t::Index_SumMC;
constexpr unsigned sigmaPlus::settings_t::Index_BkgMC;
constexpr unsigned sigmaPlus::settings_t::Index_unregTree;
constexpr unsigned sigmaPlus::settings_t::Index_brokenTree;
constexpr unsigned sigmaPlus::settings_t::Index_Offset;

const sigmaPlus::named_channel_t sigmaPlus::signal =
    {"SigmaK0S",   ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::SigmaPlusK0s_6g)};
const sigmaPlus::named_channel_t sigmaPlus::mainBackground =
    {"3Pi0Prod",    ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::ThreePi0_6g)};
const std::vector<sigmaPlus::named_channel_t> sigmaPlus::otherBackgrounds =
{
    {"Eta3Pi0",     ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::Eta_3Pi0_6g)},
    {"2Pi04g",       ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::TwoPi0_4g)},
    {"EtaPi04g",     ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::Pi0Eta_4g)},
    {"Eta4Pi0",      ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::Eta_4Pi0_8g)},
    {"EtaPi04gPiPi", ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::Pi0Eta_2gPiPi2g)},
    {"Pi0PiPi",      ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::Pi0PiPi_2gPiPi)},
    {"2Pi0PiPi",     ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::TwoPi0PiPi_4gPiPi)},
    {"Etap3Pi0",     ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::EtaPrime_3Pi0_6g)},
    {"EtapEta2Pi0",  ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::EtaPrime_2Pi0Eta_6g)}
};

auto getEgamma = [] (const TParticleTree_t& tree)
{
    return tree->Get()->Ek();
};


string sigmaPlus::getOtherChannelNames(const unsigned i)
{
    if (i == 8)
        return "unregistered tree";
    if (i == 9)
        return "broken pluto tree";
    if (i>=10 && i<10+otherBackgrounds.size())
            return otherBackgrounds.at(i-10).Name;
    return "error";
}

auto reducedChi2 = [](const APLCON::Result_t& ares)
{
    return 1. * ares.ChiSquare / ares.NDoF;
};


//auto getLorentzSumUnfitted = [](const vector<utils::TreeFitter::tree_t>& nodes)
//{
//    vector<TLorentzVector> acc;
//    for ( const auto& node: nodes)
//    {
//        LorentzVec temp({0,0,0},0);
//        for ( const auto& ph: node->Daughters())
//        {
//            temp+=(*(ph->Get().Leave->Particle));
//        }
//        acc.push_back(temp);
//    }
//    return acc;
//};

//auto getTLorentz = [] (const utils::TreeFitter::tree_t& node)
//{
//    return node->Get().LVSum;
//};

auto getTreeFitPhotonIndices = [] (const TParticleList& orig_Photons,
                                   const utils::TreeFitter& treeFitter)
{
    const auto allP = treeFitter.GetFitParticles();
    const vector<utils::Fitter::FitParticle> fitPhotons(allP.begin()+1,
                                                        allP.end());

    vector<unsigned> combination;
    for (unsigned iFit = 0; iFit < fitPhotons.size(); ++iFit)
    {
        for (unsigned iOrig = 0; iOrig < orig_Photons.size(); ++iOrig)
        {
            if ( fitPhotons.at(iFit).Particle == orig_Photons.at(iOrig))
            {
                combination.push_back(iOrig);
                continue;
            }
        }
    }
    return combination;
};

auto getLorentzSumFitted = [](const vector<utils::TreeFitter::tree_t>& nodes)
{
    vector<TLorentzVector> acc;
    for ( const auto& node: nodes)
    {
        acc.push_back(node->Get().LVSum);
    }
    return acc;
};



sigmaPlus::sigmaPlus(const string& name, ant::OptionsPtr opts):
    Physics(name, opts),
    phSettings(),
    flag_mc(opts->Get<bool>("mc", false)),
    tagger(ExpConfig::Setup::GetDetector<TaggerDetector_t>()),
    uncertModelData(// use Interpolated, based on Sergey's model
                    utils::UncertaintyModels::Interpolated::makeAndLoad(
                        utils::UncertaintyModels::Interpolated::Type_t::Data,
                        // use Sergey as starting point
                        make_shared<utils::UncertaintyModels::FitterSergey>()
                        )),
    uncertModelMC(// use Interpolated, based on Sergey's model
                  utils::UncertaintyModels::Interpolated::makeAndLoad(
                      utils::UncertaintyModels::Interpolated::Type_t::MC,
                      // use Sergey as starting point
                      make_shared<utils::UncertaintyModels::FitterSergey>()
                      )),
    fitterEMB(                          uncertModelData, true ),
    fitter3Pi0(mainBackground.DecayTree, uncertModelData, true ),
    fitterK0S(ParticleTypeTreeDatabase::Get(ParticleTypeTreeDatabase::Channel::gp_pPi0K0S_3Pi0_6g),
              uncertModelData, true)
{

    fitterEMB.SetZVertexSigma(phSettings.fitter_ZVertex);
    fitter3Pi0.SetZVertexSigma(phSettings.fitter_ZVertex);
    fitterK0S.SetZVertexSigma(phSettings.fitter_ZVertex);

    auto extractS = [] ( vector<utils::TreeFitter::tree_t>& nodes,
                         const utils::TreeFitter& fitter,
                         const ParticleTypeDatabase::Type& mother,
                         const ParticleTypeDatabase::Type& daughterSelection )
    {
        const auto head = fitter.GetTreeNode(mother);
        for (const auto& daughter: head->Daughters())
        {
            if (daughter->Get().TypeTree->Get() == daughterSelection)
                nodes.emplace_back(daughter);
        }
    };



    extractS(pionsFitter3Pi0, fitter3Pi0,
             ParticleTypeDatabase::BeamProton,
             ParticleTypeDatabase::Pi0);

    extractS(pionsFitterK0S, fitterK0S,
             ParticleTypeDatabase::BeamProton,
             ParticleTypeDatabase::Pi0);

    K0SK0S = fitterK0S.GetTreeNode(ParticleTypeDatabase::K0s);
    PionsK0S = K0SK0S->Daughters();

//    sigmaFitterSigmaPlus = fitterSigmaPlus.GetTreeNode(ParticleTypeDatabase::SigmaPlus);

//    // be lazy and catch complete class...
//    fitterSigmaPlus.SetIterationFilter([this] () {
//        const auto sigmaPlus_cut = ParticleTypeDatabase::SigmaPlus.GetWindow(200);
//        const auto K0s_cut = ParticleTypeDatabase::K0s.GetWindow(100);
//        auto ok = sigmaPlus_cut.Contains(sigmaFitterSigmaPlus->Get().LVSum.M()) &&
//                  K0s_cut.Contains(kaonFitterSigmaPlus->Get().LVSum.M());
//        return ok;
//    });

    promptrandom.AddPromptRange(phSettings.Range_Prompt);
    for ( const auto& range: phSettings.Ranges_Random)
        promptrandom.AddRandomRange(range);

    hist_steps          = HistFac.makeTH1D("steps","","# evts.",BinSettings(1,0,0),"steps");
    hist_channels       = HistFac.makeTH1D("channels","","# evts.",BinSettings(1,0,0),"channels");
    hist_channels_end   = HistFac.makeTH1D("channel-selected","","# evts.",BinSettings(1,0,0),"channels_end");

    hist_neutrals_channels
            = HistFac.makeTH2D("# neutral candidates","","# neutrals",BinSettings(1,0,0),BinSettings(15),"channels_neutrals");


    tree.CreateBranches(HistFac.makeTTree(phSettings.Tree_Name));
    seenSignal.CreateBranches(HistFac.makeTTree(seenSignal.treeName()));
    recSignal.CreateBranches(HistFac.makeTTree(recSignal.treeName()));

    if (!flag_mc)
    {
        slowcontrol::Variables::TaggerScalers->Request();
        slowcontrol::Variables::Trigger->Request();
        slowcontrol::Variables::TaggEff->Request();
    }
    fitterEMB.SetUncertaintyModel(flag_mc ? uncertModelMC : uncertModelData);
    fitter3Pi0.SetUncertaintyModel(flag_mc ? uncertModelMC : uncertModelData);

}

sigmaPlus::fitRatings_t sigmaPlus::apply3Pi0Fit(
        const utils::ProtonPhotonCombs::comb_t& protonSelection,
        const double Ebeam)
{

    fitter3Pi0.PrepareFits(Ebeam,
                       protonSelection.Proton,
                       protonSelection.Photons);
    APLCON::Result_t result;
    auto best_prob = std_ext::NaN;
    sigmaPlus::fitRatings_t fr(0,0,0,false,
                               {},
                               {},{});
    while(fitter3Pi0.NextFit(result))
        if (   (result.Status    == APLCON::Result_Status_t::Success)
               && (std_ext::copy_if_greater(best_prob,result.Probability)))
        {
            fr = sigmaPlus::fitRatings_t(best_prob,reducedChi2(result),result.NIterations,
                                         result.Status == APLCON::Result_Status_t::Success,
                                         TSimpleParticle(*fitter3Pi0.GetFittedProton()),
                                         getLorentzSumFitted(pionsFitter3Pi0),
                                         getTreeFitPhotonIndices(protonSelection.Photons,fitter3Pi0));
        }

    return fr;
}

std::pair<LorentzVec, sigmaPlus::fitRatings_t> sigmaPlus::applyK0SFit(
        const utils::ProtonPhotonCombs::comb_t& protonSelection,
        const double Ebeam)
{
    fitterK0S.PrepareFits(Ebeam,
                       protonSelection.Proton,
                       protonSelection.Photons);
    APLCON::Result_t result;
    auto best_prob = std_ext::NaN;
    sigmaPlus::fitRatings_t fr(0,0,0,false,
                               {},
                               {},{});
    LorentzVec K0S{{0,0,0},0};

    while(fitterK0S.NextFit(result))
        if (   (result.Status    == APLCON::Result_Status_t::Success)
               && (std_ext::copy_if_greater(best_prob,result.Probability)))
        {

            fr = sigmaPlus::fitRatings_t(best_prob,reducedChi2(result),result.NIterations,
                                         result.Status == APLCON::Result_Status_t::Success,
                                         TSimpleParticle(*fitterK0S.GetFittedProton()),
                                         getLorentzSumFitted(pionsFitter3Pi0),
                                         getTreeFitPhotonIndices(protonSelection.Photons,fitterK0S));
            tree.K0S       = K0SK0S->Get().LVSum;
            tree.K0S_Sigma = fitterK0S.GetTreeNode(ParticleTypeDatabase::Proton)->Get().LVSum;
            tree.K0S_Sigma_EMB = *fitterEMB.GetFittedProton();

            const auto produced = fitterK0S.GetTreeNode(ParticleTypeDatabase::BeamTarget)->Daughters();
            for ( const auto& p: produced)
            {
                if (p->Get().TypeTree->Get() == ParticleTypeDatabase::Pi0)
                {
                    tree.K0S_Sigma() += p->Get().LVSum;
                    for (const auto& d: p->Daughters())
                    {
                        for (const auto& ep: fitterEMB.GetFitParticles())
                        {
                            if ( d->Get().Leaf->Particle == ep.Particle)
                                tree.K0S_Sigma_EMB() += *ep.AsFitted();
                        }
                    }

                    break;
                }
            }

            LorentzVec kinfitK0S{{0,0,0},0};

            assert(PionsK0S.size() == 2);
            unsigned nph = 0;
            for (auto ep: fitterEMB.GetFitParticles())
            {
                for (const auto& pion: PionsK0S)
                    for (auto d: pion->Daughters())
                    {
                        assert(d->Get().Leaf->Particle->Type() == ParticleTypeDatabase::Photon);
                        if (d->Get().Leaf->Particle == ep.Particle)
                        {
                            kinfitK0S+=*ep.AsFitted();
                            nph++;
                        }
                    }
            }
            assert(nph == 4);

            K0S = kinfitK0S;
            tree.K0S_EMB() = kinfitK0S;
            tree.K0S_cosTheta_EMB() = cos(cmsBoost(Ebeam,kinfitK0S).Theta());

        }

    return { K0S , fr };
}


void sigmaPlus::ProcessEvent(const ant::TEvent& event, manager_t&)
{
    const auto& data   = event.Reconstructed();

    triggersimu.ProcessEvent(event);

    FillStep("seen");

    tree.CBESum = triggersimu.GetCBEnergySum();
    // check if mc-flag ist set properly:
    if ( flag_mc != data.ID.isSet(TID::Flags_t::MC))
        throw runtime_error("provided mc flag does not match input files!");

    const auto& particleTree = event.MCTrue().ParticleTree;
    //===================== TreeMatching   ====================================================
    string trueChannel = "data";
    tree.MCTrue = phSettings.Index_Data;
    if (flag_mc)
    {
        tree.MCTrue() = phSettings.Index_brokenTree;
        trueChannel = "no Pluto tree";
    }
    if (particleTree)
    {

        if (particleTree->IsEqual(signal.DecayTree,utils::ParticleTools::MatchByParticleName))
        {
            const auto& taggerhits = event.MCTrue().TaggerHits;

            if ( taggerhits.size() > 1)
            {
                LOG(INFO) << event ;
                throw runtime_error("mc should always have no more than one Taggerbin, check mctrue!");
            }

            // pluto reader generates only taggerhits, if beamtarget->Ek() is within
            // [Etagg_min,Etagg_max]!
            // see PlutoReader::CopyPluto(TEventData& mctrue)!!!
            if ( taggerhits.size() == 1)
            {
                seenSignal.Egamma()      = getEgamma(particleTree);
                seenSignal.TaggerBin()   = taggerhits[0].Channel;
                seenSignal.CosThetaK0S() = cos( cmsBoost(
                                                    seenSignal.Egamma(), *utils::ParticleTools::FindParticle(ParticleTypeDatabase::K0s, particleTree)
                                                    ).Theta() );
                seenSignal.Tree->Fill();
            }
            tree.MCTrue = phSettings.Index_Signal;
            trueChannel = signal.Name;
        }
        else if (particleTree->IsEqual(mainBackground.DecayTree,utils::ParticleTools::MatchByParticleName))
        {
            tree.MCTrue = phSettings.Index_MainBkg;
            trueChannel = mainBackground.Name;
        }
        else
        {
            auto index = phSettings.Index_Offset;
            bool found = false;
            for (const auto& otherChannel:otherBackgrounds)
            {
                if (particleTree->IsEqual(otherChannel.DecayTree,utils::ParticleTools::MatchByParticleName))
                {
                    tree.MCTrue = index;
                    trueChannel = otherChannel.Name;
                    found = true;
                }
                index++;
            }
            if (!found)
            {
                tree.MCTrue = phSettings.Index_unregTree;
                trueChannel = utils::ParticleTools::GetDecayString(particleTree) + ": unregistered";
            }
        }
    }
    hist_channels->Fill(trueChannel.c_str(),1);

    //simulate cb-esum-trigger
    if (!triggersimu.HasTriggered())
        return;
    FillStep("Triggered");

    if (tools::cutOn("N_{cands}",phSettings.Cut_NCands,data.Candidates.size(),hist_steps)) return;


    //===================== Reconstruction ====================================================
    tree.CBAvgTime = triggersimu.GetRefTiming();

    utils::ProtonPhotonCombs proton_photons(data.Candidates);

    for ( const auto& taggerHit: data.TaggerHits )
    {
        FillStep("[T] seen taggerhits");

        promptrandom.SetTaggerTime(triggersimu.GetCorrectedTaggerTime(taggerHit));

        if (promptrandom.State() == PromptRandom::Case::Outside)
            continue;
        FillStep("[T] taggerhit inside");

        tree.Tagg_Ch  = static_cast<unsigned>(taggerHit.Channel);
        tree.Tagg_E   = taggerHit.PhotonEnergy;
        tree.Tagg_W   = promptrandom.FillWeight();

        {
            const auto taggEff = slowcontrol::Variables::TaggEff->Get(taggerHit.Channel);
            tree.Tagg_Eff      = taggEff.Value;
            tree.Tagg_EffErr  = taggEff.Error;
        }

        auto selections =  proton_photons()
                           .FilterMult(phSettings.nPhotons,40)
                           .FilterMM(taggerHit, phSettings.Cut_MM)
//                           .FilterCustom([] (const utils::ProtonPhotonCombs::comb_t& comb) { return std_ext::radian_to_degree(comb.Proton->Theta()) > 65;})
                           ;
        if (selections.empty())
            continue;

        FillStep("[T] passed prefilter");

        auto bestFitProb = 0.0;
        auto bestFound   = false;
        for ( const auto& selection: selections)
        {
            FillStep("[T] [p] start proton loop");
            // constraint fits
            //4C-Fit
            const auto EMB_result = fitterEMB.DoFit(taggerHit.PhotonEnergy, selection.Proton, selection.Photons);
            if (!(EMB_result.Status == APLCON::Result_Status_t::Success))
                continue;
            FillStep("[T] [p] EMB-fit success");
            if (tools::cutOn("[T] [p]  EMB-prob",phSettings.Cut_EMB_prob,EMB_result.Probability,hist_steps)) continue;
            //7C-Fit
            const auto sigFitRatings = apply3Pi0Fit(selection,taggerHit.PhotonEnergy);
            if (!(sigFitRatings.FitOk))
                continue;
            FillStep("[T] [p] Tree-Fit succesful");
            const auto k0sFitRatings = applyK0SFit(selection,taggerHit.PhotonEnergy);
            if (!(k0sFitRatings.second.FitOk))
                continue;
            FillStep("[T] [p] 8C-Fit succesful");
            //status:
            const auto prob = k0sFitRatings.second.Prob;

            if ( prob > bestFitProb )
            {
                {
                    tree.K0S_prob = k0sFitRatings.second.Prob;
                    const auto boostedK0 = cmsBoost(taggerHit.PhotonEnergy,tree.K0S());
                    tree.K0S_cosTheta = cos(boostedK0.Theta());
                    recSignal.CosThetaK0S = seenSignal.CosThetaK0S();

                    tree.K0S_photonSum = accumulate(k0sFitRatings.second.Intermediates.begin(),
                                                 k0sFitRatings.second.Intermediates.end(),
                                                 LorentzVec({0,0,0},0));
                    tree.K0S_IM6g = tree.K0S_photonSum().M();
                    tree.K0S_pions = k0sFitRatings.second.Intermediates;

                    bestFound = k0sFitRatings.second.FitOk;
                }

                bestFitProb = prob;
                tree.SetRaw(selection);
                tree.SetEMB(fitterEMB,EMB_result);
                tree.SetSIG(sigFitRatings);
            } // endif best SIG - treefit

        } // proton - candidate - loop


        if (bestFound)
        {
            FillStep("[T] p identified");
            tree.ChargedClusterE() = tools::getChargedClusterE(data.Clusters);
            tree.ChargedCandidateE() = tools::getCandidateVetoE(data.Candidates);

            const auto neutralCands = tools::getNeutral(data,phSettings.vetoThreshE);

            tree.Neutrals() = neutralCands.size();

            if (flag_mc)
            {
                tree.ExpLivetime() = 1;
            }
            else if(slowcontrol::Variables::TaggerScalers->HasChanged())
            {
                tree.ExpLivetime()  = slowcontrol::Variables::Trigger->GetExpLivetime();
            }
            tree.NCands() = data.Candidates.size();

            recSignal.Egamma()      = seenSignal.Egamma();
            recSignal.TaggerBin()   = seenSignal.TaggerBin();

            hist_channels_end->Fill(trueChannel.c_str(),1);
            hist_neutrals_channels->Fill(trueChannel.c_str(),neutralCands.size(),1);
            recSignal.Tree->Fill();
            tree.Tree->Fill();
        }

    } // taggerHits - loop
}

void sigmaPlus::ShowResult()
{
    const auto colz = drawoption("colz");

    canvas("summary")
            << hist_steps
            << hist_channels
            << hist_channels_end
            << TTree_drawable(tree.Tree,"IM6g")
            << endc;

    canvas("channels")
            << colz
            << hist_neutrals_channels
            << endc;
}

void sigmaPlus::PionProdTree::SetRaw(const utils::ProtonPhotonCombs::comb_t& selection)
{
    proton()      = TSimpleParticle(*selection.Proton);
    photons()     = TSimpleParticle::TransformParticleList(selection.Photons);

    ProtonVetoE()  = selection.Proton->Candidate->VetoEnergy;
    PionPIDVetoE() = 0.;
    for (const auto& g: selection.Photons)
    {
        for (const auto& c: g->Candidate->Clusters)
        {
            if (c.DetectorType == Detector_t::Type_t::PID)
                PionPIDVetoE += c.Energy;
        }
    }

    photonSum()   = selection.PhotonSum;
    IM6g()        = photonSum().M();
    proton_MM()   = selection.MissingMass;
    DiscardedEk() = selection.DiscardedEk;
}



void sigmaPlus::PionProdTree::SetEMB(const utils::KinFitter& kF, const APLCON::Result_t& result)
{

    const auto fittedPhotons = kF.GetFittedPhotons();
    const auto phE           = kF.GetFittedBeamE();

    EMB_proton     = TSimpleParticle(*(kF.GetFittedProton()));
    EMB_photons    = TSimpleParticle::TransformParticleList(fittedPhotons);
    EMB_photonSum  = accumulate(EMB_photons().begin(),EMB_photons().end(),LorentzVec({0,0,0},0));
    EMB_IM6g       = EMB_photonSum().M();
    EMB_Ebeam      = phE;
    EMB_iterations = result.NIterations;
    EMB_prob       = result.Probability;
    EMB_chi2       = reducedChi2(result);
}

void sigmaPlus::PionProdTree::SetSIG(const sigmaPlus::fitRatings_t& fitRating)
{
    SIG_prob        = fitRating.Prob;
    SIG_chi2        = fitRating.Chi2;
    SIG_iterations  = fitRating.Niter;
    SIG_pions       = fitRating.Intermediates;
    SIG_photonSum   = accumulate(fitRating.Intermediates.begin(),
                                 fitRating.Intermediates.end(),
                                 LorentzVec({0,0,0},0));
    SIG_IM6g        = SIG_photonSum().M();
    SIG_proton      = fitRating.Proton;
    SIG_combination = fitRating.PhotonCombination;
}

using namespace ant::analysis::plot;

using sigmaPlus_PlotBase = TreePlotterBase_t<sigmaPlus::PionProdTree>;

class sigmaPlus_Test: public sigmaPlus_PlotBase{

protected:

    TH1D* m3pi0  = nullptr;

    unsigned nBins  = 200u;

    bool testCuts() const
    {
        return (
                    true                      &&
                    tree.SIG_prob() < 0.1     &&
                    tree.SIG_IM6g() < 600.0
                );
    }

    // Plotter interface
public:
    sigmaPlus_Test(const string& name, const WrapTFileInput& input, OptionsPtr opts):
        sigmaPlus_PlotBase (name,input,opts)
    {
        m3pi0  = HistFac.makeTH1D("m(3#pi^{0})","m(3#pi^0) [MeV]","#",
                                  BinSettings(nBins,400,1100));
    }

    virtual void ProcessEntry(const long long entry) override
    {
        t->GetEntry(entry);

        if (testCuts()) return;


        m3pi0->Fill(tree.IM6g);
    }
    virtual void ShowResult() override
    {
        canvas("view")
                << m3pi0
                << endc;
    }
};

AUTO_REGISTER_PHYSICS(sigmaPlus)
AUTO_REGISTER_PLOTTER(sigmaPlus_Test)
