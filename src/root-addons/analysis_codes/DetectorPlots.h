#ifndef DETECTOR_PLOTS_H
#define DETECTOR_PLOTS_H

#include <string>

namespace ant {
class TH2CB;
class TH2TAPS;
}

class DetectorPlots {
public:
    static ant::TH2TAPS* MakeTAPSTheta(const std::string& setup_name);
    static ant::TH2TAPS* MakeTAPSPhi(const std::string& setup_name);

    static ant::TH2CB*   MakeCBTheta(const std::string& setup_name);
    static ant::TH2CB*   MakeCBPhi(const std::string& setup_name);

    static void          PlotCBTheta(const std::string& setup_name);
    static void          PlotCBPhi(const std::string& setup_name);

    /**
     * @brief Plot all ignored Crystal Ball Elements in a ant::TH2CB histogram in a new Canvas
     * @param setup_name A valid Ant setup name
     */
    static void          PlotCBIgnored(const std::string& setup_name);


    static void          PlotTAPSTheta(const std::string& setup_name);
    static void          PlotTAPSPhi(const std::string& setup_name);

    /**
     * @brief Plot all ignored TAPS Elements in a ant::TH2TAPS histogram in a new Canvas
     * @param setup_name A valid Ant setup name
     */
    static void          PlotTAPSIgnored(const std::string& setup_name);

    static void          PlotDetectorPositions(const std::string& setup_name, double CB_gap = 0.0);

};

#endif
