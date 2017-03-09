#pragma once
#include "base/BinSettings.h"
#include "TAxis.h"
#include "TH1.h"
#include "TH2.h"


#include <stdexcept>

namespace ant {

namespace TH_ext {

inline BinSettings getBins(const TAxis* axis) noexcept {
    return { unsigned(axis->GetNbins()), axis->GetXmin(), axis->GetXmax()};
}


inline bool haveSameBinning(const TAxis* a1, const TAxis* a2) noexcept {
    return getBins(a1) == getBins(a2);
}

inline bool haveSameBinning(const TH1* h1, const TH1* h2) noexcept {
    return getBins(h1->GetXaxis()) == getBins(h2->GetXaxis());
}

inline bool haveSameBinning(const TH2* h1, const TH2* h2) noexcept {
    return   getBins(h1->GetXaxis()) == getBins(h2->GetXaxis())
          && getBins(h1->GetYaxis()) == getBins(h2->GetYaxis());
}

inline std::pair<int,int> getBinNums(double x, double y, const TH2* hist)
{
    const auto bin = hist->FindFixBin(x,y);
    int binx;
    int biny;
    int dummy;
    hist->GetBinXYZ(bin,binx,biny,dummy);
    return {binx,biny};
}

template <typename T>
inline T* Clone(const T* obj, const std::string& name)
{
    auto clone = dynamic_cast<T*>(obj->Clone(name.c_str()));

    if(!clone)
        throw std::bad_cast();

    return clone;
}

template <typename Func>
inline TH1* Apply(const TH1* h1, Func f) {

    auto res = Clone(h1, "");

    for(int bin=0; bin<=res->GetNbinsX()+1; ++bin) {
        res->SetBinContent(bin, f(h1->GetBinContent(bin)));
    }

    return res;
}

template <typename Func>
inline TH1* Apply(const TH1* h1, const TH1* h2, Func f) {

    if (!haveSameBinning(h1,h2)) {
        throw std::runtime_error("Incompatible X-Axis");
    }

    auto res = Clone(h1, "");

    for(int bin=0; bin<=res->GetNbinsX()+1; ++bin) {
        res->SetBinContent(bin, f(h1->GetBinContent(bin), h2->GetBinContent(bin)));
    }

    return res;
}

template <typename Func>
inline TH2* Apply(const TH2* h1, Func f) {

    auto res = Clone(h1, "");

    for(int binx=0; binx<=res->GetNbinsX()+1; ++binx) {
        for(int biny=0; biny<=res->GetNbinsY()+1; ++biny) {
            res->SetBinContent(binx, biny, f(h1->GetBinContent(binx,biny)));
        }
    }

    return res;
}

template <typename Func>
inline TH2* Apply(const TH2* h1, const TH2* h2, Func f) {

    if (!haveSameBinning(h1,h2)) {
        throw std::runtime_error("Incompatible X/Y-Axis");
    }

    auto res = Clone(h1, "");

    for(int binx=0; binx<=res->GetNbinsX()+1; ++binx) {
        for(int biny=0; biny<=res->GetNbinsY()+1; ++biny) {
            res->SetBinContent(binx, biny, f(h1->GetBinContent(binx,biny), h2->GetBinContent(binx,biny)));
        }
    }

    return res;
}

template <typename Func>
inline TH2* Apply(const TH2* h1, const TH2* h2, const TH2* h3, Func f) {

    if (!haveSameBinning(h1,h2) || !haveSameBinning(h2,h3)) {
        throw std::runtime_error("Incompatible X/Y-Axis");
    }

    auto res = Clone(h1, "");

    for(int binx=0; binx<=res->GetNbinsX()+1; ++binx) {
        for(int biny=0; biny<=res->GetNbinsY()+1; ++biny) {
            res->SetBinContent(binx, biny, f(h1->GetBinContent(binx,biny), h2->GetBinContent(binx,biny), h3->GetBinContent(binx,biny)));
        }
    }

    return res;
}

template <typename Func>
inline TH2* ApplyMany(const std::vector<const TH2*>& c, Func f) {

    for(auto h : c) {
        if(!haveSameBinning(c.front(), h)) {
            throw std::runtime_error("Incompatible X/Y-Axis");

        }
     }

    auto res = Clone(c.front(), "");

    std::vector<double> v(c.size());

    for(int binx=0; binx<=res->GetNbinsX()+1; ++binx) {
        for(int biny=0; biny<=res->GetNbinsY()+1; ++biny) {

            v.clear();
            for(auto h : c) {
                v.push_back(h->GetBinContent(binx, biny));
                }

            res->SetBinContent(binx, biny, f(v));
        }
    }

    return res;
}

}

}
