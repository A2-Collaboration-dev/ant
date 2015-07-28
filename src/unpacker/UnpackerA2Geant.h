#pragma once

#include "Unpacker.h"



#include "expconfig/ExpConfig.h"
#include "expconfig/Detector_t.h"

#include "Rtypes.h"

#include <memory>
#include <list>
#include <map>
#include <vector>
#include <cstdint>
#include <limits>

class TTree;
class TFile;

namespace ant {


class UnpackerA2Geant : public Unpacker::Module
{
public:
    UnpackerA2Geant();
    virtual ~UnpackerA2Geant();
    virtual bool OpenFile(const std::string& filename) override;
    virtual std::shared_ptr<TDataRecord> NextItem() noexcept override;

    class Exception : public Unpacker::Exception {
        using Unpacker::Exception::Exception; // use base class constructor
    };

private:
    std::uint32_t ID_upper;
    std::uint32_t ID_lower;

    unique_ptr<TFile> tfile;
    TTree* geant;


    // keep in syn with A2CBoutput.h in a2geant
    static constexpr int GEANT_MAX_TAPSHITS = 438;
    static constexpr int GEANT_MAX_CBHITS   = 720;
    static constexpr int GEANT_MAX_PIDHITS  =  24;
    static constexpr int GEANT_MAX_MWPCHITS = 400;
    static constexpr int GEANT_MAX_PART = 100;

    // Brach memories
    Int_t           fnhits = 0;
    Int_t           fnpart = 0;
    Int_t           fntaps = 0;
    Int_t           fnvtaps = 0;
    Int_t           fvhits = 0;
    Float_t         plab[GEANT_MAX_PART] = {};
    Float_t         tctaps[GEANT_MAX_TAPSHITS] = {};
    Float_t         fvertex[3] = {};
    Float_t         fbeam[5] = {};
    Float_t         dircos[GEANT_MAX_PART][3] = {};
    Float_t         ecryst[GEANT_MAX_CBHITS] = {};
    Float_t         tcryst[GEANT_MAX_CBHITS] = {};
    Float_t         ectapfs[GEANT_MAX_TAPSHITS] = {};
    Float_t         ectapsl[GEANT_MAX_TAPSHITS] = {};
    Float_t         elab[GEANT_MAX_PART] = {};
    Float_t         feleak = 0;
    Float_t         fenai = 0;
    Float_t         fetot = 0;
    Float_t         eveto[GEANT_MAX_PIDHITS] = {};
    Float_t         tveto[GEANT_MAX_PIDHITS] = {};
    Float_t         evtaps[GEANT_MAX_TAPSHITS] = {};
    Int_t           icryst[GEANT_MAX_CBHITS] = {};
    Int_t           ictaps[GEANT_MAX_TAPSHITS] = {};
    Int_t           ivtaps[GEANT_MAX_TAPSHITS] = {};
    Int_t           idpart[GEANT_MAX_PART] = {};
    Int_t           iveto[GEANT_MAX_PIDHITS] = {};
    Int_t           fnmwpc = 0;
    Int_t           imwpc[GEANT_MAX_MWPCHITS] = {};
    Float_t         mposx[GEANT_MAX_MWPCHITS] = {};
    Float_t         mposy[GEANT_MAX_MWPCHITS] = {};
    Float_t         mposz[GEANT_MAX_MWPCHITS] = {};
    Float_t         emwpc[GEANT_MAX_MWPCHITS] = {};
    Long64_t        mc_evt_id = -1;
    Long64_t        mc_rnd_id = -1;
};

} // namespace ant