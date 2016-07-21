#include "DebugPhysics.h"

#include "slowcontrol/SlowControlVariables.h"

#include "base/Logger.h"
#include "expconfig/ExpConfig.h"

using namespace std;
using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::physics;

DebugPhysics::DebugPhysics(const std::string& name, OptionsPtr opts) :
    Physics(name, opts),
    noDump(opts->Get<bool>("NoDump", false)),
    writeEvents(opts->Get<unsigned>("WriteEvents", 0)),
    keepReadHits(opts->Get<bool>("KeepReadHits", false)),
    requestSlowControl(opts->Get<bool>("RequestSlowControl", false))
{
    if(requestSlowControl)
        slowcontrol::Variables::TaggerScalers->Request();
}

DebugPhysics::~DebugPhysics() {}

void DebugPhysics::ProcessEvent(const TEvent& event, manager_t& manager)
{
    if(writeEvents>0 && seenEvents % writeEvents == 0) {
        manager.SaveEvent();
        if(keepReadHits)
            manager.KeepDetectorReadHits();
        if(requestSlowControl)
            LOG_N_TIMES(1, INFO) <<  "First Tagger Scalers: " << slowcontrol::Variables::TaggerScalers->Get();
    }
    else if(!writeEvents && !noDump) {
        LOG(INFO) << event;
        // only access slowcontrol if it was actually requested in the beginning
        if(requestSlowControl)
            LOG(INFO) << "Tagger Scalers: " << slowcontrol::Variables::TaggerScalers->Get();
    }
    seenEvents++;
    lastTID = event.Reconstructed().ID;
}

void DebugPhysics::Finish()
{
    LOG(INFO) << "Seen " << seenEvents << "=0x" << hex << seenEvents << dec
              << " Events, last TID " << lastTID;
}

void DebugPhysics::ShowResult()
{
    LOG(INFO) << "ShowResult called";
}





DebugPIDAlignment::DebugPIDAlignment(const std::string& name, OptionsPtr opts):
    Physics(name, opts)
{
    const BinSettings bins(360,-180,180);
    angles_mc = HistFac.makeTH2D("PID Phi angles MC","MCTrue #phi","Rec #phi",bins,bins);
    angles_candidates = HistFac.makeTH2D("PID Phi angles Candidates","PID #phi","CB #phi",bins,bins);
    angles_clusters   = HistFac.makeTH2D("PID Phi angles Clusters","PID #phi","CB #phi",bins,bins);
    angles_diff = HistFac.makeTH1D("PID-CB Angle Difference","#Delta#phi","",BinSettings(720,-360,360));
    angles_diff_wrap = HistFac.makeTH1D("PID-CB Angle Difference wrapped","#Delta#phi","",BinSettings(720,-360,360));
}

DebugPIDAlignment::~DebugPIDAlignment()
{

}

void DebugPIDAlignment::ProcessEvent(const TEvent& event, manager_t&)
{

    constexpr auto radtodeg = std_ext::radian_to_degree(1.0);

    if(event.MCTrue().Particles.GetAll().size() == 1) {
        const auto mctrue_phi = event.MCTrue().Particles.GetAll().front()->Phi()*radtodeg;

        for(const TCandidate& cand : event.Reconstructed().Candidates) {
            for(const TCluster& c : cand.Clusters) {
                if(c.DetectorType == Detector_t::Type_t::PID) {
                    angles_mc->Fill(mctrue_phi, c.Position.Phi()*radtodeg);
                }
            }
        }
    }

    for(const TCandidate& cand : event.Reconstructed().Candidates) {
        if(cand.Detector & Detector_t::Any_t::CB_Apparatus) {
            auto cl_cb = cand.FindCaloCluster();
            auto cl_pid = cand.FindVetoCluster();
            if(cl_cb && cl_pid) {
                angles_candidates->Fill(cl_pid->Position.Phi()*radtodeg,
                                        cl_cb->Position.Phi()*radtodeg);
            }
        }
    }

    const auto& clusters =  event.Reconstructed().Clusters;

    auto cb_clusters = clusters.get_ptr_list([] (const TCluster& cluster) {
        return cluster.DetectorType == Detector_t::Type_t::CB;
    });
    auto pid_clusters = clusters.get_ptr_list([] (const TCluster& cluster) {
        return cluster.DetectorType == Detector_t::Type_t::PID;
    });

    if(cb_clusters.size() == 1 && pid_clusters.size() == 1) {

        const auto& cl_cb = *cb_clusters.front();
        const auto& cl_pid = *pid_clusters.front();

        const double phi_cb = cl_cb.Position.Phi();
        const double phi_pid = cl_pid.Position.Phi();
        angles_clusters->Fill(phi_pid*radtodeg,
                              phi_cb*radtodeg);
        angles_diff->Fill((phi_cb-phi_pid)*radtodeg);
        angles_diff_wrap->Fill(vec2::Phi_mpi_pi(phi_cb-phi_pid)*radtodeg);
    }
}

void DebugPIDAlignment::ShowResult()
{
    canvas("PID angles")
            << drawoption("colz")
            << angles_mc
            << angles_candidates
            << angles_clusters
            << angles_diff
            << angles_diff_wrap
            << endc;

}

AUTO_REGISTER_PHYSICS(DebugPhysics)
AUTO_REGISTER_PHYSICS(DebugPIDAlignment)
