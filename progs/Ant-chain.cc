/**
  * @file Ant-chain.cc
  * @brief hadd like tool to concatenate TTrees in ROOT files using TChains.
  *
  *        Creates a TChain in the output file for each TTree found in the first input file
  *        and uses AddFile() to add each input file to each TChain.
  */

//Ant
#include "base/Logger.h"
#include "base/CmdLine.h"
#include "base/WrapTFile.h"
#include "base/std_ext/string.h"
#include "base/std_ext/system.h"
#include "base/std_ext/memory.h"

//ROOT
#include "TDirectory.h"
#include "TChain.h"
#include "TTree.h"

//stl
#include <string>
#include <list>
#include <functional>

using namespace std;
using namespace ant;

template<typename T>
string get_path(T* dir) {
    if(dir && dir->GetMotherDir()) {
        auto prev = get_path(dir->GetMotherDir());
        return std_ext::formatter() << prev << (prev.empty() ? "" : "/") << dir->GetName();
    }
    else {
        return "";
    }
}

set<string> GetTreeNames(const std::vector<string> filenames) {
    set<string> chains; // we use a set to avoid adding the TTree more than once
    for(const auto& filename : filenames) {
        try {
            WrapTFileInput firstfile(filename);
            firstfile.Traverse([&chains] (TKey* key) {
                if(string(key->GetClassName()) == "TTree")
                    chains.insert(get_path(key));
            });
            break;
        }
        catch(WrapTFile::Exception e) {
            LOG(WARNING) << "File " << filename << " appears invalid: " << e.what();
            continue;
        }
    }
    return chains;
}

int main(int argc, char** argv) {
    SetupLogger();

    TCLAP::CmdLine cmd("Ant-chain", ' ', "0.1");
    auto cmd_verbose = cmd.add<TCLAP::ValueArg<int>>("v","verbose","Verbosity level (0..9)", false, 0,"level");
    auto cmd_output     = cmd.add<TCLAP::ValueArg<string>>("o","output","Output file",true,"","filename");
    auto cmd_inputfiles = cmd.add<TCLAP::UnlabeledMultiArg<string>>("inputfiles","input root files",true,"inputfiles");
    auto cmd_macrooverwrite  = cmd.add<TCLAP::SwitchArg>("","macrooverwrite","Overwrite existing macro file",false);
    auto cmd_nomacro = cmd.add<TCLAP::SwitchArg>("","nomacro","Do not create a macro file",false);
    auto cmd_proofworkers  = cmd.add<TCLAP::ValueArg<unsigned>>("","proofworkers","Specify number of workers for PROOF, =0 disables it.",false,8,"n");
    auto cmd_ignoretreeevents  = cmd.add<TCLAP::SwitchArg>("","ignoretreeevents","Ignore the ubiquitious treeEvents",false);
    auto cmd_checkentries = cmd.add<TCLAP::SwitchArg>("","checkentries","Check the total entries of tree (might be slow)",false);

    cmd.parse(argc, argv);


    if(cmd_verbose->isSet())
        el::Loggers::setVerboseLevel(cmd_verbose->getValue());

    if(cmd_inputfiles->isSet()) {
        const auto& inputs = cmd_inputfiles->getValue();

        const auto chain_names = GetTreeNames(inputs);
        if(chain_names.empty()) {
            LOG(ERROR) << "No TTrees found in input files";
            return EXIT_FAILURE;
        }

        const string outfilename = cmd_output->getValue();
        WrapTFileOutput outfile(outfilename,
                                WrapTFileOutput::mode_t::recreate,
                                true);
        list<TChain*> chains;
        for(const auto& name : chain_names) {
            if(cmd_ignoretreeevents->getValue() && name == "treeEvents")
                continue;
            auto chain = new TChain(name.c_str());
            chains.push_back(chain);
            gDirectory->Add(chain);
        }

        for(const auto& file : inputs) {
            for(auto chain : chains) {
                const auto absFile = std_ext::system::absolutePath(file);
                const auto res = chain->AddFile(absFile.c_str());
                if(res != 1) {
                    LOG(WARNING) << "Problem with " << chain->GetName() << " and file " << file << " (" << res << ")";
                }
            }
            LOG(INFO) << "Added " << file;
        }

        unique_ptr<ofstream> macrofile;
        const string macrofilename = outfilename + ".C";
        if(!cmd_nomacro->getValue()) {
            if(!std_ext::system::testopen(macrofilename) || cmd_macrooverwrite->getValue()) {
                macrofile = std_ext::make_unique<ofstream>();
                macrofile->open(macrofilename);
                *macrofile << "{" << endl;
                *macrofile << "TFile* myfile = TFile::Open(\"" << outfilename << "\");" << endl;
                if(cmd_proofworkers->getValue()>0)
                    *macrofile << "TProof::Open(\"workers=" << cmd_proofworkers->getValue() << "\");" << endl;
            }
            else {
                LOG(WARNING) << "Macro '" << macrofilename <<  "' already exists. Not overwriting it. Use --macrooverwrite to force it.";
            }
        }

        unsigned n = 0;
        for(auto chain : chains) {
            if(cmd_checkentries->isSet())
                LOG(INFO) << "Chain: " << chain->GetName() << ": " << chain->GetEntries() << " Entries";
            else
                LOG(INFO) << "Chain: " << chain->GetName();
            if(macrofile) {
                string chainname = "tree";
                if(chains.size()>1)
                    chainname += to_string(n);
                *macrofile << "TChain* " << chainname << " = myfile->GetKey(\""
                           << chain->GetName() <<  "\")->ReadObj();" << endl;
                if(cmd_proofworkers->getValue()>0)
                    *macrofile << chainname << "->SetProof();" << endl;

            }
            n++;
        }

        if(macrofile) {
            *macrofile << "}" << endl;
            macrofile->close();
            LOG(INFO) << "Created macro '" << macrofilename << "'. Use it when starting ROOT.";
        }

    }

    return EXIT_SUCCESS;
}
