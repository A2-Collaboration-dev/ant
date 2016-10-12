#include "PhysicsManager.h"

#include "utils/ParticleID.h"
#include "input/DataReader.h"

#include "tree/TSlowControl.h"
#include "tree/TAntHeader.h"
#include "base/Logger.h"

#include "slowcontrol/SlowControlManager.h"

#include "base/ProgressCounter.h"

#include "TTree.h"

#include <iomanip>


using namespace std;
using namespace ant;
using namespace ant::analysis;

PhysicsManager::PhysicsManager(volatile bool* interrupt_) :
    physics(),
    interrupt(interrupt_)
{}

PhysicsManager::~PhysicsManager() {}

void PhysicsManager::SetParticleID(std::unique_ptr<utils::ParticleID> pid) {
    particleID = move(pid);
}

void PhysicsManager::SetAntHeader(TAntHeader& header)
{
    header.FirstID = firstID;
    header.LastID = lastID;
}

void PhysicsManager::ShowResults()
{
    for(auto& p : physics) {
        p->ShowResult();
    }
}

void PhysicsManager::InitReaders(readers_t readers_)
{
    amenders = move(readers_);

    // figure out what set of readers we have
    // we expect maximum one source and several amenders
    auto it_amender = amenders.begin();
    while(it_amender != amenders.end()) {
        if((*it_amender)->IsSource()) {
            if(source != nullptr)
                throw Exception("Found more than one source in given readers");
            source = move(*it_amender);
            it_amender = amenders.erase(it_amender);
        }
        else {
            ++it_amender;
        }
    }
}


void PhysicsManager::ReadFrom(
        list<unique_ptr<input::DataReader> > readers_,
        long long maxevents)
{
    InitReaders(move(readers_));

    if(physics.empty())
        throw Exception("No analysis instances activated. Cannot not analyse anything.");

    // prepare slowcontrol, init here since physics classes
    // register slowcontrol variables in constructor
    slowcontrol_mgr = std_ext::make_unique<SlowControlManager>();


    // prepare output of TEvents
    treeEvents = new TTree("treeEvents","TEvent data");
    treeEventPtr = nullptr;
    treeEvents->Branch("data", addressof(treeEventPtr));

    long long nEventsRead = 0;
    long long nEventsProcessed = 0;
    long long nEventsAnalyzed = 0;
    long long nEventsSaved = 0;

    bool reached_maxevents = false;


    ProgressCounter progress(
                [this, &nEventsAnalyzed, maxevents]
                (std::chrono::duration<double> elapsed)
    {
        const double percent = maxevents == numeric_limits<decltype(maxevents)>::max() ?
                                   source->PercentDone() :
                                   (double)nEventsAnalyzed/maxevents;

        static double last_PercentDone = 0;
        const double speed = (percent - last_PercentDone)/elapsed.count();
        LOG(INFO) << setw(2) << std::setprecision(4)
                  << percent*100 << " % done, ETA: " << ProgressCounter::TimeToStr((1-percent)/speed);
        last_PercentDone = percent;
    });
    while(true) {
        if(reached_maxevents || interrupt)
            break;

        // read events until slowcontrol_mgr is complete
        while(true) {
            if(interrupt) {
                VLOG(3) << "Reading interrupted";
                break;
            }

            TEvent event;
            if(!TryReadEvent(event)) {
                VLOG(5) << "No more events to read, finish.";
                reached_maxevents = true;
                break;
            }
            nEventsRead++;

            // dump it into slowcontrol until full...
            if(slowcontrol_mgr->ProcessEvent(move(event)))
                break;
            // ..or max buffersize reached: 20000 corresponds to two Acqu Scaler blocks
            if(slowcontrol_mgr->BufferSize()>20000) {
                throw Exception(std_ext::formatter() <<
                                "Slowcontrol buffer reached maximum size " << slowcontrol_mgr->BufferSize()
                                << " without becoming complete. Stopping.");
            }
        }

        // read the slowcontrol_mgr's buffer and process the events
        while(auto buf_event = slowcontrol_mgr->PopEvent()) {

            TEvent& event = buf_event.Event;

            if(interrupt) {
                VLOG(3) << "Processing interrupted";
                break;
            }

            logger::DebugInfo::nProcessedEvents = nEventsProcessed;

            physics::manager_t manager;

            // if we've already reached the maxevents,
            // we just postprocess the remaining slowcontrol buffer (if any)
            if(!reached_maxevents) {
                if(nEventsAnalyzed == maxevents) {
                    VLOG(3) << "Reached max Events " << maxevents;
                    reached_maxevents = true;
                    // we cannot simply break here since might
                    // need to save stuff for slowcontrol purposes
                    if(slowcontrol_mgr->BufferSize()==0)
                        break;
                }

                if(!reached_maxevents && !buf_event.WantsSkip) {
                    ProcessEvent(event, manager);

                    // prefer Reconstructed ID, but at least one branch should be non-null
                    const auto& eventid = event.HasReconstructed() ? event.Reconstructed().ID : event.MCTrue().ID;
                    if(nEventsAnalyzed==0)
                        firstID = eventid;
                    lastID = eventid;

                    nEventsAnalyzed++;

                    if(manager.saveEvent)
                        nEventsSaved++;
                }
            }

            // SaveEvent is the sink for events
            SaveEvent(move(event), manager);

            nEventsProcessed++;
        }
        ProgressCounter::Tick();
    }

    for(auto& pclass : physics) {
        pclass->Finish();
    }

    VLOG(5) << "First EventId processed: " << firstID;
    VLOG(5) << "Last  EventId processed: " << lastID;

    string processed_str;
    if(nEventsProcessed != nEventsAnalyzed)
        processed_str += std_ext::formatter() << " (" << nEventsProcessed << " processed)";
    if(nEventsRead != nEventsAnalyzed)
        processed_str += std_ext::formatter() << " (" << nEventsRead << " read)";


    LOG(INFO) << "Analyzed " << nEventsAnalyzed << " events"
              << processed_str << ", speed "
              << nEventsProcessed/progress.GetTotalSecs() << " event/s";

    const auto nEventsSavedTotal = treeEvents->GetEntries();
    if(nEventsSaved==0) {
        if(nEventsSavedTotal>0)
            VLOG(5) << "Deleting " << nEventsSavedTotal << " treeEvents from slowcontrol only";
        delete treeEvents;
    }
    else if(treeEvents->GetCurrentFile() != nullptr) {
        treeEvents->Write();
        const auto n_sc = nEventsSavedTotal - nEventsSaved;
        LOG(INFO) << "Wrote " << nEventsSaved  << " treeEvents"
                  << (n_sc>0 ? string(std_ext::formatter() << " (+slowcontrol: " << n_sc << ")") : "")
                  << ": "
                  << (double)treeEvents->GetTotBytes()/(1 << 20) << " MB (uncompressed), "
                  << (double)treeEvents->GetTotBytes()/nEventsSavedTotal << " bytes/event";
    }

    // cleanup readers (important for stopping progress output)
    source = nullptr;
    amenders.clear();
}


