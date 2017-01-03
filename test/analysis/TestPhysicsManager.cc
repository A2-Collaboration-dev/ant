#include "catch.hpp"
#include "catch_config.h"
#include "expconfig_helpers.h"

#include "analysis/physics/PhysicsManager.h"
#include "analysis/input/ant/AntReader.h"
#include "analysis/input/pluto/PlutoReader.h"

#include "analysis/utils/Uncertainties.h"

#include "unpacker/Unpacker.h"
#include "reconstruct/Reconstruct.h"
#include "expconfig/ExpConfig.h"
#include "tree/TAntHeader.h"

#include "base/tmpfile_t.h"
#include "base/WrapTFile.h"

#include "TTree.h"


#include <iostream>
#include <list>

using namespace std;
using namespace ant;
using namespace ant::analysis;

void dotest_raw();
void dotest_raw_nowrite();
void dotest_plutogeant();
void dotest_pluto();
void dotest_runall();

TEST_CASE("PhysicsManager: Raw Input", "[analysis]") {
    test::EnsureSetup();
    dotest_raw();
}

TEST_CASE("PhysicsManager: Raw Input without TEvent writing", "[analysis]") {
    test::EnsureSetup();
    dotest_raw_nowrite();
}

TEST_CASE("PhysicsManager: Pluto/Geant Input", "[analysis]") {
    test::EnsureSetup();
    dotest_plutogeant();
}

TEST_CASE("PhysicsManager: Pluto only Input", "[analysis]") {
    test::EnsureSetup();
    dotest_pluto();
}

TEST_CASE("PhysicsManager: Run all physics", "[analysis]") {
    test::EnsureSetup();
    dotest_runall();
}

struct TestPhysics : Physics
{
    bool finishCalled = false;
    bool showCalled = false;
    bool nowrite    = false;
    unsigned seenEvents = 0;
    unsigned seenTaggerHits = 0;
    unsigned seenCandidates = 0;
    unsigned seenMCTrue = 0;
    unsigned seenTrueTargetPos = 0;
    unsigned seenReconTargetPosNaN = 0;



    TestPhysics(bool nowrite_ = false) :
        Physics("TestPhysics", nullptr),
        nowrite(nowrite_)
    {
        HistFac.makeTH1D("test","test","test",BinSettings(10));
    }

    virtual void ProcessEvent(const TEvent& event, physics::manager_t& manager) override
    {
        seenEvents++;
        seenTaggerHits += event.Reconstructed().TaggerHits.size();
        seenCandidates += event.Reconstructed().Candidates.size();
        seenMCTrue += event.MCTrue().Particles.GetAll().size();
        // make sure it's non-zero and not nan only for MCTrue
        seenTrueTargetPos += event.MCTrue().Target.Vertex.z < -1;
        seenReconTargetPosNaN += std::isnan(event.Reconstructed().Target.Vertex.z);
        // request to save every third event
        if(!nowrite && seenEvents % 3 == 0)
            manager.SaveEvent();
    }
    virtual void Finish() override
    {
        finishCalled = true;
    }
    virtual void ShowResult() override
    {
        showCalled = true;
    }
};

struct PhysicsManagerTester : PhysicsManager
{
    using PhysicsManager::PhysicsManager;

    shared_ptr<TestPhysics> GetTestPhysicsModule() {
        // bit ugly to obtain the physics module back
        auto module = move(physics.back());
        physics.pop_back();
        return dynamic_pointer_cast<TestPhysics, Physics>(
                    std::shared_ptr<Physics>(module.release()));
    }
};

