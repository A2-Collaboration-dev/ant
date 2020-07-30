// --------------------------- Important ---------------------------
// In Compton folder is a README file. Please read it if you
// want to use this code.


#include "He4Pi0.h"

using namespace std;
using namespace ant;
using namespace ant::analysis;
using namespace ant::analysis::physics;

// Defining the contructor for the Compton class
He4Pi0::He4Pi0(const string& name, OptionsPtr opts) :
    Physics(name, opts),
    tagger(ExpConfig::Setup::GetDetector<TaggerDetector_t>()),
    nchannels(tagger->GetNChannels())
{
    // Bins used in histograms
    const BinSettings time_bins(2000, -200, 200);
    const BinSettings mass_bins(250,3500,4000);
    const BinSettings mass_bins_pi0(250, 0, 500);
    const BinSettings missing_energy_bins(250,-250,250);
    const BinSettings angle_bins(18, 0, 360);
    const BinSettings taggerchannel_bins(nchannels);

// ------------ Histograms Created Here but not Filled ------------

    h_WeightedTaggerTime = HistFac.makeTH1D("Weighted Tagger Time",
                                    "t [ns]","#",
                                    time_bins,
                                    "h_WeightedTaggerTime"
                                    );
//    h_MM = HistFac.makeTH1D("All particles, No Weights",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM"
//                                     );
//    h_MM1 = HistFac.makeTH1D("All Particles",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM1"
//                                     );
//    h_MM11 = HistFac.makeTH1D("Uncharged",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM11"
//                                     );
//    h_MM101 = HistFac.makeTH1D("1 Particle",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM101"
//                                     );
    h_MM111 = HistFac.makeTH1D("1 Particle, Uncharged",
                                     "mass [MeV/c^2]","#",
                                     mass_bins,
                                     "h_MM111"
                                     );
    h3D_MM111 = HistFac.makeTH3D("1 Particle, Uncharged",
                                 "Missing Mass [MeV]",
                                 "Angle [deg]",
                                 "Tagger Channel",
                                 mass_bins,
                                 angle_bins,
                                 taggerchannel_bins ,
                                 "h3D_MM111"
                                 );
    h3D_MM111_projX = HistFac.makeTH1D("1 Particle, Uncharged",
                                     "mass [MeV/c^2]","#",
                                     mass_bins,
                                     "h3D_MM111_projX"
                                     );
//    h_MM102 = HistFac.makeTH1D("2 Particles",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM102"
//                                     );
//    h_MM112 = HistFac.makeTH1D("2 Particles, "
//                                     "One is Uncharged",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM112"
//                                     );
//    h_MM1021 = HistFac.makeTH1D("2 Particles, "
//                                     "Closer Missing Mass",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM1021"
//                                     );
//    h_MM10201 = HistFac.makeTH1D("2 Particles, Coplanar",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM10201"
//                                     );
//    h_MM11201 = HistFac.makeTH1D("2 Particles, Coplanar, "
//                                     "One is Uncharged",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM11201"
//                                     );
//    h_MM10211 = HistFac.makeTH1D("2 Particles, Coplanar, "
//                                     "Closer Missing Mass",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM10211"
//                                     );
//    h_MM102001 = HistFac.makeTH1D("2 Particles, Open Ang < 15",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM102001"
//                                     );
//    h_MM112001 = HistFac.makeTH1D("2 Particles, Open Ang < 15, "
//                                     "Uncharged",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM112001"
//                                     );
//    h_MM112001_switch = HistFac.makeTH1D("2 Particles, One is Uncharged, "
//                                     "Open Ang < 15",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM112001_switch"
//                                     );
//    h_MM102011 = HistFac.makeTH1D("2 Particles, Open Ang < 15, "
//                                     "Coplanar",
//                                     "mass [MeV/c^2]","#",
//                                     mass_bins,
//                                     "h_MM102011"
//                                     );
    h_MM112011 = HistFac.makeTH1D("2 Particles, Open Ang < 15, "
                                  "Coplanar, Uncharged",
                                  "mass [MeV/c^2]","#",
                                  mass_bins,
                                  "h_MM112011"
                                     );
    h3D_MM112011 = HistFac.makeTH3D("2 Particles, Open Ang < 15, "
                                    "Coplanar, Uncharged",
                                    "Missing Mass [MeV]",
                                    "Angle [deg]",
                                    "Tagger Channel",
                                    mass_bins,
                                    angle_bins,
                                    taggerchannel_bins ,
                                    "h3D_MM112011"
                                    );
    h3D_MM112011_projX = HistFac.makeTH1D("2 Particles, Open Ang < 15, "
                                  "Coplanar, Uncharged",
                                  "mass [MeV/c^2]","#",
                                  mass_bins,
                                  "h3D_MM112011_projX"
                                     );
    h_MM112011_switch = HistFac.makeTH1D("2 Particles, One is Uncharged, "
                                         "Open Ang < 15, Coplanar",
                                         "mass [MeV/c^2]","#",
                                         mass_bins,
                                         "h_MM112011_switch"
                                         );
    h3D_MM112011_switch = HistFac.makeTH3D("2 Particles, One is Uncharged, "
                                           "Open Ang < 15, Coplanar",
                                           "Missing Mass [MeV]",
                                           "Angle [deg]",
                                           "Tagger Channel",
                                           mass_bins,
                                           angle_bins,
                                           taggerchannel_bins ,
                                           "h3D_MM112011_switch"
                                           );
    h3D_MM112011_switch_projX = HistFac.makeTH1D("2 Particles, One is Uncharged, "
                                         "Open Ang < 15, Coplanar",
                                         "mass [MeV/c^2]","#",
                                         mass_bins,
                                         "h3D_MM112011_switch_projX"
                                         );

    h_ScalarCounts = HistFac.makeTH1D("Total Counts in Tagger",
                                      "Tagger Channel","#",
                                      taggerchannel_bins,
                                      "h_ScalarCounts");

    h_MMpi0 = HistFac.makeTH1D("Pi0 Missing Mass",
                                      "Missing Mass","#",
                                      mass_bins_pi0,
                                      "h_MMpi0");

    h_MMhe4 = HistFac.makeTH1D("He4 Missing Energy",
                                          "Missing Energy","#",
                                          missing_energy_bins,
                                          "h_MMhe4");

    h_MMpi0_2 = HistFac.makeTH1D("Pi0 Missing Energy in CM Frame",
                                      "Missing Energy","#",
                                      missing_energy_bins,
                                      "h_MMpi0_2");

//  ---------------- Get Variables at Command Line ----------------

    // Getting the prompt random windows
    if (opts->HasOption("PR"))
        PR_windows = opts->Get<std::string>("PR","-200,7,9,19,21,200");

    // Turning string into a vector with string entries
    const auto& PR_string_vec = std_ext::tokenize_string(PR_windows,",");

    // Specifying prompt and random ranges (see README for more info)
    promptrandom.AddRandomRange
            ({ stod(PR_string_vec.at(0)), stod(PR_string_vec.at(1)) });
    promptrandom.AddPromptRange
            ({ stod(PR_string_vec.at(2)), stod(PR_string_vec.at(3)) });
    promptrandom.AddRandomRange
            ({ stod(PR_string_vec.at(4)), stod(PR_string_vec.at(5)) });

    // Get variables at command line. The range of taggerhit energies
    // that one would like to use can be specified
    if (opts->HasOption("low"))
        tagger_energy_low = opts->Get<double>("low", 0);
    if (opts->HasOption("high"))
        tagger_energy_high = opts->Get<double>("high", 2000);


    // Getting target specification at the command line
    // Current set up to accept "he4" or "p"
    // Eventually would be best to read in from setup file, but helium-4 is currently not an option there
    if (opts->HasOption("target"))
        target_type = opts->Get<std::string>("target","he4");
    if (target_type == "he4")
        target_mass = He4_mass;
    else if(target_type == "p")
        target_mass = proton_mass;
    else
    {
        LOG(ERROR) << "Unrecognized target type. Using helium-4 default. Use \"p\" for proton or \"he4\" for helium-4.";
        target_mass = He4_mass;
    }
// ------------------ Getting Tagger Scalars ------------------

    slowcontrol::Variables::TaggerScalers->Request();
}

