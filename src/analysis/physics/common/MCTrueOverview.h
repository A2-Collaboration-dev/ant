#pragma once

#include "analysis/physics/Physics.h"
#include "base/ParticleTypeTree.h"
#include "tree/TParticle.h"

#include <map>

namespace ant {
namespace analysis {
namespace physics {


class MCTrueOverview : public Physics {
protected:

    struct perChannel_t {
        perChannel_t(const HistogramFactory& histFac,
                     const ParticleTypeTree& typetree);

        void Fill(const TParticleTree_t& ptree) const;
        void Show(canvas& c) const;

    protected:
        struct histnode_t {
            using typeptr_t = const ParticleTypeDatabase::Type*;
            histnode_t(std::unique_ptr<const HistogramFactory> histFacPtr,
                       const std::vector<typeptr_t>& leafTypes);
            void Fill(const TParticle& p);
            void Show(canvas& c) const;

        protected:
            struct perType_t {
                perType_t(const HistogramFactory& HistFac);
                TH2D* h_EkTheta = nullptr;
            };
            std::map<typeptr_t, perType_t> hists;
        };

        using histtree_t = Tree<histnode_t>::node_t;
        histtree_t histtree;
        void traverse_tree_and_fill(const histtree_t& histtree, const TParticleTree_t& ptree) const;

        TH1D* h_CBEsum = nullptr;
    };

    std::map<ParticleTypeTreeDatabase::Channel, perChannel_t> channels;

public:
    MCTrueOverview(const std::string& name, OptionsPtr opts);

    virtual void ProcessEvent(const TEvent& event, manager_t& manager) override;
    virtual void ShowResult() override;
};

}
}
}