void dotest_raw()
{
    const unsigned expectedEvents = 221;

    tmpfile_t tmpfile;

    // write out some file
    {
        WrapTFileOutput outfile(tmpfile.filename, WrapTFileOutput::mode_t::recreate, true);
        PhysicsManagerTester pm;
        pm.AddPhysics<TestPhysics>();

        // make some meaningful input for the physics manager
        auto unpacker = Unpacker::Get(string(TEST_BLOBS_DIRECTORY)+"/Acqu_oneevent-big.dat.xz");
        auto reconstruct = std_ext::make_unique<Reconstruct>();
        list< unique_ptr<analysis::input::DataReader> > readers;
        readers.emplace_back(std_ext::make_unique<input::AntReader>(nullptr, move(unpacker), move(reconstruct)));
        pm.ReadFrom(move(readers), numeric_limits<long long>::max());
        TAntHeader header;
        pm.SetAntHeader(header);

        const std::uint32_t timestamp = 1408221194;
        REQUIRE(header.FirstID == TID(timestamp, 0u));
        REQUIRE(header.LastID == TID(timestamp, expectedEvents-1));

        std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

        REQUIRE(physics->finishCalled);
        REQUIRE_FALSE(physics->showCalled);

        REQUIRE(physics->seenEvents == expectedEvents);
        REQUIRE(physics->seenCandidates == 862);
        REQUIRE(physics->seenTrueTargetPos == 0);
        REQUIRE(physics->seenReconTargetPosNaN == expectedEvents);

        // quick check if TTree was there...
        auto tree = outfile.GetSharedClone<TTree>("treeEvents");
        REQUIRE(tree != nullptr);
        REQUIRE(tree->GetEntries() == expectedEvents/3);
    }

    // read in file with AntReader
    {
        auto inputfiles = make_shared<WrapTFileInput>(tmpfile.filename);

        PhysicsManagerTester pm;
        pm.AddPhysics<TestPhysics>();

        // make some meaningful input for the physics manager

        auto reconstruct = std_ext::make_unique<Reconstruct>();
        list< unique_ptr<analysis::input::DataReader> > readers;

        readers.emplace_back(std_ext::make_unique<input::AntReader>(inputfiles, nullptr, move(reconstruct)));
        pm.ReadFrom(move(readers), numeric_limits<long long>::max());
        TAntHeader header;
        pm.SetAntHeader(header);

        // note that we actually requested every third event to be saved in the physics class
        const std::uint32_t timestamp = 1408221194;
        REQUIRE(header.FirstID == TID(timestamp, 2u));
        REQUIRE(header.LastID == TID(timestamp, 3*unsigned((expectedEvents-1)/3)-1));

        std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

        REQUIRE(physics->seenEvents == expectedEvents/3);
        // make sure the reconstruction wasn't applied twice!
        REQUIRE(physics->seenCandidates == 286);

    }

    // read in file with AntReader without reconstruction
    {
        auto inputfiles = make_shared<WrapTFileInput>(tmpfile.filename);

        PhysicsManagerTester pm;
        pm.AddPhysics<TestPhysics>();

        // make some meaningful input for the physics manager

        list< unique_ptr<analysis::input::DataReader> > readers;
        readers.emplace_back(std_ext::make_unique<input::AntReader>(inputfiles, nullptr, nullptr));
        pm.ReadFrom(move(readers), numeric_limits<long long>::max());
        TAntHeader header;
        pm.SetAntHeader(header);

        // note that we actually requested every third event to be saved in the physics class
        const std::uint32_t timestamp = 1408221194;
        REQUIRE(header.FirstID == TID(timestamp, 2u));
        REQUIRE(header.LastID == TID(timestamp, 3*unsigned((expectedEvents-1)/3)-1));

        std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

        REQUIRE(physics->seenEvents == expectedEvents/3);
        REQUIRE(physics->seenCandidates == 286);

    }

}

void dotest_raw_nowrite()
{
    tmpfile_t tmpfile;
    WrapTFileOutput outfile(tmpfile.filename, WrapTFileOutput::mode_t::recreate, true);

    PhysicsManagerTester pm;
    pm.AddPhysics<TestPhysics>(true);

    // make some meaningful input for the physics manager
    auto unpacker = Unpacker::Get(string(TEST_BLOBS_DIRECTORY)+"/Acqu_oneevent-big.dat.xz");
    auto reconstruct = std_ext::make_unique<Reconstruct>();
    list< unique_ptr<analysis::input::DataReader> > readers;
    readers.emplace_back(std_ext::make_unique<input::AntReader>(nullptr, move(unpacker), move(reconstruct)));
    pm.ReadFrom(move(readers), numeric_limits<long long>::max());
    TAntHeader header;
    pm.SetAntHeader(header);

    const std::uint32_t timestamp = 1408221194;
    const unsigned expectedEvents = 221;
    REQUIRE(header.FirstID == TID(timestamp, 0u));
    REQUIRE(header.LastID == TID(timestamp, expectedEvents-1));

    std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

    REQUIRE(physics->finishCalled);
    REQUIRE_FALSE(physics->showCalled);

    CHECK(physics->seenEvents == expectedEvents);
    CHECK(physics->seenTaggerHits == 6272);
    CHECK(physics->seenCandidates == 862);

    // the PhysicsManager should not create a TTree...
    REQUIRE(outfile.GetSharedClone<TTree>("treeEvents") == nullptr);
}