// ------------ Funtions specific to the Compton class ------------

// Checks if veto_energy (energy depositied in PID and Veto wall) meets
// threshold for particle being considered charged. Returns true if
// particle is charged and false if it is not
bool He4Pi0::IsParticleCharged(double veto_energy)
{
    if (veto_energy < .2) { return false; }
    else { return true; }
}


// Checks if the two particles are both photons
// Returns true if they are and false if not
bool He4Pi0::IsTwoPhotons(const TCandidateList &candidates)
{
    if (candidates.size() != 2)
    {
        // Condition not met
        LOG(ERROR) << "Size of candidates should be 2";
        return false;
    }
    else
    {
        // Check if the particles are charged
        bool front_charged = IsParticleCharged(candidates.front().VetoEnergy);
        bool back_charged = IsParticleCharged(candidates.back().VetoEnergy);

        // If either particle is charged, this is not two photons, return false
        if (front_charged || back_charged)
            return false;
        else
            return true;

    }
}


// For 2 particle events.
// Checks if two particles are a photon a He4 nucleus
// based on their veto energy. Returns 0 if they are
// not, returns 1 if the front is a photon and returns
// 2 is the back if a photon.
int He4Pi0::IsChargedUncharged(const TCandidateList& candidates)
{
    // Default is that both particles are charged
    bool isfrontcharged = true;
    bool isbackcharged = true;

    // Must be 2 particles
    if (candidates.size() != 2)
    {
        // Condition not met
        LOG(ERROR) << "Size of candidates should be 2";
        return 0;
    }
    else
    {
        // Checking if particles are uncharged
        if (candidates.front().VetoEnergy < .2)
        {
            isfrontcharged = false;
        }

        if (candidates.back().VetoEnergy < .2)
        {
            isbackcharged = false;
        }

        // Return 1 or 2 if one particle is charged and the other is
        // uncharged (condition met)
        if ((isfrontcharged == false) & (isbackcharged == true))
        {
            return 1;
        }
        else if ((isfrontcharged == true) & (isbackcharged == false))
        {
            return 2;
        }

        // Condition not met
        else { return 0; }
    }
}


