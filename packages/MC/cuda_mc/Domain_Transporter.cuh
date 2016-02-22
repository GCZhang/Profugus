//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   cuda_mc/Domain_Transporter.cuh
 * \author Thomas M. Evans
 * \date   Mon May 12 12:02:13 2014
 * \brief  Domain_Transporter class definition.
 * \note   Copyright (C) 2014 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#ifndef cuda_mc_Domain_Transporter_cuh
#define cuda_mc_Domain_Transporter_cuh

#include "Particle.hh"
#include "Physics.hh"
#include "Step_Selector.cuh"
//#include "Variance_Reduction.hh"
//#include "Tallier.hh"

namespace cuda_mc
{

//===========================================================================//
/*!
 * \class Domain_Transporter
 * \brief Transport a particle on a computational domain.
 *
 * This class does no communication; it takes a particle and transports it
 * until it leaves the domain.
 */
/*!
 * \example mc/test/tstDomain_Transporter.cc
 *
 * Test of Domain_Transporter.
 */
//===========================================================================//

template <class Geometry>
class Domain_Transporter
{
  public:
    //@{
    //! Useful typedefs.
    typedef Geometry                                   Geometry_t;
    typedef Physics<Geometry_t>                        Physics_t;
    typedef typename Geometry_t::Space_Vector          Space_Vector;
    typedef typename Geometry_t::Geo_State_t           Geo_State_t;
    typedef typename Physics_t::Particle_t             Particle_t;
    //typedef typename Physics_t::Bank_t                 Bank_t;
    //typedef typename Physics_t::Fission_Site_Container Fission_Site_Container;
    //typedef Variance_Reduction<Geometry_t>             Variance_Reduction_t;
    //typedef Tallier<Geometry_t>                        Tallier_t;
    //@}

    //@{
    //! Smart pointers.
    typedef cuda::Shared_Device_Ptr<Geometry_t>       SDP_Geometry;
    typedef cuda::Shared_Device_Ptr<Physics_t>        SDP_Physics;
    typedef cuda::Shared_Device_Ptr<Particle_t>       SDP_Particle;
    //typedef std::shared_ptr<Fission_Site_Container> SP_Fission_Sites;
    //typedef std::shared_ptr<Variance_Reduction_t>   SP_Variance_Reduction;
    //typedef std::shared_ptr<Tallier_t>              SP_Tallier;
    //@}

  private:
    // >>> DATA

    // Problem geometry implementation.
    SDP_Geometry d_geometry_host;
    Geometry    *d_geometry;

    // Problem physics implementation.
    SDP_Physics d_physics_host;
    Physics_t  *d_physics;

    // Variance reduction.
    //SP_Variance_Reduction d_var_reduction;

    // Regular tallies.
    //SP_Tallier d_tallier;

    // Fission sites.
    //SP_Fission_Sites d_fission_sites;

  public:
    // Constructor.
    Domain_Transporter();

    // Set the geometry and physics classes.
    void set(SDP_Geometry geometry, SDP_Physics physics);

    // Set the variance reduction.
    //void set(SP_Variance_Reduction reduction);

    // Set regular tallies.
    //void set(SP_Tallier tallies);

    // Set fission site sampling.
    //void set(SP_Fission_Sites fission_sites, double keff);

    // Transport a particle through the domain.
    __device__ void transport(Particle_t &particle);

    //! Return the number of sampled fission sites.
    __host__ __device__
    int num_sampled_fission_sites() const { return d_num_fission_sites; }

  private:
    // >>> IMPLEMENTATION

    // Step selector.
    Step_Selector d_step;

    // Tracking distances.
    double d_dist_mfp, d_dist_bnd, d_dist_col;

    // Total cross section in region.
    double d_xs_tot;

    // Flag indicating that fission sites should be sampled.
    bool d_sample_fission_sites;

    // Number of fission sites sampled.
    int d_num_fission_sites;

    // Current keff iterate.
    double d_keff;

    // Process collisions and boundaries.
    __device__ void process_boundary(Particle_t &particle);
    __device__ void process_collision(Particle_t &particle);
};

} // end namespace cuda_mc 

#include "Domain_Transporter.i.cuh"

#endif // cuda_mc_Domain_Transporter_cuh


//---------------------------------------------------------------------------//
//                 end of Domain_Transporter.cuh
//---------------------------------------------------------------------------//
