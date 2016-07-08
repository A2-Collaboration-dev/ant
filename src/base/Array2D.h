#include <vector>

class TH2D;

namespace ant {

struct Array2DBase {

    virtual void   Set(const unsigned x, const unsigned y, const double v) =0;
    virtual double Get(const unsigned x, const unsigned y) const =0;

    virtual unsigned Width() const =0;
    virtual unsigned Height() const =0;
    virtual std::size_t Size() const =0;

    virtual ~Array2DBase();

};

struct Array2D : Array2DBase {

protected:

    unsigned width;
    unsigned height;
    std::vector<double> data;
    std::size_t Bin(const unsigned x, const unsigned y) const noexcept { return x + y*width; }

public:

    Array2D(const unsigned w, const unsigned h, const double default_value=0.0):
        width(w), height(h), data(w*h, default_value) {}
    virtual ~Array2D();

          double& at(const unsigned x, const unsigned y);
    const double& at(const unsigned x, const unsigned y) const;

    virtual void   Set(const unsigned x, const unsigned y, const double v) override;
    virtual double Get(const unsigned x, const unsigned y) const override;

    virtual unsigned Width() const override { return width; }
    virtual unsigned Height() const override { return height; }


    std::size_t Size() const override { return data.size(); }

    const std::vector<double>& Data() const noexcept { return data; }
};

class Array2D_TH2D : public Array2DBase {
protected:
    TH2D* hist = nullptr;

public:
    Array2D_TH2D(TH2D* h): hist(h) {}
    virtual ~Array2D_TH2D();
    virtual void   Set(const unsigned x, const unsigned y, const double v) override;
    virtual double Get(const unsigned x, const unsigned y) const override;
    unsigned Width() const override;
    unsigned Height() const override;
    std::size_t Size() const override;
};

static void CopyRect(const ant::Array2DBase& src, ant::Array2DBase& dst, const unsigned x, const unsigned y);

}
