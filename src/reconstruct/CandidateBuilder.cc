#include "CandidateBuilder.h"
#include "base/Logger.h"
#include "tree/TCluster.h"
#include "expconfig/detectors/CB.h"
#include "expconfig/detectors/PID.h"
#include "expconfig/detectors/TAPS.h"
#include "expconfig/detectors/TAPSVeto.h"
#include "base/std_ext/math.h"



using namespace ant;
using namespace std;
using namespace ant::reconstruct;
using namespace ant::expconfig;

CandidateBuilder::CandidateBuilder(
        const CandidateBuilder::sorted_detectors_t& sorted_detectors,
        const std::shared_ptr<ExpConfig::Reconstruct>& _config
        ) :
    config(_config->GetCandidateBuilderConfig())
{

    try {
        cb  = dynamic_pointer_cast<detector::CB>(sorted_detectors.at(Detector_t::Type_t::CB));
    } catch (...) {
        VLOG(3) << "Detector CB not initialized";
    }

    try {
        pid = dynamic_pointer_cast<detector::PID>(sorted_detectors.at(Detector_t::Type_t::PID));
    } catch (...) {
        VLOG(3) << "Detector PID not initialized";
    }

    try {
        taps = dynamic_pointer_cast<detector::TAPS>(sorted_detectors.at(Detector_t::Type_t::TAPS));
    } catch (...) {
        VLOG(3) << "Detector TAPS not initialized";
    }

    try {
        tapsveto = dynamic_pointer_cast<detector::TAPSVeto>(sorted_detectors.at(Detector_t::Type_t::TAPSVeto));
    } catch (...) {
        VLOG(3) << "Detector TAPSVeto not initialized";
    }

}

void CandidateBuilder::Build_PID_CB(sorted_clusters_t& sorted_clusters,
                                    candidates_t& candidates, clusters_t& all_clusters)
{
    auto it_cb_clusters = sorted_clusters.find(Detector_t::Type_t::CB);
    if(it_cb_clusters == sorted_clusters.end())
        return;
    auto& cb_clusters  = it_cb_clusters->second;
    if(cb_clusters.empty())
        return;

    auto it_pid_clusters = sorted_clusters.find(Detector_t::Type_t::PID);
    if(it_pid_clusters == sorted_clusters.end())
        return;
    auto& pid_clusters  = it_pid_clusters->second;
    if(pid_clusters.empty())
        return;

    auto it_pid_cluster = pid_clusters.begin();

    while(it_pid_cluster != pid_clusters.end()) {

        auto& pid_cluster = *it_pid_cluster;
        const auto pid_phi = pid_cluster.Position.Phi();
        const auto dphi_max = (pid->dPhi(pid_cluster.CentralElement) + config.PID_Phi_Epsilon) / 2.0;

        bool matched = false;

        auto it_cb_cluster = cb_clusters.begin();

        while(it_cb_cluster != cb_clusters.end()) {
            auto& cb_cluster = *it_cb_cluster;
            const auto cb_phi = cb_cluster.Position.Phi();

            // calculate phi angle difference.
            // Phi_mpi_pi() takes care of wrap-arounds at 180/-180 deg
            const auto dphi = fabs(vec2::Phi_mpi_pi(cb_phi - pid_phi));
            if(dphi < dphi_max ) { // match!

                candidates.emplace_back(
                            Detector_t::Type_t::CB | Detector_t::Type_t::PID,
                            cb_cluster.Energy,
                            cb_cluster.Position.Theta(),
                            cb_cluster.Position.Phi(),
                            cb_cluster.Time,
                            cb_cluster.Hits.size(),
                            pid_cluster.Energy,
                            numeric_limits<double>::quiet_NaN(), // no tracker information
                            TClusterList{it_cb_cluster, it_pid_cluster}
                            );
                all_clusters.push_back(it_cb_cluster);
                it_cb_cluster = cb_clusters.erase(it_cb_cluster);
                matched = true;
            }
            else {
                ++it_cb_cluster;
            }
        }

        if(matched) {
            all_clusters.push_back(it_pid_cluster);
            it_pid_cluster = pid_clusters.erase(it_pid_cluster);
        } else {
            ++it_pid_cluster;
        }
    }
}

void CandidateBuilder::Build_TAPS_Veto(sorted_clusters_t& sorted_clusters,
                                       candidates_t& candidates, clusters_t& all_clusters)
{
    auto it_taps_clusters = sorted_clusters.find(Detector_t::Type_t::TAPS);
    if(it_taps_clusters == sorted_clusters.end())
        return;
    auto& taps_clusters  = it_taps_clusters->second;
    if(taps_clusters.empty())
        return;

    auto it_veto_clusters = sorted_clusters.find(Detector_t::Type_t::TAPSVeto);
    if(it_veto_clusters == sorted_clusters.end())
        return;
    auto& veto_clusters  = it_veto_clusters->second;
    if(veto_clusters.empty())
        return;

    const auto element_radius2 = std_ext::sqr(tapsveto->GetElementRadius());

    auto it_veto_cluster = veto_clusters.begin();
    while(it_veto_cluster != veto_clusters.end()) {

        auto& veto_cluster = *it_veto_cluster;

        bool matched = false;

        const auto& vpos = veto_cluster.Position;

        auto it_taps_cluster = taps_clusters.begin();

        while(it_taps_cluster != taps_clusters.end()) {

            auto& taps_cluster = *it_taps_cluster;

            const auto& tpos = taps_cluster.Position;
            const auto& d = tpos - vpos;

            if( d.XY().R() < element_radius2 ) {
                candidates.emplace_back(
                            Detector_t::Type_t::TAPS | Detector_t::Type_t::TAPSVeto,
                            taps_cluster.Energy,
                            taps_cluster.Position.Theta(),
                            taps_cluster.Position.Phi(),
                            taps_cluster.Time,
                            taps_cluster.Hits.size(),
                            veto_cluster.Energy,
                            numeric_limits<double>::quiet_NaN(), // no tracker information
                            TClusterList{it_taps_cluster, it_veto_cluster}
                            );
                all_clusters.push_back(it_taps_cluster);
                it_taps_cluster = taps_clusters.erase(it_taps_cluster);
                matched = true;
            } else {
                ++it_taps_cluster;
            }
        }

        if(matched) {
            all_clusters.push_back(it_veto_cluster);
            it_veto_cluster = veto_clusters.erase(it_veto_cluster);
        } else {
            ++it_veto_cluster;
        }
    }
}

