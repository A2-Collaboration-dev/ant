#include "Clustering.h"
#include "detail/Clustering_NextGen.h"

#include "base/Detector_t.h"

#include "tree/TCluster.h"
#include "tree/TDetectorReadHit.h"

#include "base/Logger.h"

using namespace std;
using namespace ant;
using namespace ant::reconstruct;

Clustering::Clustering()
{
}

void Clustering::Build(const ClusterDetector_t& clusterdetector,
        const TClusterHitList& clusterhits,
        TClusterList& clusters)
{
    // clustering detector, so we need additional information
    // to build the crystals_t
    list<clustering::crystal_t> crystals;
    for(const TClusterHit& hit : clusterhits) {
        // ignore hits without energy or time information
        if(!hit.IsSane()) {
            continue;
        }
        crystals.emplace_back(
                    hit.Energy,
                    clusterdetector.GetClusterElement(hit.Channel),
                    addressof(hit)
                    );
    }

    // do the clustering (calls detail/Clustering_NextGen.h code)
    vector< clustering::cluster_t > crystal_clusters;
    clustering::do_clustering(crystals, crystal_clusters);

    // now calculate some cluster properties,
    // and create TCluster out of it

    for(const clustering::cluster_t& cluster : crystal_clusters) {
        const double cluster_energy = clustering::calc_total_energy(cluster);

        clusters.emplace_back(
                    vec3(0,0,0),
                    cluster_energy,
                    std_ext::NaN,
                    clusterdetector.Type,
                    0
                    );
        auto& the_cluster = clusters.back();

        auto& clusterhits = the_cluster.Hits;
        clusterhits.reserve(cluster.size());

        double weightedSum = 0;
        double cluster_maxenergy   = 0;
        bool crystalTouchesHole = false;
        for(const clustering::crystal_t& crystal : cluster) {

            clusterhits.emplace_back(*crystal.Hit);

            double wgtE = clustering::calc_energy_weight(crystal.Energy, cluster_energy);
            the_cluster.Position += crystal.Element->Position * wgtE;
            weightedSum += wgtE;

            crystalTouchesHole |= crystal.Element->TouchesHole;

            // search for crystal with maximum energy
            // which is defined as the central element
            if(crystal.Energy >= cluster_maxenergy) {
                cluster_maxenergy = crystal.Energy;

                the_cluster.SetFlag(TCluster::Flags_t::TouchesHoleCentral, crystal.Element->TouchesHole);
                the_cluster.Time = crystal.Hit->Time;
                the_cluster.CentralElement = crystal.Element->Channel;
                // search for short energy
                for(const TClusterHit::Datum& datum : crystal.Hit->Data) {
                    if(datum.Type == Channel_t::Type_t::IntegralShort) {
                        the_cluster.ShortEnergy = datum.Value.Calibrated;
                        break;
                    }
                }
            }
        }
        the_cluster.Position *= 1.0/weightedSum;

        if(cluster.Split)
            the_cluster.SetFlag(TCluster::Flags_t::Split);

        if(crystalTouchesHole)
            the_cluster.SetFlag(TCluster::Flags_t::TouchesHoleCrystal);
    }
}