// Input: Two photon candidates. Output: the missing
// mass of pi0.
double He4Pi0::GetPi0MissingMass(const TCandidate& front_photon, const TCandidate& back_photon)
{
    vec3 front_unit_vec = vec3(front_photon);
    vec3 back_unit_vec = vec3(back_photon);

    LorentzVec front_scattered = LorentzVec({front_unit_vec.x*front_photon.CaloEnergy,
                                       front_unit_vec.y*front_photon.CaloEnergy,
                                       front_unit_vec.z*front_photon.CaloEnergy},
                                      front_photon.CaloEnergy);

    LorentzVec back_scattered = LorentzVec({back_unit_vec.x*back_photon.CaloEnergy,
                                           back_unit_vec.y*back_photon.CaloEnergy,
                                           back_unit_vec.z*back_photon.CaloEnergy},
                                          back_photon.CaloEnergy);

    return (front_scattered + back_scattered).M();
}


// Input: Two photon candidates. Output: the reconstructed
// pi0 vector.
LorentzVec He4Pi0::GetPi0Vec(const TCandidate& front_photon, const TCandidate& back_photon)
{
    vec3 front_unit_vec = vec3(front_photon);
    vec3 back_unit_vec = vec3(back_photon);

    LorentzVec front_scattered = LorentzVec({front_unit_vec.x*front_photon.CaloEnergy,
                                       front_unit_vec.y*front_photon.CaloEnergy,
                                       front_unit_vec.z*front_photon.CaloEnergy},
                                      front_photon.CaloEnergy);

    LorentzVec back_scattered = LorentzVec({back_unit_vec.x*back_photon.CaloEnergy,
                                           back_unit_vec.y*back_photon.CaloEnergy,
                                           back_unit_vec.z*back_photon.CaloEnergy},
                                          back_photon.CaloEnergy);

    return (front_scattered + back_scattered);
}


