//---------------------------------*-C++-*-----------------------------------//
/*!
 * \file   acc/test/tstPhysics.cc
 * \author Seth R Johnson
 * \date   Wed Oct 29 13:35:43 2014
 * \brief  tstPhysics class definitions.
 * \note   Copyright (c) 2014 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#include "../Physics.hh"
#include "xs/XS.hh"
#include "gtest/utils_gtest.hh"

#include <vector>
#include <memory>

#include "utils/Definitions.hh"

using namespace std;

using def::X; using def::Y; using def::Z;

//---------------------------------------------------------------------------//
// Test fixture
//---------------------------------------------------------------------------//

class PhysicsTest : public testing::Test
{
  protected:
    typedef acc::Physics       Physics;
    typedef profugus::XS       XS;

  protected:

    void SetUp()
    {
        build_xs();
    }

    void build_xs()
    {
        xs.set(0, 5);

        vector<double> bnd(6, 0.0);
        bnd[0] = 100.0;
        bnd[1] = 10.0;
        bnd[2] = 1.0;
        bnd[3] = 0.1;
        bnd[4] = 0.01;
        bnd[5] = 0.001;
        xs.set_bounds(bnd);

        XS::OneDArray total(5);
        XS::TwoDArray scat(5, 5);

        double f[5] = {0.1, 0.4, 1.8, 5.7, 9.8};
        double c[5] = {0.3770, 0.4421, 0.1809, 0.0, 0.0};
        double n[5] = {2.4*f[0], 2.4*f[1], 2.4*f[2], 2.4*f[3], 2.4*f[4]};
        XS::OneDArray fission(begin(f), end(f));
        XS::OneDArray chi(begin(c), end(c));
        XS::OneDArray nus(begin(n), end(n));
        xs.add(1, XS::SIG_F, fission);
        xs.add(1, XS::NU_SIG_F, nus);
        xs.add(1, XS::CHI, chi);

        // mat 0
        total[0] = 5.2 ;
        total[1] = 11.4;
        total[2] = 18.2;
        total[3] = 29.9;
        total[4] = 27.3;
        xs.add(0, XS::TOTAL, total);

        // mat 1
        total[0] = 5.2  + f[0];
        total[1] = 11.4 + f[1];
        total[2] = 18.2 + f[2];
        total[3] = 29.9 + f[3];
        total[4] = 27.3 + f[4];
        xs.add(1, XS::TOTAL, total);

        scat(0, 0) = 1.2;
        scat(1, 0) = 0.9;
        scat(1, 1) = 3.2;
        scat(2, 0) = 0.4;
        scat(2, 1) = 2.8;
        scat(2, 2) = 6.9;
        scat(2, 3) = 1.5;
        scat(3, 0) = 0.1;
        scat(3, 1) = 2.1;
        scat(3, 2) = 5.5;
        scat(3, 3) = 9.7;
        scat(3, 4) = 2.1;
        scat(4, 1) = 0.2;
        scat(4, 2) = 1.3;
        scat(4, 3) = 6.6;
        scat(4, 4) = 9.9;
        xs.add(0, 0, scat);
        xs.add(1, 0, scat);

        xs.complete();
    }

  protected:
    XS xs;
};

//---------------------------------------------------------------------------//
// TESTS
//---------------------------------------------------------------------------//
TEST_F(PhysicsTest, Access)
{
    // Make the physics
    Physics physics(xs);

    // Check the fissionable data
    EXPECT_FALSE(physics.is_fissionable(0));
    EXPECT_TRUE(physics.is_fissionable(1));

    // test data access without fission
    {
        int g = 0;
        EXPECT_SOFTEQ(5.2, physics.total(0, g), 1.e-12);
        EXPECT_SOFTEQ(2.6 / 5.2, physics.scattering_ratio(0, g), 1.e-12);
        EXPECT_SOFTEQ(0.0, physics.nusigf(0, g), 1.e-12);

        g = 1;
        EXPECT_SOFTEQ(11.4, physics.total(0, g), 1.e-12);
        EXPECT_SOFTEQ(8.3 / 11.4, physics.scattering_ratio(0, g), 1.e-12);
        EXPECT_SOFTEQ(0.0, physics.nusigf(0, g), 1.e-12);

        g = 2;
        EXPECT_SOFTEQ(18.2, physics.total(0, g), 1.e-12);
        EXPECT_SOFTEQ(13.7 / 18.2, physics.scattering_ratio(0, g), 1.e-12);
        EXPECT_SOFTEQ(0.0, physics.nusigf(0, g), 1.e-12);

        g = 3;
        EXPECT_SOFTEQ(29.9, physics.total(0, g), 1.e-12);
        EXPECT_SOFTEQ(17.8 / 29.9, physics.scattering_ratio(0, g), 1.e-12);
        EXPECT_SOFTEQ(0.0, physics.nusigf(0, g), 1.e-12);

        g = 4;
        EXPECT_SOFTEQ(27.3, physics.total(0, g), 1.e-12);
        EXPECT_SOFTEQ(12.0 / 27.3, physics.scattering_ratio(0, g), 1.e-12);
        EXPECT_SOFTEQ(0.0, physics.nusigf(0, g), 1.e-12);

    }
    
    // test data access with fission
    {
        int g = 0;
        EXPECT_SOFTEQ(5.3, physics.total(1, g), 1.e-12);
        EXPECT_SOFTEQ(2.6 / 5.3, physics.scattering_ratio(1, g), 1.e-12);
        EXPECT_SOFTEQ(0.24, physics.nusigf(1, g), 1.e-12);

        g = 1;
        EXPECT_SOFTEQ(11.8, physics.total(1, g), 1.e-12);
        EXPECT_SOFTEQ(8.3 / 11.8, physics.scattering_ratio(1, g), 1.e-12);
        EXPECT_SOFTEQ(0.96, physics.nusigf(1, g), 1.e-12);

        g = 2;
        EXPECT_SOFTEQ(20.0, physics.total(1, g), 1.e-12);
        EXPECT_SOFTEQ(13.7 / 20.0, physics.scattering_ratio(1, g), 1.e-12);
        EXPECT_SOFTEQ(4.32, physics.nusigf(1, g), 1.e-12);

        g = 3;
        EXPECT_SOFTEQ(35.6, physics.total(1, g), 1.e-12);
        EXPECT_SOFTEQ(17.8 / 35.6, physics.scattering_ratio(1, g), 1.e-12);
        EXPECT_SOFTEQ(13.68, physics.nusigf(1, g), 1.e-12);

        g = 4;
        EXPECT_SOFTEQ(37.1, physics.total(1, g), 1.e-12);
        EXPECT_SOFTEQ(12.0 / 37.1, physics.scattering_ratio(1, g), 1.e-12);
        EXPECT_SOFTEQ(23.52, physics.nusigf(1, g), 1.e-12);
    }
}

//---------------------------------------------------------------------------//
// end of acc/test/tstPhysics.cc
//---------------------------------------------------------------------------//