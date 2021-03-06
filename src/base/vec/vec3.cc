#include "vec3.h"

#include "TVector3.h"

using namespace ant;

vec3::vec3(const TVector3& v) noexcept :
    x(v.X()),y(v.Y()),z(v.Z())
{}

vec3& vec3::operator=(const TVector3& v) noexcept {
    x = v.X();
    y = v.Y();
    z = v.Z();
    return *this;
}

vec3::operator TVector3() const noexcept {
    return TVector3(x,y,z);
}
