#include "ProtonVertexTest.h"

#include "base/vec/LorentzVec.h"
#include "base/std_ext/math.h"
#include "base/Logger.h"

#include "utils/uncertainties/Interpolated.h"

#include "expconfig/setups/Setup.h"

#include "analysis/physics/production/triplePi0.h"

#include "analysis/physics/scratch/wolfes/tools/tools.h"



using namespace std;
using namespace ant;
using namespace ant::std_ext;
using namespace ant::analysis;
using namespace ant::analysis::physics;


ProtonVertexTest::ProtonVertexTest(const string& name, OptionsPtr opts):
     Physics(name, opts),
     uncertModel(utils::UncertaintyModels::Interpolated::makeAndLoad()),
     kinFitterEMB("fitterEMB", 6, uncertModel, true)
{
    kinFitterEMB.SetZVertexSigma(fitter_ZVertex);

    const auto setup = ant::ExpConfig::Setup::GetLastFound();
    if(!setup) {
        throw std::runtime_error("No Setup found");
    }

    promptrandom.AddPromptRange(Range_Prompt);
    for ( const auto& range: Ranges_Random)
        promptrandom.AddRandomRange(range);

    hist_steps = HistFac.makeTH1D("steps","","# evts.",BinSettings(1,0,0),"steps");
    hist_theta = HistFac.makeTH1D("theta","#theta_{p} [#circ]","#",BinSettings(180));

    tree.CreateBranches(HistFac.makeTTree("ptree"));
}

void ProtonVertexTest::ProcessEvent(const TEvent& event, manager_t&)
{
    triggersimu.ProcessEvent(event);

    const auto& data   = event.Reconstructed();

    FillStep("seen");

    if (cutOn("ncands", NCands, data.Candidates.size())) return;

    for ( const auto& taggerHit: data.TaggerHits )
    {
        FillStep("seen taggerhits");


        promptrandom.SetTaggerHit(triggersimu.GetCorrectedTaggerTime(taggerHit));
        if (promptrandom.State() == PromptRandom::Case::Outside)
            continue;
        tree.prW() = promptrandom.FillWeight();
        FillStep("Promt");


        tree.prob() = 0;
        for ( auto i_proton: data.Candidates.get_iter())
        {
            FillStep("proton Candidate");

            //helper for proton idf from other physics class:
            const auto selection =  tools::getProtonSelection(i_proton, data.Candidates,
                                                              taggerHit.GetPhotonBeam(),
                                                              taggerHit.PhotonEnergy);

            if (cutOn("p-copl",     ProtonCopl, selection.Copl_pg))       continue;
            if (cutOn("p-MM",       MM,         selection.Proton_MM.M())) continue;
            if (cutOn("p-mm-angle", MMAngle,    selection.Angle_pMM))     continue;

            auto kinfit_result = kinFitterEMB.DoFit(selection.Tagg_E, selection.Proton, selection.Photons);
            if (cutOn("EMB prob",   EMB_prob,   kinfit_result.Probability))  continue;

            if (kinfit_result.Probability > tree.prob())
            {
                tree.prob()           = kinfit_result.Probability;
                tree.coplanarity()    = selection.Copl_pg;
                tree.ekin()           = selection.Proton->Ek();
                tree.mm_angle()       = selection.Angle_pMM;
                tree.mm_im()          = selection.Proton_MM.M();
                tree.theta()          = kinFitterEMB.GetFittedProton()->Theta();
                tree.zvertex()        = kinFitterEMB.GetFittedZVertex();
                tree.photonVeto()     = tools::getPhotonVetoEnergy(selection);
                tree.corrPhotonVeto() = tools::getPhotonVetoEnergy(selection,true);
            }
        } // proton
        hist_theta->Fill(radian_to_degree(tree.theta()));
        tree.Tree->Fill();
    } // tagger
}

void ProtonVertexTest::ShowResult()
{
    canvas("summary")
            << hist_steps
            << hist_theta
            << TTree_drawable(tree.Tree,"corrPhotonVeto")
            << TTree_drawable(tree.Tree,"zvertex")
            << endc;
}

AUTO_REGISTER_PHYSICS(ProtonVertexTest)
