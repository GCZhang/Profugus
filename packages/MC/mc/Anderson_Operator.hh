//---------------------------------*-C++-*-----------------------------------//
/*!
 * \file   MC/mc/Anderson_Operator.hh
 * \author Thomas M. Evans
 * \date   Tue Apr 07 21:05:56 2015
 * \brief  Anderson_Operator class declaration.
 * \note   Copyright (c) 2015 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#ifndef MC_mc_Anderson_Operator_hh
#define MC_mc_Anderson_Operator_hh

#include <memory>

#include "Teuchos_RCP.hpp"
#include "Teuchos_ParameterList.hpp"
#include "Teuchos_ArrayView.hpp"
#include "Teuchos_BLAS.hpp"

#include "harness/DBC.hh"
#include "comm/global.hh"
#include "spn/OperatorAdapter.hh"
#include "geometry/Cartesian_Mesh.hh"
#include "Source_Transporter.hh"
#include "Fission_Source.hh"
#include "Tallier.hh"
#include "Keff_Tally.hh"
#include "Fission_Tally.hh"

namespace profugus
{

//===========================================================================//
/*!
 * \class Anderson_Operator
 * \brief Operator for solving eigenvalue problem using Anderson.
 */
/*!
 * \example mc/test/tstAnderson_Operator.cc
 *
 * Test of Anderson_Operator.
 */
//===========================================================================//

template<class Geometry, class T>
class Anderson_Operator : public OperatorAdapter<T>
{
    typedef OperatorAdapter<T> Base;

public:
    typedef Geometry                                Geometry_t;
    typedef Source_Transporter<Geometry_t>          Source_Transporter_t;
    typedef std::shared_ptr<Source_Transporter_t>   SP_Source_Transporter;
    typedef Fission_Source<Geometry_t>              FS_t;
    typedef Keff_Tally<Geometry_t>                  Keff_Tally_t;
    typedef Fission_Tally<Geometry_t>               Fission_Tally_t;
    typedef std::shared_ptr<FS_t>                   SP_Fission_Source;
    typedef typename FS_t::SP_Fission_Sites         SP_Fission_Sites;
    typedef Tallier<Geometry_t>                     Tallier_t;
    typedef std::shared_ptr<Tallier_t>              SP_Tallier;
    typedef typename T::MV                          MV;
    typedef typename T::MAP                         MAP;
    typedef Teuchos::RCP<MAP>                       RCP_MAP;
    typedef Teuchos::RCP<MV>                        RCP_MV;
    typedef Teuchos::RCP<Teuchos::ParameterList>    RCP_PL;
    typedef std::shared_ptr<Cartesian_Mesh>         SP_Cart_Mesh;
    typedef std::shared_ptr<Keff_Tally_t>           SP_Keff_Tally;
    typedef std::shared_ptr<Fission_Tally_t>        SP_Fission_Tally;

  private:
    // >>> DATA

    // Database
    RCP_PL d_pl;

    // Source transporter.
    SP_Source_Transporter d_transporter;

    // Fission source.
    SP_Fission_Source        d_source;
    mutable SP_Fission_Sites d_fission_sites;

    // Tallier
    SP_Tallier d_tallier;

    // Keff Tally
    SP_Keff_Tally d_keff_tally;

    // Fission Tally
    SP_Fission_Tally d_fisn_tally;

    // Global eigenvalue mesh
    SP_Cart_Mesh d_mesh;

  public:
    // Constructor.
    Anderson_Operator(RCP_PL pl, SP_Source_Transporter transporter,
                      SP_Fission_Source fission_source, SP_Cart_Mesh mesh,
                      RCP_MAP map, profugus::Communicator_t set_comm);

    // Set the tallier.
    void set_tallier(SP_Tallier t)
    {
        REQUIRE(t);
        d_tallier = t;
        d_transporter->set(d_tallier);
    }

    // Do a transport iteration.
    void iterate(double k) const;

    // Update the fission source with the latest fission site bank.
    void update_source() const;

    // Call before first iterate
    void build_tallies();

    // Call at beginning of Anderson solve.
    RCP_MV initialize_Anderson();

    // Call after Anderson solve.
    double finalize_Anderson(const MV &v);

    // >>> PUBLIC INTERFACE FOR OPERATOR

    // Apply.
    void ApplyImpl(const MV &x, MV &y) const;

    // >>> ACCESSORS

    //! Get the source.
    SP_Fission_Source source() const { return d_source; }

  private:
    // >>> IMPLEMENTATION

    typedef Teuchos::ArrayView<double>            View;
    typedef Teuchos::ArrayView<const double>      const_View;
    typedef typename FS_t::Fission_Site_Container Fission_Site_Container;

    // Number of particles per cycle (constant weight).
    double d_Np;

    // Last g vector.
    RCP_MV d_gp;

    // Node and nodes
    int d_nodes, d_node;

    // Have tallies been built
    bool d_tallies_built;

    // Use fission tally for eigenvector
    bool d_use_tally;

    // Set-constant communicator.
    profugus::Communicator_t d_set_comm;

    // BLAS interface.
    Teuchos::BLAS<int, double> d_blas;

    // Prolongation, P: g -> f.
    void prolongate(const_View g, Fission_Site_Container &f) const;

    // Restriction, Rf = g.
    void restrict(const Fission_Site_Container &f, View g) const;
};

} // end namespace profugus

#endif // MC_mc_Anderson_Operator_hh

//---------------------------------------------------------------------------//
// end of MC/mc/Anderson_Operator.hh
//---------------------------------------------------------------------------//