// Input: Two photon candidates and the 4 momentum vectors
// of the incoming photon and He4 nucleus target. Output:
// the missing energy of the he4 nucleus.
double He4Pi0::GetHe4MissingEnergy(const TCandidate &front_photon, const TCandidate &back_photon,
                                   const LorentzVec target, const LorentzVec incoming)
{
    vec3 front_unit_vec = vec3(front_photon);
    vec3 back_unit_vec = vec3(back_photon);

    LorentzVec front_scattered = LorentzVec({front_unit_vec.x*front_photon.CaloEnergy,
                                           front_unit_vec.y*front_photon.CaloEnergy,
                                           front_unit_vec.z*front_photon.CaloEnergy},
                                          front_photon.CaloEnergy);

    LorentzVec back_scattered = LorentzVec({back_unit_vec.x*back_photon.CaloEnergy,
                                               back_unit_vec.y*back_photon.CaloEnergy,
                                               back_unit_vec.z*back_photon.CaloEnergy},
                                              back_photon.CaloEnergy);

    LorentzVec scattered = front_scattered + back_scattered;

    return ((incoming + target - scattered).M() - He4_mass);
}


// Input: pi0, target, and incident photon 4-momentum vectors.
// Output: Pi0 missing energy in the CM frame based on He4 target.
double He4Pi0::GetPi0MissingEnergy(const LorentzVec pi0,
                                   const LorentzVec target,
                                   const LorentzVec photon)
{
    // Frame stuff
    LorentzVec total_incoming = target + photon;
    vec3 cmBoost = -total_incoming.BoostVector();

    // Pi0 CM Energy
    LorentzVec pi0_vec_cm = pi0;
    pi0_vec_cm.Boost(cmBoost);
    double pi0_E_cm = pi0_vec_cm.E;

    // Pi0 CM Energy Reconstructed
    double S = total_incoming.M2();
    double pi0_E_cm_rec = (S + pow(pi0_mass,2) - pow(He4_mass,2))/(2*sqrt(S));

    return (pi0_E_cm - pi0_E_cm_rec);
}


// Input: a candidate and the 4 momentum vectors of the
// incoming photon and He4 nucleus target. Output: the missing
// mass
double He4Pi0::GetMissingMass(const TCandidate& candidate,
                               const LorentzVec target,
                               const LorentzVec incoming)
{
    vec3 unit_vec = vec3(candidate);
    LorentzVec scattered = LorentzVec({unit_vec.x*candidate.CaloEnergy,
                                       unit_vec.y*candidate.CaloEnergy,
                                       unit_vec.z*candidate.CaloEnergy},
                                      candidate.CaloEnergy);

    // Calculating the mass of the recoil He4 nucleus (missing
    // mass) from the 4 momentum vector using .M()
    // Should be 3727.84 MeV if there was a Compton event
    // involving this particle
    return (incoming + target - scattered).M();
}

// Used for a 2 particle events.
// Calculates the missing mass using both particles, the outputs
// the one that is closer to the mass of a He4 nucleus (3727.84 MeV)
double He4Pi0::GetCloserMM(const TCandidateList& candidates,
                            const LorentzVec target,
                            const LorentzVec incoming)
{
    if (candidates.size() != 2)
    {
        LOG(ERROR) << "Size of candidates should be 2";
    }
    double front_missing_mass =
            GetMissingMass(candidates.front(), target, incoming);
    double back_missing_mass =
            GetMissingMass(candidates.back(), target, incoming);

    if (abs(front_missing_mass - He4_mass) <
            abs(back_missing_mass - He4_mass))
    {
        return front_missing_mass;
    }
    else if (abs(front_missing_mass - He4_mass) >
             abs(back_missing_mass - He4_mass))
    {
        return back_missing_mass;
    }
    else
    {
        LOG(WARNING) << "Missing Masses are the same";
        return front_missing_mass;
    }
}

