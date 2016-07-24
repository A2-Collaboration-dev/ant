#include "catch.hpp"
#include "catch_config.h"
#include "expconfig_helpers.h"

#include "base/WrapTFile.h"
#include "tree/TEvent.h"
#include "tree/TEventData.h"

#include "analysis/input/pluto/PlutoReader.h"

#include "analysis/utils/Fitter.h"

#include "analysis/utils/MCFakeReconstructed.h"
#include "analysis/utils/MCSmear.h"

#include <iostream>

using namespace std;
using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::input;

void dotest(bool, bool, bool);

TEST_CASE("Fitter: Ideal KinFitter, z vertex fixed, proton measured", "[analysis]") {
    dotest(false, false, false);
}

TEST_CASE("Fitter: Ideal KinFitter, z vertex fixed, proton UNmeasured", "[analysis]") {
    dotest(false, true, false);
}

TEST_CASE("Fitter: Ideal KinFitter, z vertex free, proton measured", "[analysis]") {
    dotest(true, false, false);
}

TEST_CASE("Fitter: Ideal KinFitter, z vertex free, proton UNmeasured", "[analysis]") {
    dotest(true, true, false);
}

TEST_CASE("Fitter: Smeared KinFitter, z vertex fixed, proton measured", "[analysis]") {
    dotest(false, false, true);
}

TEST_CASE("Fitter: Smeared KinFitter, z vertex fixed, proton UNmeasured", "[analysis]") {
    dotest(false, true, true);
}

TEST_CASE("Fitter: Smeared KinFitter, z vertex free, proton measured", "[analysis]") {
    dotest(true, false, true);
}

TEST_CASE("Fitter: Smeared KinFitter, z vertex free, proton UNmeasured", "[analysis]") {
    dotest(true, true, true);
}

struct TestUncertaintyModel : utils::UncertaintyModel {
    const bool ProtonUnmeasured;
    TestUncertaintyModel(bool protonUnmeasured) : ProtonUnmeasured(protonUnmeasured) {}
    virtual utils::Uncertainties_t GetSigmas(const TParticle& particle) const
    {
        utils::Uncertainties_t  u{
                    0.05*particle.Ek(),
                    std_ext::degree_to_radian(2.0),
                    std_ext::degree_to_radian(2.0)
        };
        if(ProtonUnmeasured && particle.Type() == ParticleTypeDatabase::Proton)
            u.sigmaE = 0;
        return u;
    }
};

struct Pulls_t {
    std_ext::RMS Ek;
    std_ext::RMS Theta;
    std_ext::RMS Phi;

    void Fill(const utils::Fitter::FitParticle& p) {
        Ek.Add(p.Ek.Pull);
        Theta.Add(p.Theta.Pull);
        Phi.Add(p.Phi.Pull);
    }
};

struct Constraint_t {
    std_ext::RMS E;
    std_ext::RMS px;
    std_ext::RMS py;
    std_ext::RMS pz;

    void Fill(const LorentzVec& v) {
        E.Add(v.E);
        px.Add(v.p.x);
        py.Add(v.p.y);
        pz.Add(v.p.z);
    }
};

