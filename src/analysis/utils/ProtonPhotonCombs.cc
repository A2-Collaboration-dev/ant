#include "ProtonPhotonCombs.h"

using namespace std;
using namespace ant;
using namespace ant::analysis::utils;

ProtonPhotonCombs::Combinations_t& ProtonPhotonCombs::Combinations_t::FilterMult(unsigned nPhotonsRequired, double maxDiscardedEk)
{
    // prevent some (accidental) misusage
    if(nPhotonsRequired==0)
        throw Exception("Makes no sense to require zero photons");
    auto it = this->begin();
    while(it != this->end()) {
        const auto nPhotons = it->Photons.size();
        if(nPhotons < nPhotonsRequired) {
            if(Observer)
                Observer(std_ext::formatter() << "n(#gamma)=" << nPhotons << "<" << nPhotonsRequired);
            it = this->erase(it);
            continue;
        }
        // calc discarded Ek and do cu
        it->DiscardedEk = 0;
        for(auto i=nPhotonsRequired;i<nPhotons;i++) {
            it->DiscardedEk += it->Photons[i]->Ek();
        }
        if(it->DiscardedEk >= maxDiscardedEk) {
            it = this->erase(it);
            continue;
        }
        if(Observer && isfinite(maxDiscardedEk)) {
            Observer(std_ext::formatter() << "DiscEk<" << maxDiscardedEk);
        }
        it->Photons.resize(nPhotonsRequired); // will always shrink, as nPhotons >= nPhotonsRequired
        ++it;
    }
    return *this;
}

ProtonPhotonCombs::Combinations_t& ProtonPhotonCombs::Combinations_t::FilterIM(const IntervalD& photon_IM_sum_cut) {
    auto it = this->begin();
    while(it != this->end()) {
        it->PhotonSum = LorentzVec{{0,0,0}, 0};
        for(const auto& p : it->Photons)
            it->PhotonSum += *p;
        if(!photon_IM_sum_cut.Contains(it->PhotonSum.M())) {
            it = this->erase(it);
        }
        else {
            if(Observer && photon_IM_sum_cut != nocut)
                Observer(photon_IM_sum_cut.AsCutString("IM(#gamma)"));
            ++it;
        }
    }
    return *this;
}

ProtonPhotonCombs::Combinations_t& ProtonPhotonCombs::Combinations_t::FilterMM(const TTaggerHit& taggerhit, const IntervalD& missingmass_cut, const ParticleTypeDatabase::Type& target) {
    auto it = this->begin();
    const auto beam_target = taggerhit.GetPhotonBeam() + LorentzVec::AtRest(target.Mass());
    while(it != this->end()) {
        // check that PhotonSum is present
        if(it->PhotonSum == LorentzVec{{0,0,0}, 0})
            throw Exception("PhotonSum appears not calculated yet. Call FilterIM beforehand.");
        // remember hit and cut on missing mass
        it->TaggerHit = taggerhit;
        it->MissingMass = (beam_target - it->PhotonSum).M();
        if(!missingmass_cut.Contains(it->MissingMass))
            it = this->erase(it);
        else {
            if(Observer && missingmass_cut != nocut)
                // note that in A2's speech is often "missing mass of proton",
                // but it's actually the "missing mass of photons" expected to be close to the
                // rest mass of the proton
                Observer(missingmass_cut.AsCutString("MM(#gamma)"));
            ++it;
        }
    }
    return *this;
}

ProtonPhotonCombs::Combinations_t ProtonPhotonCombs::MakeCombinations(const TCandidateList& cands) {
    TParticleList all_protons;
    TParticleList all_photons;
    for(auto cand : cands.get_iter()) {
        all_protons.emplace_back(std::make_shared<TParticle>(ParticleTypeDatabase::Proton, cand));
        all_photons.emplace_back(std::make_shared<TParticle>(ParticleTypeDatabase::Photon, cand));
    }

    // important for DiscardedEk cut later
    std::sort(all_photons.begin(), all_photons.end(), [] (const TParticlePtr& a, const TParticlePtr& b) {
        // sort by descending kinetic energy
        return a->Ek() > b->Ek();
    });

    Combinations_t combs;
    for(const auto& proton : all_protons) {
        combs.emplace_back(proton);
        auto& comb = combs.back();
        for(auto photon : all_photons) {
            if(photon->Candidate == proton->Candidate)
                continue;
            comb.Photons.emplace_back(photon);
        }
        assert(comb.Photons.size()+1 == all_photons.size());
    }
    // as every candidate is the proton, it's only
    return combs;
}