// For 2 particle events.
// Checks if the 2 particles are coplanar. Returns true
// if they are and false if they are not.
bool He4Pi0::IsCoplanar(const TCandidateList& candidates)
{
    if (candidates.size() != 2)
    {
        LOG(WARNING) << "Size of candidates should be 2";
        return false;
    }

    else
    {
        double front_phi = candidates.front().Phi;
        double back_phi = candidates.back().Phi;

        // Default value (will not make condition)
        double diff = 0.0;

        if (front_phi > back_phi)
        {
            diff = front_phi - back_phi;
        }
        if (back_phi > front_phi)
        {
            diff = back_phi - front_phi;
        }

        // Condition: phi angles must be 180±10 deg apart. If
        // condition is met -> particles are coplanar
        if (((180*diff/M_PI) > 170) & ((180*diff/M_PI) < 190))    // Jenna note to self: & is fine - returns 0 or 1 instead of bool
        {
            return true;
        }
        else { return false; }
    }
}

// For 2 particle events.
// Check the angle between the calculated and detected
// recoil He4 nucleus (opening angle), should be less than
// 15 deg if event is Compton.
int He4Pi0::IsOpeningAngle(const TCandidateList& candidates,
                            const LorentzVec target,
                            const LorentzVec incoming)
{
    // Lorentz vectors for the scattering photon (scattered)
    // and what should be the recoil He4 (missing) for the
    // first and second particle in the event.
    LorentzVec front_scattered;
    LorentzVec front_missing;
    LorentzVec back_scattered;
    LorentzVec back_missing;

    vec3 front_unit_vec = vec3(candidates.front());
    vec3 back_unit_vec = vec3(candidates.back());

    front_scattered = LorentzVec({front_unit_vec.x*candidates.front().CaloEnergy,
                                  front_unit_vec.y*candidates.front().CaloEnergy,
                                  front_unit_vec.z*candidates.front().CaloEnergy},
                                  candidates.front().CaloEnergy);

    front_missing = incoming + target - front_scattered;

    back_scattered = LorentzVec({back_unit_vec.x*candidates.back().CaloEnergy,
                                 back_unit_vec.y*candidates.back().CaloEnergy,
                                 back_unit_vec.z*candidates.back().CaloEnergy},
                                 candidates.back().CaloEnergy);

    back_missing = incoming + target - back_scattered;

    // Don't know which particle is the photon, so calculate the
    // opening angle twice assuming both are the photon -> take
    // the smaller opening angle
    // THIS MIGHT NOT BE THE BEST WAY TO DO THIS
    double open_ang2 = 180*front_scattered.Angle(back_missing)/M_PI;
    double open_ang1 = 180*back_scattered.Angle(front_missing)/M_PI;

    if (open_ang1 < open_ang2)
    {
        if (open_ang1 < 15.0) { return 1; }
        else { return 0; }
    }

    else
    {
        if (open_ang2 < 15.0) { return 2; }
        else { return 0; }
    }
}

