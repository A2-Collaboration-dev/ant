#pragma once

#include "calibration/Calibration.h"
#include "Energy.h"
#include "base/OptionsList.h"

class TH1D;

namespace ant {

class TH2TAPS;

namespace expconfig {
namespace detector {
struct TAPS;
}}

namespace calibration {

namespace gui {
class FitGausPol3;
}


class TAPS_Energy : public Energy
{


public:

    using detector_ptr_t = std::shared_ptr<const expconfig::detector::TAPS>;

    struct GUI_Gains : GUI_CalibType {
        GUI_Gains(const std::string& basename,
               OptionsPtr options,
               CalibType& type,
               const std::shared_ptr<DataManager>& calmgr,
               const detector_ptr_t& taps_detector_);

        virtual void InitGUI(gui::ManagerWindow_traits* window) override;
        virtual DoFitReturn_t DoFit(TH1* hist, unsigned channel) override;
        virtual void DisplayFit() override;
        virtual void StoreFit(unsigned channel) override;
        virtual bool FinishSlice() override;

    protected:
        std::shared_ptr<gui::FitGausPol3> func;
        gui::CalCanvas* canvas;
        TH1*  h_projection = nullptr;
        TH1D* h_peaks = nullptr;
        TH1D* h_relative = nullptr;

        TH2TAPS* h_peaks_taps = nullptr;
        TH2TAPS* h_relative_taps = nullptr;

        double AutoStopOnChi2 = 6;
        interval<double> FitRange = {80, 250};
        double ConvergenceFactor = 1.0;
        bool SkipTouchesHole = true;

        const std::shared_ptr<const expconfig::detector::TAPS> taps_detector;
    };

    TAPS_Energy(
            const detector_ptr_t& taps,
            const std::shared_ptr<DataManager>& calmgr,
            const Calibration::Converter::ptr_t& converter,
            const std::vector<double>& defaultPedestals,
            const std::vector<double>& defaultGains,
            const std::vector<double>& defaultThresholds,
            const std::vector<double>& defaultRelativeGains);

    virtual void GetGUIs(std::list<std::unique_ptr<calibration::gui::CalibModule_traits> >& guis, ant::OptionsPtr options) override;
protected:
    const detector_ptr_t taps_detector;
};

}} // namespace ant::calibration
