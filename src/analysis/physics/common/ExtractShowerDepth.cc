#include "ExtractShowerDepth.h"

#include "plot/root_draw.h"
#include "expconfig/ExpConfig.h"
#include "expconfig/detectors/CB.h"
#include "expconfig/detectors/TAPS.h"


using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::physics;
using namespace std;

ExtractShowerDepth::ExtractShowerDepth(const string& name, OptionsPtr opts) :
    Physics(name, opts)
{

    steps = HistFac.makeTH1D("Steps","","#",BinSettings(10),"steps");
    t.CreateBranches(HistFac.makeTTree("tree"));
}

void ExtractShowerDepth::ProcessEvent(const TEvent& event, manager_t&)
{
    const TEventData& recon = event.Reconstructed();
    const TEventData& mctrue = event.MCTrue();

    steps->Fill("Seen",1);

    if(mctrue.Particles.GetAll().size() != 1)
        return;
    steps->Fill("MC True nParticles==1",1);

    auto& true_particle = mctrue.Particles.GetAll().front();

    TClusterPtr calocluster;

    for(auto& cluster : recon.Clusters.get_iter()) {
        if(cluster->DetectorType & Detector_t::Any_t::Calo)
            calocluster = cluster;
    }
    if(!calocluster)
        return;
    steps->Fill("nCaloClusters==1",1);

    auto theta = calocluster->Position.Theta();
    auto Ek = calocluster->Energy;
    auto z_vertex = mctrue.Target.Vertex.z;

    t.TrueZVertex = z_vertex;
    t.TrueTheta = std_ext::radian_to_degree(true_particle->Theta());
    t.TrueEk = true_particle->Ek();
    t.Theta = std_ext::radian_to_degree(theta);
    t.Ek = Ek;

    // copied from Fitter::FitParticle::GetVector
    t.ThetaCorr = std_ext::NaN;
    if(calocluster->DetectorType == Detector_t::Type_t::CB) {
        static auto cb = ExpConfig::Setup::GetDetector<expconfig::detector::CB>();
        auto elem = cb->GetClusterElement(calocluster->CentralElement);
        const auto R  = cb->GetInnerRadius() + 1.0*elem->RadiationLength*std::log2(Ek/elem->CriticalE);
        t.ThetaCorr = std::acos(( R*std::cos(theta) - z_vertex) / R );
    }
    else if(calocluster->DetectorType == Detector_t::Type_t::TAPS) {
        static auto taps = ExpConfig::Setup::GetDetector<expconfig::detector::TAPS>();
        auto elem = taps->GetClusterElement(calocluster->CentralElement);
        const auto Z  =  taps->GetZPosition() + elem->RadiationLength*std::log2(Ek/elem->CriticalE);
        t.ThetaCorr = std::atan( Z*std::tan(theta) / (Z - z_vertex));
    }
    t.ThetaCorr = std_ext::radian_to_degree(t.ThetaCorr());

    t.Tree->Fill();

}

void ExtractShowerDepth::ShowResult()
{
    canvas(GetName()) << steps
                      << drawoption("colz")
                      << TTree_drawable(t.Tree, "Ek:TrueEk >> (1000,0,1600,1000,0,1600)","")
                      << TTree_drawable(t.Tree, "Theta:TrueTheta >> (180,0,180,180,0,180)","")
                      << TTree_drawable(t.Tree, "ThetaCorr:TrueTheta >> (180,0,180,180,0,180)","")
                      << endc;
    const string bins_theta = ">>(180,0,180,40,-10,10)";
    {
        string formula = "(Theta-TrueTheta):TrueTheta";
        canvas(GetName()+" ZVertex")
                << drawoption("colz")
                << TTree_drawable(t.Tree, formula+bins_theta,"")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2")
                << TTree_drawable(t.Tree, formula+bins_theta,"-2<TrueZVertex && TrueZVertex<-1")
                << TTree_drawable(t.Tree, formula+bins_theta,"-1<TrueZVertex && TrueZVertex<+0")
                << TTree_drawable(t.Tree, formula+bins_theta,"+0<TrueZVertex && TrueZVertex<+1")
                << TTree_drawable(t.Tree, formula+bins_theta,"+1<TrueZVertex && TrueZVertex<+2")
                << TTree_drawable(t.Tree, formula+bins_theta,"+2<TrueZVertex && TrueZVertex<+3")
                << endc;
    }
    {
        string formula = "(ThetaCorr-TrueTheta):TrueTheta";
        canvas(GetName()+" ZVertex ThetaCorr")
                << drawoption("colz")
                << TTree_drawable(t.Tree, formula+bins_theta,"")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2")
                << TTree_drawable(t.Tree, formula+bins_theta,"-2<TrueZVertex && TrueZVertex<-1")
                << TTree_drawable(t.Tree, formula+bins_theta,"-1<TrueZVertex && TrueZVertex<+0")
                << TTree_drawable(t.Tree, formula+bins_theta,"+0<TrueZVertex && TrueZVertex<+1")
                << TTree_drawable(t.Tree, formula+bins_theta,"+1<TrueZVertex && TrueZVertex<+2")
                << TTree_drawable(t.Tree, formula+bins_theta,"+2<TrueZVertex && TrueZVertex<+3")
                << endc;
    }
    {
        string formula = "(Theta-TrueTheta):TrueTheta";
        canvas(GetName()+" ZVertex Energy")
                << drawoption("colz")
                << TTree_drawable(t.Tree, formula+bins_theta,"")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 000<TrueEk && TrueEk<100")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 100<TrueEk && TrueEk<200")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 200<TrueEk && TrueEk<300")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 300<TrueEk && TrueEk<400")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 400<TrueEk && TrueEk<500")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 500<TrueEk && TrueEk<600")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 600<TrueEk && TrueEk<700")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 700<TrueEk && TrueEk<800")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 800<TrueEk && TrueEk<900")
                << endc;
    }
    {
        string formula = "(ThetaCorr-TrueTheta):TrueTheta";
        canvas(GetName()+" ZVertex Energy ThetaCorr")
                << drawoption("colz")
                << TTree_drawable(t.Tree, formula+bins_theta,"")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 000<TrueEk && TrueEk<100")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 100<TrueEk && TrueEk<200")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 200<TrueEk && TrueEk<300")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 300<TrueEk && TrueEk<400")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 400<TrueEk && TrueEk<500")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 500<TrueEk && TrueEk<600")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 600<TrueEk && TrueEk<700")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 700<TrueEk && TrueEk<800")
                << TTree_drawable(t.Tree, formula+bins_theta,"-3<TrueZVertex && TrueZVertex<-2 && 800<TrueEk && TrueEk<900")
                << endc;
    }
}

AUTO_REGISTER_PHYSICS(ExtractShowerDepth)