// For 2 particle events.
// Find out which particle is charged first so we know which is which.
// Check the angle between the calculated and detected
// recoil He4 nucleus (opening angle), should be less than
// 15 deg if event is Compton.
bool He4Pi0::IsOpeningAngle2(const TCandidateList& candidates,
                     const LorentzVec target,
                     const LorentzVec incoming,
                     const int IsChargedUncharged_output)
{
    LorentzVec scattered;
    LorentzVec missing;
    LorentzVec recoil;

    vec3 front_unit_vec = vec3(candidates.front());
    vec3 back_unit_vec = vec3(candidates.back());

    double opening_angle;

    if (IsChargedUncharged_output == 1)
    {
        scattered = LorentzVec({front_unit_vec.x*candidates.front().CaloEnergy,
                                    front_unit_vec.y*candidates.front().CaloEnergy,
                                    front_unit_vec.z*candidates.front().CaloEnergy},
                                    candidates.front().CaloEnergy);

        missing = incoming + target - scattered;

        recoil = LorentzVec({back_unit_vec.x*candidates.back().CaloEnergy,
                                 back_unit_vec.y*candidates.back().CaloEnergy,
                                 back_unit_vec.z*candidates.back().CaloEnergy},
                                 candidates.back().CaloEnergy);

        opening_angle = 180*recoil.Angle(missing)/M_PI;

        if (opening_angle < 15.0 ) { return true; }

        else { return false; }
    }

    if (IsChargedUncharged_output == 2)
    {
        scattered = LorentzVec({back_unit_vec.x*candidates.back().CaloEnergy,
                                    back_unit_vec.y*candidates.back().CaloEnergy,
                                    back_unit_vec.z*candidates.back().CaloEnergy},
                                    candidates.back().CaloEnergy);

        missing = incoming + target - scattered;

        recoil = LorentzVec({front_unit_vec.x*candidates.front().CaloEnergy,
                                 front_unit_vec.y*candidates.front().CaloEnergy,
                                 front_unit_vec.z*candidates.front().CaloEnergy},
                                 candidates.front().CaloEnergy);

        opening_angle = 180*recoil.Angle(missing)/M_PI;

        if (opening_angle < 15.0 ) { return true; }

        else { return false; }
    }

    else
    {
        LOG(ERROR) << "Invalid IsChargedUncharged_output, "
                      "function returns false";
        return false;
    }
}

// ------------------------- Other Methods -------------------------

void He4Pi0::PlotCounts()
{
    for ( auto ch = 0u ; ch < nchannels ; ++ch)
    {
        const auto counts = slowcontrol::Variables::TaggerScalers->
                            GetCounts().at(ch);
        h_ScalarCounts->Fill(ch,counts);
    }
}

// ----------------------- Where the Physics Happens -----------------------