void dotest(bool z_vertex, bool proton_unmeas, bool smeared) {

    auto rootfile = make_shared<WrapTFileInput>(string(TEST_BLOBS_DIRECTORY)+"/Pluto_Etap2g.root");
    PlutoReader reader(rootfile);

    REQUIRE_FALSE(reader.IsSource());

    auto model = make_shared<TestUncertaintyModel>(proton_unmeas);

    utils::KinFitter kinfitter("kinfitter", 2,
                               model, z_vertex);
    if(z_vertex) {
        kinfitter.SetZVertexSigma(0.0);
        test::EnsureSetup(); // needed for MCFake
    }

    // use mc_fake with complete 4pi (no lost photons)
    auto mc_fake = z_vertex ? std_ext::make_unique<utils::MCFakeReconstructed>(true) : nullptr;
    auto mc_smear = smeared ? std_ext::make_unique<utils::MCSmear>(model) : nullptr;

    unsigned nEvents = 0;
    unsigned nFitOk = 0;
    unsigned nFitIterations = 0;

    std_ext::RMS pulls_Beam;
    Pulls_t      pulls_Photons;
    Pulls_t      pulls_Proton;
    Constraint_t constraint_before;
    Constraint_t constraint_after;

    std_ext::RMS fit_prob;
    std_ext::RMS fitted_z_vertex;
    std_ext::RMS IM_2g_before;
    std_ext::RMS IM_2g_after;

    while(true) {
        TEvent event;
        if(!reader.ReadNextEvent(event))
            break;
        nEvents++;

        INFO("nEvents="+to_string(nEvents));

        const TEventData& eventdata = z_vertex ? mc_fake->Get(event.MCTrue()) : event.MCTrue();

        TParticlePtr beam = event.MCTrue().ParticleTree->Get();
        TParticleList protons = eventdata.Particles.Get(ParticleTypeDatabase::Proton);
        TParticleList photons = eventdata.Particles.Get(ParticleTypeDatabase::Photon);

        REQUIRE(beam->Type() == ParticleTypeDatabase::BeamProton);
        REQUIRE(protons.size() == 1);
        REQUIRE(photons.size() == 2);

        TParticlePtr proton = protons.front();

        LorentzVec constraint_unsmeared = *beam - *proton - *photons.front() - *photons.back();

        REQUIRE(constraint_unsmeared.E == Approx(0).epsilon(1e-3));
        REQUIRE(constraint_unsmeared.p.x == Approx(0).epsilon(1e-3));
        REQUIRE(constraint_unsmeared.p.y == Approx(0).epsilon(1e-3));
        REQUIRE(constraint_unsmeared.p.z == Approx(0).epsilon(1e-3));

        if(smeared) {
            beam = mc_smear->Smear(beam);
            proton = mc_smear->Smear(proton);
            for(auto& photon : photons)
                photon = mc_smear->Smear(photon);
        }

        auto photon_sum = *photons.front() + *photons.back();
        IM_2g_before.Add(photon_sum.M());
        LorentzVec constraint_smeared = *beam - *proton - photon_sum;
        constraint_before.Fill(constraint_smeared);

        // do the fit
        kinfitter.SetEgammaBeam(beam->Ek());
        kinfitter.SetProton(proton);
        kinfitter.SetPhotons(photons);
        const APLCON::Result_t& res = kinfitter.DoFit();

        if(res.Status != APLCON::Result_Status_t::Success)
            continue;
        nFitOk++;

        fit_prob.Add(res.Probability);

        if(!smeared) // in ideal conditions, the fitter should converge immediately
            REQUIRE(res.NIterations == 2);

        nFitIterations += res.NIterations;

        if(z_vertex)
            fitted_z_vertex.Add(kinfitter.GetFittedZVertex());
        else
            REQUIRE(std::isnan(kinfitter.GetFittedZVertex()));

        pulls_Beam.Add(kinfitter.GetBeamEPull());

        const auto& fitparticles = kinfitter.GetFitParticles();
        REQUIRE(fitparticles.size() == 3);
        REQUIRE(fitparticles.front().Particle->Type() == ParticleTypeDatabase::Proton);
        auto it_fitparticle = fitparticles.begin();
        auto fitted_proton = it_fitparticle->AsFitted();
        pulls_Proton.Fill(*it_fitparticle);
        ++it_fitparticle;
        LorentzVec fitted_photon_sum{0,0,0,0};
        while (it_fitparticle != fitparticles.end()) {
            pulls_Photons.Fill(*it_fitparticle);
            fitted_photon_sum += *it_fitparticle->AsFitted();
            ++it_fitparticle;
        }

        // check some kinematics

        auto fitted_beam = kinfitter.GetFittedBeamParticle();
        constraint_after.Fill(*fitted_beam - *fitted_proton - fitted_photon_sum);

        REQUIRE(fitted_photon_sum.M() < fitted_beam->M());
        IM_2g_after.Add(fitted_photon_sum.M());

    }

    CHECK(nEvents==1000);

    if(smeared && z_vertex)
        //  smearing and free z vertex does not always converge
        CHECK(nFitOk==Approx(0.98*nEvents).epsilon(0.01));
    else
        CHECK(nFitOk==nEvents);

    CHECK(constraint_after.E.GetMean() == Approx(0));
    CHECK(constraint_after.E.GetRMS() == Approx(0));
    CHECK(constraint_after.px.GetMean() == Approx(0));
    CHECK(constraint_after.px.GetRMS() == Approx(0));
    CHECK(constraint_after.py.GetMean() == Approx(0));
    CHECK(constraint_after.py.GetRMS() == Approx(0));
    CHECK(constraint_after.pz.GetMean() == Approx(0));
    CHECK(constraint_after.pz.GetRMS() == Approx(0));

    if(smeared) {
        CHECK(fit_prob.GetMean() == Approx(0.5).epsilon(1e-2));
        CHECK(fit_prob.GetRMS() ==  Approx(1/sqrt(12.0)).epsilon(1e-2));

        CHECK(nFitIterations > nEvents*2); // fitter should take longer to converge

        CHECK(constraint_before.E.GetMean() == Approx(0.0).scale(constraint_before.E.GetRMS()).epsilon(0.1));
        CHECK(constraint_before.E.GetRMS() == Approx(40.0).epsilon(0.2));
        CHECK(constraint_before.px.GetMean() == Approx(0.0).scale(constraint_before.px.GetRMS()).epsilon(0.1));
        CHECK(constraint_before.px.GetRMS() == Approx(40.0).epsilon(0.2));
        CHECK(constraint_before.py.GetMean() == Approx(0.0).scale(constraint_before.py.GetRMS()).epsilon(0.1));
        CHECK(constraint_before.py.GetRMS() == Approx(40.0).epsilon(0.2));
        CHECK(constraint_before.pz.GetMean() == Approx(0.0).scale(constraint_before.pz.GetRMS()).epsilon(0.1));
        CHECK(constraint_before.pz.GetRMS() == Approx(40.0).epsilon(0.2));

        CHECK(pulls_Beam.GetMean() == Approx(0).epsilon(0.12));
        CHECK(pulls_Beam.GetRMS() == Approx(1).epsilon(0.03));

        if(proton_unmeas) {
            CHECK(pulls_Proton.Ek.GetMean() == Approx(0));
            CHECK(pulls_Proton.Ek.GetRMS() == Approx(0));
        }
        else {
            CHECK(pulls_Proton.Ek.GetMean() == Approx(0).scale(pulls_Proton.Ek.GetRMS()).epsilon(0.1));
            CHECK(pulls_Proton.Ek.GetRMS() == Approx(1).epsilon(0.04));
        }

        CHECK(pulls_Proton.Theta.GetMean() == Approx(0).scale(pulls_Proton.Theta.GetRMS()).epsilon(0.1));
        CHECK(pulls_Proton.Theta.GetRMS() == Approx(1).epsilon(0.006));
        CHECK(pulls_Proton.Phi.GetMean() == Approx(0).scale(pulls_Proton.Phi.GetRMS()).epsilon(0.1));
        CHECK(pulls_Proton.Phi.GetRMS() == Approx(1).epsilon(0.01));

        CHECK(pulls_Photons.Ek.GetMean() == Approx(0).scale(pulls_Photons.Ek.GetRMS()).epsilon(0.1));
        CHECK(pulls_Photons.Ek.GetRMS() == Approx(1).epsilon(0.03));
        CHECK(pulls_Photons.Theta.GetMean() == Approx(0).scale(pulls_Photons.Theta.GetRMS()).epsilon(0.1));
        CHECK(pulls_Photons.Theta.GetRMS() == Approx(1).epsilon(0.02));
        CHECK(pulls_Photons.Phi.GetMean() == Approx(0).scale(pulls_Photons.Phi.GetRMS()).epsilon(0.1));
        CHECK(pulls_Photons.Phi.GetRMS() == Approx(1).epsilon(0.03));

        CHECK(IM_2g_before.GetMean() == Approx(ParticleTypeDatabase::EtaPrime.Mass()).epsilon(0.003));
        CHECK(IM_2g_after.GetMean() == Approx(ParticleTypeDatabase::EtaPrime.Mass()).epsilon(0.003));


        if(z_vertex) {
            CHECK(fitted_z_vertex.GetMean() == Approx(0.0).scale(fitted_z_vertex.GetRMS()).epsilon(0.08));
            if(proton_unmeas)
                CHECK(fitted_z_vertex.GetRMS() ==  Approx(1.8).epsilon(0.03));
            else
                CHECK(fitted_z_vertex.GetRMS() ==  Approx(1.3).epsilon(0.03));

            // fitting should improve resolution considerably, but free z vertex worsens it
            CHECK(IM_2g_after.GetRMS() < 0.34*IM_2g_before.GetRMS());
        }
        else {
            // fitting should improve resolution considerably
            CHECK(IM_2g_after.GetRMS() < 0.22*IM_2g_before.GetRMS());
        }
    }
    else {
        // probability peaks at 1
        CHECK(fit_prob.GetMean() == Approx(1.0));
        CHECK(fit_prob.GetRMS() ==  Approx(0.0));

        // unsmeared, so all pulls and constraint_before and fitted_z_vertex should be delta peaks aka mean=RMS=0

        if(z_vertex) {
            CHECK(fitted_z_vertex.GetMean() == Approx(0.0));
            CHECK(fitted_z_vertex.GetRMS() ==  Approx(0.0).epsilon(1e-3));
        }

        constexpr double eps_constraint = 4e-4;
        CHECK(constraint_before.E.GetMean() == Approx(0).epsilon(eps_constraint));
        CHECK(constraint_before.E.GetRMS() == Approx(0).epsilon(eps_constraint));
        CHECK(constraint_before.px.GetMean() == Approx(0).epsilon(eps_constraint));
        CHECK(constraint_before.px.GetRMS() == Approx(0).epsilon(eps_constraint));
        CHECK(constraint_before.py.GetMean() == Approx(0).epsilon(eps_constraint));
        CHECK(constraint_before.py.GetRMS() == Approx(0).epsilon(eps_constraint));
        CHECK(constraint_before.pz.GetMean() == Approx(0).epsilon(eps_constraint));
        CHECK(constraint_before.pz.GetRMS() == Approx(0).epsilon(eps_constraint));

        constexpr double eps_pulls = 1e-3;
        CHECK(pulls_Beam.GetMean() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Beam.GetRMS() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Proton.Ek.GetMean() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Proton.Ek.GetRMS() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Proton.Theta.GetMean() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Proton.Theta.GetRMS() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Proton.Phi.GetMean() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Proton.Phi.GetRMS() == Approx(0).epsilon(eps_pulls));

        CHECK(pulls_Photons.Ek.GetMean() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Photons.Ek.GetRMS() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Photons.Theta.GetMean() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Photons.Theta.GetRMS() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Photons.Phi.GetMean() == Approx(0).epsilon(eps_pulls));
        CHECK(pulls_Photons.Phi.GetRMS() == Approx(0).epsilon(eps_pulls));

        CHECK(IM_2g_before.GetMean() == Approx(ParticleTypeDatabase::EtaPrime.Mass()).epsilon(0.003));
        CHECK(IM_2g_before.GetRMS() == Approx(0).epsilon(0.01).scale(100));
        CHECK(IM_2g_after.GetMean() == Approx(ParticleTypeDatabase::EtaPrime.Mass()).epsilon(0.003));
        CHECK(IM_2g_after.GetRMS() == Approx(0).epsilon(0.01).scale(100));
    }
}
