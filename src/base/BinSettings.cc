#include "BinSettings.h"
#include <cmath>
#include <numeric>

using namespace std;
using namespace ant;

BinSettings BinSettings::RoundToBinSize(const BinSettings& bins, const double binSize) {

    unsigned nOptBins = unsigned(bins.Length() / binSize);

    const auto factor = unsigned(std::round(double(nOptBins) / double(bins.Bins())));

    if(nOptBins >= binSize) {
         nOptBins /= factor;
    } else {
        nOptBins *= factor;
    }

    const auto length = binSize * nOptBins * factor;

    return BinSettings(nOptBins, interval<double>::CenterWidth(bins.Center(), length));
}

BinSettings BinSettings::Make(const vector<double>& x_values)
{
    if(x_values.size()<2)
        throw runtime_error("Given x_values should contain at least 2 elements");
    vector<double> diffs(x_values.size());
    std::adjacent_difference(x_values.begin(), x_values.end(), diffs.begin());
    if(std::find_if(diffs.begin(), diffs.end(), [] (double v) { return v<=0; }) != diffs.end())
        throw runtime_error("Given x_values not strictly monotonic");
    double mean_distance = std::accumulate(std::next(diffs.begin()), diffs.end(), 0.0) / diffs.size();
    return { static_cast<unsigned>(x_values.size()),
                x_values.front() - mean_distance/2.0,
                x_values.back()  + mean_distance/2.0 };
}

int BinSettings::getBin(const double v) const noexcept {
    if(this->Contains(v)) {
        const auto x = v - this->Start();
        const auto t = x / this->Length(); // [0..1]
        const auto bin = bins * t; // [0..bins]
        return int(std::floor(bin));
    } else
        return -1;
}
