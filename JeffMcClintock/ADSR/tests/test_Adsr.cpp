#define CATCH_CONFIG_MAIN
#include "../../../catch2/catch.hpp"

#include "../Adsr.h"

TEST_CASE("Curve parameters are computed", "[CalculateCurve]")
{
    const double sampleRate = 44100;

    auto result = CalculateCurve(0.0, sampleRate, 6.5, 1.0, 0.0);
    CHECK(std::get<0>(result) == 0.0);
    CHECK(std::get<1>(result) == Approx(2.2671e-13) );
    CHECK(std::get<2>(result) == Approx(1000.5) );
}

TEST_CASE("Sustain parameters are computed", "[CalculateSustain]")
{
    const double sampleRate = 44100;

    auto [legalLow, curveRate, CurveTarget] = CalculateSteadyState(0.5);

    CHECK(legalLow == -0.5);
    CHECK(curveRate == 0.0);
    CHECK(CurveTarget == 0.0);
}