void CandidateBuilder::Catchall(sorted_clusters_t& sorted_clusters,
                                candidates_t& candidates, clusters_t& all_clusters)
{
    for(auto& cluster_list : sorted_clusters ) {

        const auto& detector_type = cluster_list.first;
        auto& clusters      = cluster_list.second;

        if(option_allowSingleVetoClusters &&
           (detector_type == Detector_t::Type_t::PID || detector_type == Detector_t::Type_t::TAPSVeto)) {
            for(auto it_c = clusters.begin(); it_c != clusters.end(); ++it_c) {
                auto& c = *it_c;
                candidates.emplace_back(
                            detector_type,
                            0, // no energy in calo
                            c.Position.Theta(),
                            c.Position.Phi(),
                            c.Time,
                            1, // cluster size
                            c.Energy,
                            numeric_limits<double>::quiet_NaN(), // no tracker information
                            TClusterList{it_c}
                            );
                all_clusters.push_back(it_c);
            }
            clusters.clear();
        } else if(detector_type == Detector_t::Type_t::CB || detector_type == Detector_t::Type_t::TAPS) {
            for(auto it_c = clusters.begin(); it_c != clusters.end(); ++it_c) {
                auto& c = *it_c;
                candidates.emplace_back(
                            detector_type,
                            c.Energy,
                            c.Position.Theta(),
                            c.Position.Phi(),
                            c.Time,
                            c.Hits.size(),
                            0, // no energy in Veto
                            numeric_limits<double>::quiet_NaN(), // no tracker information
                            TClusterList{it_c}
                            );
                all_clusters.push_back(it_c);
            }
            clusters.clear();
        } else if(detector_type == Detector_t::Type_t::MWPC0 || detector_type == Detector_t::Type_t::MWPC1
                  || detector_type == Detector_t::Type_t::Cherenkov) {
            /// @todo Think about Cherenkov's role more closely, tracker energy looks wrong...
            /// @todo Implement MWPC matching...
            for(auto it_c = clusters.begin(); it_c != clusters.end(); ++it_c) {
                auto& c = *it_c;
                candidates.emplace_back(
                            detector_type,
                            0,        // no energy in calo
                            c.Position.Theta(),
                            c.Position.Phi(),
                            c.Time,
                            1,        // cluster size
                            0,        // no energy in veto
                            c.Energy, // tracker energy (not meaningful for cherenkov clusters)
                            TClusterList{it_c}
                            );
                all_clusters.push_back(it_c);
            }
            clusters.clear();
        }
    }
}





void CandidateBuilder::BuildCandidates(sorted_clusters_t& sorted_clusters,
        candidates_t& candidates, clusters_t& all_clusters
        )
{
    // build candidates
    if(cb && pid)
        Build_PID_CB(sorted_clusters, candidates, all_clusters);

    if(taps && tapsveto)
        Build_TAPS_Veto(sorted_clusters, candidates, all_clusters);

    Catchall(sorted_clusters, candidates, all_clusters);
}

void CandidateBuilder::Build(
        sorted_clusters_t sorted_clusters,
        candidates_t& candidates,
        clusters_t& all_clusters
        )
{
    // search for clusters which are not sane or don't pass thresholds
    // add them to all_clusters with unmatched flag set
    for(auto& det_entry : sorted_clusters) {
        const auto detectortype = det_entry.first;
        auto& clusters = det_entry.second;

        auto it_cluster = clusters.begin();
        while(it_cluster != clusters.end()) {
            double threshold = 0.0;
            if(detectortype == Detector_t::Type_t::CB)
                threshold = config.CB_ClusterThreshold;
            if(detectortype == Detector_t::Type_t::TAPS)
                threshold = config.TAPS_ClusterThreshold;

            TCluster& cluster = *it_cluster;

            // do not remove clusters which are sane and pass the thresholds
            if(cluster.isSane() && cluster.Energy > threshold) {
                ++it_cluster;
                continue;
            }

            cluster.SetFlag(TCluster::Flags_t::Unmatched);
            all_clusters.push_back(it_cluster);
            it_cluster = clusters.erase(it_cluster);
        }
    }

    BuildCandidates(sorted_clusters, candidates, all_clusters);

    // add remaining unmatched clusters to all_clusters with Unmatched flag set
    for(auto& det_entry : sorted_clusters) {
        auto& clusters = det_entry.second;
        for(auto it_cluster = clusters.begin(); it_cluster != clusters.end(); ++it_cluster)
        {
            it_cluster->SetFlag(TCluster::Flags_t::Unmatched);
            all_clusters.push_back(it_cluster);
        }
    }
}