bool PhysicsManager::TryReadEvent(TEvent& event)
{
    bool event_read = false;
    if(source) {
        if(!source->ReadNextEvent(event)) {
            return false;
        }
        event_read = true;
    }


    auto it_amender = amenders.begin();
    while(it_amender != amenders.end()) {

        if((*it_amender)->ReadNextEvent(event)) {
            ++it_amender;
            event_read = true;
        }
        else {
            it_amender = amenders.erase(it_amender);
        }
    }

    return event_read;
}

void PhysicsManager::ProcessEvent(TEvent& event, physics::manager_t& manager)
{
    if(particleID && event.HasReconstructed()) {
        // run particle ID for Reconstructed candidates
        // but only if there are no identified particles present yet
        /// \todo implement flag to force particle ID again?
        TEventData& recon = event.Reconstructed();
        if(recon.Particles.GetAll().empty()) {
            for(auto cand : recon.Candidates.get_iter()) {
                auto particle = particleID->Process(cand);
                if(particle)
                    recon.Particles.Add(particle);
            }
        }
    }

    event.EnsureTempBranches();

    // run the physics classes
    for( auto& m : physics ) {
        m->ProcessEvent(event, manager);
    }

    event.ClearTempBranches();
}

void PhysicsManager::SaveEvent(TEvent event, const physics::manager_t& manager)
{
    if(manager.saveEvent || event.SavedForSlowControls) {
        // only warn if manager says it should save
        if(!treeEvents->GetCurrentFile() && manager.saveEvent)
            LOG_N_TIMES(1, WARNING) << "Writing treeEvents to memory. Might be a lot of data!";


        // always keep read hits if saving for slowcontrol
        if(!manager.keepReadHits && !event.SavedForSlowControls)
            event.ClearDetectorReadHits();

        treeEventPtr = addressof(event);
        treeEvents->Fill();
    }
}