void He4Pi0::ProcessEvent(const TEvent& event, manager_t&)
{

//     --------------------- Prompt Random Stuff ---------------------

    // Runs ProcessEvent function in TriggerSimulation file which
    // does the calculations
    triggersimu.ProcessEvent(event);

    for (const auto& taggerhit : event.Reconstructed().TaggerHits)
    {

        // Skipping taggerhits outside the specified energy range
        if ((taggerhit.PhotonEnergy < tagger_energy_low) ||
                (taggerhit.PhotonEnergy > tagger_energy_high))
        {
            continue;
        }

        // Apply trigger simulation to tagger hits
        // This subtracts a weighted time from the CB (see wiki)
        const auto& CorrectedTaggerTime =
                triggersimu.GetCorrectedTaggerTime(taggerhit);

        // This assigns weights to the TaggerHits based on which
        // time window they fall into
        promptrandom.SetTaggerTime(CorrectedTaggerTime);

        // When the Tagger hit is in neither the prompt or random
        // window, then skip
        if (promptrandom.State() == PromptRandom::Case::Outside)
            continue;

        // Plot taggerhits with weights. Weight of 1.0 in
        // prompt, weight of 0.0 in outside and weight of between
        // 0 and 1 in random (for calculus reasons)
        const double weight = promptrandom.FillWeight();
        h_WeightedTaggerTime->Fill(taggerhit.Time, weight);

//     ---------- The Bulk (cuts, and filling histograms) ----------

        // Filling in the momentum 4 vec for the incoming photon
        // using tagger info
        incoming_vec = LorentzVec({0.0,0.0,taggerhit.PhotonEnergy},
                                      taggerhit.PhotonEnergy);

//     ------------- Identifying Compton and Pi0 Events ------------

//             -------------- 1 Particle Events --------------

        if (event.Reconstructed().Candidates.size() == 1)
        {
            for (const auto& candidate : event.Reconstructed().Candidates)
            {

//               ----- Compton ----

                missing_mass = GetMissingMass(candidate, target_vec, incoming_vec);

                if (He4Pi0::IsParticleCharged(candidate.VetoEnergy) == false)
                {
                    // 1 particle in event, particle is uncharged
                    h_MM111->Fill(missing_mass, weight);

                    // Filling 3D Plot
                    h3D_MM111->Fill(missing_mass, candidate.Theta,
                                    taggerhit.Channel, weight);
                }
            }
        }













//         ------------ Pi0 stuff -------------


        if (event.Reconstructed().Candidates.size() == 2)
        {
            const auto& pi0candidates = event.Reconstructed().Candidates;
            if(IsTwoPhotons(pi0candidates))
            {
                // old method
                pi0_missing_mass = GetPi0MissingMass(pi0candidates.front(), pi0candidates.back());
                h_MMpi0->Fill(pi0_missing_mass, weight);

                if ((pi0_missing_mass > 115) && (pi0_missing_mass < 155))  // anything in here is dubbed a pi0 event
                {
                    missing_energy = GetHe4MissingEnergy(pi0candidates.front(), pi0candidates.back(), target_vec, incoming_vec);
                    h_MMhe4->Fill(missing_energy, weight);


                    // Pi0 CM Energy
                    pi0_vec = GetPi0Vec(pi0candidates.front(), pi0candidates.back());
                    pi0_E_miss = GetPi0MissingEnergy(pi0_vec, target_vec, incoming_vec);
                    h_MMpi0_2->Fill(pi0_E_miss, weight);
                }
            }
        }




//             -------------- 2 Particle Events --------------

        if (event.Reconstructed().Candidates.size() == 2)
        {
            const auto& candidates = event.Reconstructed().Candidates;


//          ------- Pi0 Production ------

            // Only keep the particles if they're two photons
            if(IsTwoPhotons(candidates))
            {
                // Calculate pi0 missing mass from the two photons
                pi0_missing_mass = GetPi0MissingMass(candidates.front(), candidates.back());
                h_MMpi0->Fill(pi0_missing_mass, weight);

                // Cut events well outside the pi0 missing mass peak
                if ((pi0_missing_mass > 115) && (pi0_missing_mass < 155))
                {
                    // Method 1: Calculate missing energy of He4 nucleus from the reconstructed pi0
                    // This peak should all be pi0 production events
                    missing_energy = GetHe4MissingEnergy(candidates.front(), candidates.back(), target_vec, incoming_vec);
                    h_MMhe4->Fill(missing_energy, weight);

                    // Method 2: Calculate missing energy of pi0 in CM frame from the expected He4 target
                    // This peak should similarly all be pi0 events
                    pi0_vec = GetPi0Vec(candidates.front(), candidates.back());
                    pi0_E_miss = GetPi0MissingEnergy(pi0_vec, target_vec, incoming_vec);
                    h_MMpi0_2->Fill(pi0_E_miss, weight);
                 }
             }


//          ------ Compton -----

            // Keeping only the 2 particles in which one is charged and
            // the other is not. Using the uncharged particle to calc
            // the missing mass
            if (IsChargedUncharged(candidates) == 1)
            {
                // Opening angle cut
                if( IsOpeningAngle2(candidates,target_vec,incoming_vec, 1) == true )
                {
                    // Colpanar cut
                    if ( IsCoplanar(candidates) == true )
                    {
                        missing_mass = GetMissingMass(candidates.front(),
                                                      target_vec, incoming_vec);

                        // 2 particles in event, one is charged and the other is not,
                        // opening angle < 15 degrees, coplanar
                        h_MM112011_switch->Fill(missing_mass, weight);

                        // Fill 3D histogram
                        h3D_MM112011_switch->Fill(missing_mass,
                                                  candidates.front().Theta,
                                                  taggerhit.Channel,
                                                  weight);
                    }
                }
            }

            // All again but with other configuration
            if (IsChargedUncharged(candidates) == 2)
            {
                // Opening angle cut
                if( IsOpeningAngle2(candidates,target_vec,incoming_vec, 2) == true )
                {
                    // Colpanar cut
                    if ( IsCoplanar(candidates) == true )
                    {
                        missing_mass = GetMissingMass(candidates.back(),
                                                      target_vec, incoming_vec);

                        // 2 particles in event, one is charged and the other is not,
                        // opening angle < 15 degrees, coplanar
                        h_MM112011_switch->Fill(missing_mass, weight);

                        // Fill 3D histogram
                        h3D_MM112011_switch->Fill(missing_mass,
                                                  candidates.front().Theta,
                                                  taggerhit.Channel,
                                                  weight);
                    }
                }

            }

            // Check if opening angle is < 15 deg
            if (IsOpeningAngle(candidates, target_vec, incoming_vec) == 1 )
            {
                // Keep only coplanar events
                if (IsCoplanar(candidates) == true )
                {
                    // Cut charged particles
                    if (IsParticleCharged(candidates.front().VetoEnergy) == false)
                    {
                        missing_mass = GetMissingMass
                                 (candidates.front(), target_vec, incoming_vec);

                        // 2 particles in event, open ang < 15, uncharged,
                        // event is coplanar
                        h_MM112011->Fill(missing_mass, weight);
                        
                        // Fill 3D histogram
                        h3D_MM112011->Fill(missing_mass,
                                           candidates.front().Theta,
                                           taggerhit.Channel,
                                           weight);
                    }
                }
            }

            // All that again but for the other configuration
            if (IsOpeningAngle(candidates, target_vec, incoming_vec) == 2 )
            {
                // Keep only coplanar events
                if (IsCoplanar(candidates) == true )
                {
                    // Cut charged particles
                    if (IsParticleCharged(candidates.front().VetoEnergy) == false)
                    {
                        missing_mass = GetMissingMass
                                 (candidates.back(), target_vec, incoming_vec);

                        // 2 particles in event, open ang < 15, uncharged,
                        // event is coplanar
                        h_MM112011->Fill(missing_mass, weight);

                        // Fill 3D histogram
                        h3D_MM112011->Fill(missing_mass,
                                           candidates.front().Theta,
                                           taggerhit.Channel,
                                           weight);
                    }
                }
            }
        }
    }


    if(slowcontrol::Variables::TaggerScalers->HasChanged())
    {
        seenScalerBlocks++;
	PlotCounts();
    }
}

