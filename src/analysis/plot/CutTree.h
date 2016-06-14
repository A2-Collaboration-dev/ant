#pragma once

#include "HistStyle.h"

#include "analysis/plot/HistogramFactories.h"
#include "root-addons/analysis_codes/hstack.h"
#include "base/Tree.h"


#include <vector>
#include <list>
#include <map>
#include <string>
#include <functional>

namespace ant {
namespace analysis {
namespace plot {

/**
 * @brief Please see progs/EtapOmegaG_plot.cc how to use cuttree
 */
namespace cuttree {

template<typename Fill_t>
struct Cut_t {
    using Passes_t = std::function<bool(const Fill_t&)>;
    Cut_t(const std::string& name,
          Passes_t passes = [] (const Fill_t&) { return true; }) : Name(name), Passes(passes) {}
    const std::string Name;
    const Passes_t Passes;
};

template<typename Fill_t>
using MultiCut_t = std::vector<Cut_t<Fill_t>>;

template<typename Fill_t>
using Cuts_t = std::list<MultiCut_t<Fill_t>>;

template<typename ToFill_t, typename FromFill_t>
Cuts_t<ToFill_t> ConvertCuts(const Cuts_t<FromFill_t>& from_cuts) {
    Cuts_t<ToFill_t> to_cuts;
    for(const auto& from_multicut : from_cuts) {
        to_cuts.emplace_back();
        auto& to_multicut = to_cuts.back();
        for(const auto& from_cut : from_multicut) {
            auto passes = from_cut.Passes;
            to_multicut.emplace_back(from_cut.Name, [passes] (const ToFill_t& f) { return passes(f); });
        }
    }
    return to_cuts;
}

struct TreeInfo_t {
    std::size_t Level;
    std::size_t nDaughters;
    std::size_t nLevelMultiplicity;
    TreeInfo_t(std::size_t level, std::size_t  daughters, std::size_t multiplicity) :
        Level(level), nDaughters(daughters), nLevelMultiplicity(multiplicity)
    {}
};

template<typename Hist_t>
struct Node_t {
    // Hist_t should have that type defined
    using Fill_t = typename Hist_t::Fill_t;

    HistogramFactory HistFac;
    Hist_t Hist;
    typename Cut_t<Fill_t>::Passes_t PassesCut;

    Node_t(const HistogramFactory& parentHistFac,
           Cut_t<Fill_t> cut,
           const TreeInfo_t& treeinfo) :
        HistFac(cut.Name, parentHistFac, cut.Name),
        Hist(HistFac, treeinfo),
        PassesCut(cut.Passes)
    {}

    bool Fill(const Fill_t& f) {
       if(PassesCut(f)) {
           Hist.Fill(f);
           return true;
       }
       return false;
    }
};

template<typename Hist_t>
using CutsIterator_t = typename Cuts_t<typename Hist_t::Fill_t>::const_iterator;

template<typename Hist_t>
using Tree_t = typename Tree<Node_t<Hist_t>>::node_t;

template<typename Hist_t>
void Build(Tree_t<Hist_t> cuttree,
           CutsIterator_t<Hist_t> first,
           CutsIterator_t<Hist_t> last,
           TreeInfo_t& treeInfo)
{
    if(first == last)
        return;

    const auto& multicut = *first;
    const auto& next_it = std::next(first);

    treeInfo.Level++;
    treeInfo.nDaughters = next_it == last ? 0 : next_it->size();
    treeInfo.nLevelMultiplicity = first->size();

    for(const auto& cut : multicut) {
        auto daughter = cuttree->CreateDaughter(cuttree->Get().HistFac, cut, treeInfo);
        Build<Hist_t>(daughter, next_it, last, treeInfo);
    }
}

template<typename Hist_t, typename Fill_t = typename Hist_t::Fill_t>
Tree_t<Hist_t> Make(HistogramFactory histFac, const std::string& name, const Cuts_t<Fill_t>& cuts) {
    if(cuts.empty())
        return nullptr;
    TreeInfo_t treeInfo{0,cuts.front().size(),1};
    auto cuttree = Tree<Node_t<Hist_t>>::MakeNode(histFac, Cut_t<Fill_t>{name}, treeInfo);
    Build<Hist_t>(cuttree, cuts.begin(), cuts.end(), treeInfo);
    return cuttree;
}

template<typename Hist_t, typename Fill_t = typename Hist_t::Fill_t>
void Fill(Tree_t<Hist_t> cuttree, const Fill_t& f) {
    if(cuttree->Get().Fill(f)) {
        for(const auto& d : cuttree->Daughters()) {
            Fill<Hist_t>(d, f);
        }
    }
}

template<typename Hist_t>
struct StackedHists_t {
protected:
    StackedHists_t(const HistogramFactory& histFac, const TreeInfo_t& treeInfo) :
        TreeInfo(treeInfo),
        HistFac(histFac),
        H("h", HistFac)
    {
    }


    const Hist_t& GetHist(unsigned key,
                          const std::string& name = "",
                          histstyle::Mod_t histmod = {}) {
        auto it_hist = Hists.lower_bound(key);
        if(it_hist == Hists.end() || it_hist->first != key) {
            if(name.empty())
                throw std::runtime_error("Cannot create hist with empty name");
            it_hist = Hists.emplace_hint(it_hist, key, WrappedHist_t{
                                             name,
                                             H,
                                             TreeInfo,
                                             histmod
                                         });

            AddToStack(it_hist->second);
        }
        return it_hist->second.Hist;
    }



private:

    TreeInfo_t TreeInfo;
    HistogramFactory HistFac; // directory for stacks and H itself
    HistogramFactory H; // directory for mctrue splitted histfacs

    struct WrappedHist_t {
        Hist_t           Hist;
        histstyle::Mod_t Modify;
        WrappedHist_t(const std::string& name,
                      const HistogramFactory& parentHistFac,
                      const TreeInfo_t& treeInfo,
                      histstyle::Mod_t mod
                      ) :
            Hist(HistogramFactory(name, parentHistFac, name), treeInfo), Modify(mod) {}
    };

    std::map<unsigned, WrappedHist_t> Hists;
    std::vector<ant::hstack*> Stacks;

    void AddToStack(const WrappedHist_t& hist) {
        auto histptrs = hist.Hist.GetHists();
        histptrs.erase(
                    std::remove_if(histptrs.begin(), histptrs.end(),
                                   [] (TH1* h) { return h==nullptr; }
                    ),
                histptrs.end()
                );

        if(Stacks.empty()) {
            for(auto h : histptrs) {
                const std::string name = h->GetName();
                Stacks.emplace_back(
                            HistFac.make<ant::hstack>(
                                // use the parent histFac here!
                                name,
                                H.GetTitlePrefix()+": "+name
                                ));
            }
        }
        assert(histptrs.size() == Stacks.size());

        for(size_t i=0;i<Stacks.size();i++) {
            TH1* h = histptrs[i];
            *Stacks[i] << hist.Modify(h) << h;
        }
    }

};

} // namespace cuttree

}}}
