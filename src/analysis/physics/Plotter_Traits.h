#pragma once
#include "analysis/plot/HistogramFactory.h"
#include "base/OptionsList.h"
#include "base/WrapTFile.h"
#include <functional>
#include <memory>
#include <vector>

namespace ant {
namespace analysis {



struct Plotter_Trait {

    Plotter_Trait(const std::string& name, WrapTFileInput& input, const HistogramFactory& HistFactory, OptionsPtr otps) {}

    virtual long long GetNumEntries() const =0;
    virtual bool ProcessEntry(const long long entry) =0;
    virtual void Finish() {}
    virtual void ShowResult() {}

    virtual ~Plotter_Trait() {}

};

using plotter_creator = std::function< std::unique_ptr<Plotter_Trait>(const std::string& name, WrapTFileInput& input, const HistogramFactory& HistFactory, OptionsPtr otps) >;

class PlotterRegistry
{
    friend class PlotterRegistration;

private:
    using plotter_creators_t = std::map<std::string, plotter_creator>;
    plotter_creators_t plotter_creators;
    static PlotterRegistry& get_instance();

    void RegisterPlotter(plotter_creator c, const std::string& name) {
        plotter_creators[name] = c;
    }
public:

    static std::unique_ptr<Plotter_Trait> Create(const std::string& name, WrapTFileInput& input, const HistogramFactory& HistFactory, OptionsPtr opts = std::make_shared<OptionsList>());

    static std::vector<std::string> GetList();

    struct Exception : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

};

class PlotterRegistration
{
public:
    PlotterRegistration(plotter_creator c, const std::string& name);
};

template<class T>
std::unique_ptr<Plotter_Trait> physics_factory(const std::string& name, WrapTFileInput& input, const HistogramFactory& HistFactory, OptionsPtr opts)
{
    return std_ext::make_unique<T>(name, input, HistFactory, opts);
}

#define AUTO_REGISTER_PHYSICS(plotter) \
    ant::analysis::PlotterRegistration _plotter_registration_ ## plotter(ant::analysis::plotter_factory<physics>, #plotter);

}


}
