//---------------------------------*-C++-*-----------------------------------//
/*!
 * \file   MC/mc/KDE_Kernel.hh
 * \author Gregory Davidson
 * \date   Mon Feb 16 14:21:15 2015
 * \brief  KDE_Kernel class declaration.
 * \note   Copyright (c) 2015 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#ifndef MC_mc_KDE_Kernel_hh
#define MC_mc_KDE_Kernel_hh

#include <map>
#include <utility>
#include <vector>

#include "utils/Definitions.hh"
#include "rng/RNG_Control.hh"
#include "geometry/Definitions.hh"
#include "Physics.hh"
#include "Particle.hh"

namespace profugus
{

//===========================================================================//
/*!
 * \class KDE_Kernel
 * \brief Base class for sampling from a KDE.
 */
/*!
 * \example mc/test/tstKDE_Kernel.cc
 *
 * Test of KDE_Kernel.
 */
//===========================================================================//
template<class Geometry>
class KDE_Kernel
{
  public:
    //@{
    //! Useful typedefs.
    typedef def::Space_Vector                          Space_Vector;
    typedef def::size_type                             size_type;
    typedef geometry::cell_type                        cell_type;
    typedef Geometry                                   Geometry_t;
    typedef std::shared_ptr<Geometry_t>                SP_Geometry;
    typedef profugus::Physics<Geometry>                Physics_t;
    typedef std::shared_ptr<Physics_t>                 SP_Physics;
    typedef typename Physics_t::Fission_Site_Container Fission_Site_Container;
    typedef std::pair<cell_type, double>               Bandwidth_Element;
    typedef std::map<cell_type, double>                Bandwidth_Map;
    //@}

  protected:
    // Stores the geometry and physics classes
    SP_Geometry b_geometry;
    SP_Physics  b_physics;

    // Stores the coefficient to use in calculating the bandwidth
    double b_coefficient;

    // Stores the exponent to use in calculating the bandwidth
    double b_exponent;

    // Stores the bandwidth on each cell
    Bandwidth_Map b_bndwidth_map;

  public:
    // Constructor.
    KDE_Kernel(SP_Geometry geometry,
               SP_Physics  physics,
               double      coefficient = 1.06,
               double      exponent    = -0.20);

    //! Get the bandwidth coefficient
    double coefficient() const { return b_coefficient; }

    //! Get the bandwidth exponent
    double exponent() const { return b_exponent; }

    //! Calculate the bandwidths
    void calc_bandwidths(const Fission_Site_Container &fis_sites);

    //! Return the bandwidth for a given cell
    double bandwidth(cell_type cellid) const;

    //! Return the bandwidths for all cells
    std::vector<double> get_bandwidths() const;

    //! Manually set the bandwidth
    void set_bandwidth(cell_type cell,
                       double    bandwidth);

    //! Sample a new position
    virtual Space_Vector sample_position(const Space_Vector &orig_position,
                                         RNG                &rng) const = 0;

    // Fraction of samples accepted inside kernel
    double acceptance_fraction() const;

  protected:
    // >>> IMPLEMENTATION FUNCTIONS
    std::vector<Space_Vector> communicate_sites(
        const Fission_Site_Container &fis_sites) const;

    // >>> IMPLEMENTATION DATA

    // Keeps track of the number of kernel samples
    mutable size_type b_num_sampled;

    // Keeps track of the number of accepted
    mutable size_type b_num_accepted;
};

} // end namespace profugus

#endif // MC_mc_KDE_Kernel_hh

//---------------------------------------------------------------------------//
// end of MC/mc/kde/KDE_Kernel.hh
//---------------------------------------------------------------------------//