void He4Pi0::Finish()
{
    h3D_MM111_projX =
            h3D_MM111->ProjectionX();
    h3D_MM112011_projX =
            h3D_MM112011->ProjectionX();
    h3D_MM112011_switch_projX =
            h3D_MM112011_switch->ProjectionX();

    LOG(INFO) << "Seen scaler-blocks: " << seenScalerBlocks;
}

// ---------------------- Outputing the Histograms ----------------------

void He4Pi0::ShowResult()
{

    ant::canvas(GetName()+": Pi0 Production Plots")
	    << h_MMpi0
        << h_MMhe4
        << h_MMpi0_2
	    << endc;

    ant::canvas(GetName()+": Tagger Time Plots")
            << h_WeightedTaggerTime
            << endc; // actually draws the canvas

    ant::canvas(GetName()+": 1 Particle Events")
            << h_MM111
            << h3D_MM111
            << h3D_MM111_projX
            << endc;

    ant::canvas(GetName()+": Events with Opening Angle < 15")
            << h_MM112011
            << h3D_MM112011
            << h3D_MM112011_projX
            << h_MM112011_switch
            << h3D_MM112011_switch
            << h3D_MM112011_switch_projX
            << endc;

    ant::canvas(GetName()+": Scalar Counts")
            << h_ScalarCounts
            << endc;
}

// ---------------- Registering the Class ----------------

// A macro that registers the Compton class with Ant so that
// you can call this class using "Ant -p He4Pi0"
AUTO_REGISTER_PHYSICS(He4Pi0)