void dotest_plutogeant()
{
    tmpfile_t tmpfile;
    WrapTFileOutput outfile(tmpfile.filename, WrapTFileOutput::mode_t::recreate, true);

    PhysicsManagerTester pm;
    pm.AddPhysics<TestPhysics>();

    // make some meaningful input for the physics manager
    auto unpacker = Unpacker::Get(string(TEST_BLOBS_DIRECTORY)+"/Geant_with_TID.root");
    auto reconstruct = std_ext::make_unique<Reconstruct>();
    list< unique_ptr<analysis::input::DataReader> > readers;
    readers.emplace_back(std_ext::make_unique<input::AntReader>(nullptr, move(unpacker), move(reconstruct)));

    auto plutofile = std::make_shared<WrapTFileInput>(string(TEST_BLOBS_DIRECTORY)+"/Pluto_with_TID.root");
    readers.push_back(std_ext::make_unique<analysis::input::PlutoReader>(plutofile));

    pm.ReadFrom(move(readers), numeric_limits<long long>::max());

    std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

    CHECK(physics->seenEvents == 100);
    CHECK(physics->seenTaggerHits == 100);
    CHECK(physics->seenCandidates == 309);
    CHECK(physics->seenMCTrue == 300);
    CHECK(physics->seenTrueTargetPos == 46);
    CHECK(physics->seenReconTargetPosNaN == 100);
}

void dotest_pluto()
{
    tmpfile_t tmpfile;
    WrapTFileOutput outfile(tmpfile.filename, WrapTFileOutput::mode_t::recreate, true);

    PhysicsManagerTester pm;
    pm.AddPhysics<TestPhysics>();

    // make some meaningful input for the physics manager
    list< unique_ptr<analysis::input::DataReader> > readers;
    auto plutofile = std::make_shared<WrapTFileInput>(string(TEST_BLOBS_DIRECTORY)+"/Pluto_with_TID.root");
    readers.push_back(std_ext::make_unique<analysis::input::PlutoReader>(plutofile));

    pm.ReadFrom(move(readers), numeric_limits<long long>::max());

    std::shared_ptr<TestPhysics> physics = pm.GetTestPhysicsModule();

    CHECK(physics->seenEvents == 100);
    CHECK(physics->seenTaggerHits == 0); // recon only
    CHECK(physics->seenCandidates == 0); // recon only
    CHECK(physics->seenMCTrue == 300);
    CHECK(physics->seenTrueTargetPos == 0); // pluto does not know anything about target
    CHECK(physics->seenReconTargetPosNaN == 100);
}

void dotest_runall() {

    for(auto name : PhysicsRegistry::GetList()) {
        PhysicsManager pm;
        INFO(name);
        try {
            pm.AddPhysics(PhysicsRegistry::Create(name));
        }
        catch(ExpConfig::ExceptionNoDetector) {
            // ignore silently if test setup did not provide detector
            continue;
        }
        catch(utils::UncertaintyModel::Exception) {
            // ignore silently if class cannot load uncertainty model
            continue;
        }
        catch(const std::exception& e) { // use reference to prevent object slicing!
            FAIL(string("Unexpected exception while creating physics class: ")+e.what());
        }
        catch(...) {
            FAIL("Something weird was thrown.");
        }
        auto unpacker = Unpacker::Get(string(TEST_BLOBS_DIRECTORY)+"/Acqu_twoscalerblocks.dat.xz");
        auto reconstruct = std_ext::make_unique<Reconstruct>();
        list< unique_ptr<analysis::input::DataReader> > readers;
        readers.emplace_back(std_ext::make_unique<input::AntReader>(nullptr, move(unpacker), move(reconstruct)));

        REQUIRE_NOTHROW(pm.ReadFrom(move(readers), 20));
    }

}
