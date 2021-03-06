//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   SPn/spn/Energy_Multigrid.t.hh
 * \author Thomas M. Evans, Steven Hamilton
 * \date   Tue Feb 25 13:04:00 2014
 * \brief  Energy_Multigrid template member definitions.
 * \note   Copyright (C) 2014 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#ifndef SPn_spn_Energy_Multigrid_t_hh
#define SPn_spn_Energy_Multigrid_t_hh

#include "solvers/PreconditionerBuilder.hh"
#include "solvers/LinAlgTypedefs.hh"
#include "xs/Energy_Collapse.hh"
#include "Linear_System_FV.hh"
#include "Energy_Multigrid.hh"
#include "VectorTraits.hh"

namespace profugus
{

//---------------------------------------------------------------------------//
// CONSTRUCTOR
//---------------------------------------------------------------------------//

template <class T>
Energy_Multigrid<T>::Energy_Multigrid(RCP_ParameterList              main_db,
                                      RCP_ParameterList              prec_db,
                                      Teuchos::RCP<Dimensions>       dim,
                                      Teuchos::RCP<Mat_DB>           mat_db,
                                      Teuchos::RCP<Mesh>             mesh,
                                      Teuchos::RCP<LG_Indexer>       indexer,
                                      Teuchos::RCP<Global_Mesh_Data> data,
                                      Teuchos::RCP<Linear_System<T> >
                                          fine_system)
    : OperatorAdapter<T>(fine_system->get_Map())
{
    using Teuchos::RCP;
    using Teuchos::rcp;

    // get parameters for preconditioner
    int coarse_factor = prec_db->get("Coarse Factor", 2);
    int max_depth     = prec_db->get("Max Depth", 10);
    int fine_groups   = mat_db->xs().num_groups();

    // old and new groups
    int old_groups = 0, new_groups = fine_groups;

    // material databases for preconditioner
    Teuchos::RCP<Mat_DB> old_mat, new_mat = mat_db;

    // Fill vectors with fine level objects, don't build new matrix
    d_operators.push_back(fine_system->get_Operator());
    d_maps.push_back( fine_system->get_Map() );
    d_solutions.push_back( VectorTraits<T>::build_vector(d_maps[0]) );
    d_residuals.push_back( VectorTraits<T>::build_vector(d_maps[0]) );
    d_rhss.push_back( VectorTraits<T>::build_vector(d_maps[0]) );

    // Build level 0 smoother
    RCP_ParameterList smoother_db = sublist(prec_db, "Smoother");
    d_smoothers.push_back(
        LinearSolverBuilder<T>::build_solver(smoother_db));
    d_smoothers.back()->set_operator(d_operators.back());
    d_preconditioners.push_back(PreconditionerBuilder<T>::
        build_preconditioner(fine_system->get_Matrix(),smoother_db) );
    if (d_preconditioners.back() != Teuchos::null)
    {
        d_smoothers.back()->set_preconditioner(d_preconditioners.back());
    }
    else
    {
        d_smoothers.back()->set_preconditioner(fine_system->get_Matrix());
    }

    // loop through levels
    int level = 0;
    do
    {
        level++;

        // Determine number of groups at next level
        old_groups = new_groups;
        new_groups = old_groups / coarse_factor;
        std::vector<int> collapse(new_groups,coarse_factor);
        int extra_grps = old_groups%coarse_factor;
        if( extra_grps > 0 )
        {
            new_groups++;
            collapse.push_back(extra_grps);
        }

        // Create new Mat_DB
        std::vector<double> weights(old_groups,1.0);
        old_mat = new_mat;
        new_mat = Energy_Collapse::collapse_all_mats(
            old_mat, collapse, weights);
        CHECK( !new_mat.is_null() );

        // Build linear system
        RCP<Linear_System<T> > system = rcp(
            new Linear_System_FV<T>(
                main_db, dim, new_mat, mesh, indexer, data));

        system->build_Matrix();
        d_operators.push_back( system->get_Operator() );
        CHECK( d_operators.back() != Teuchos::null );

        // Allocate vectors
        RCP<MV> tmp_vec = system->get_RHS();
        d_maps.push_back( system->get_Map() );
        d_solutions.push_back( VectorTraits<T>::build_vector(d_maps[level]));
        d_rhss.push_back(      VectorTraits<T>::build_vector(d_maps[level]));
        d_residuals.push_back( VectorTraits<T>::build_vector(d_maps[level]));

        // Build Restriction
        d_restrictions.push_back(
            rcp(new Energy_Restriction<T>(d_maps[level-1],
                                          d_maps[level],
                                          collapse)));

        // Build Prolongation
        d_prolongations.push_back(
            rcp(new Energy_Prolongation<T>(d_maps[level],
                                           d_maps[level-1],
                                           collapse)));

        // Build smoother
        d_smoothers.push_back(
            LinearSolverBuilder<T>::build_solver(smoother_db));
        d_smoothers.back()->set_operator(d_operators.back());

        // Store and set preconditioner
        d_preconditioners.push_back(PreconditionerBuilder<T>::
            build_preconditioner(system->get_Matrix(),smoother_db) );
        if( d_preconditioners.back() != Teuchos::null )
        {
            d_smoothers.back()->set_preconditioner(d_preconditioners.back());
        }
        else
        {
            d_smoothers.back()->set_preconditioner(system->get_Matrix());
        }

    } while( new_groups!=1 && level!=max_depth );

    d_num_levels = level+1;

    // By default, the coarse grid solve is the same as the smoothers
    // If requested, we can replace this with something different
    if (prec_db->isSublist("Coarse Solver"))
    {
        // get the sublist
        RCP_ParameterList coarse_db = sublist(prec_db, "Coarse Solver");

        // Replace last smoother, don't add a new one
        d_smoothers.push_back(
            profugus::LinearSolverBuilder<T>::build_solver(coarse_db));
        d_smoothers.back()->set_operator(d_operators.back());
        if( d_preconditioners.back() != Teuchos::null )
        {
            d_smoothers.back()->set_preconditioner(d_preconditioners.back());
        }
        else
        {
            d_smoothers.back()->set_preconditioner(d_operators.back());
        }
    }
}

//---------------------------------------------------------------------------//
// APPLY MULTIGRID V-CYCLE
//---------------------------------------------------------------------------//

template <class T>
void Energy_Multigrid<T>::ApplyImpl(const MV &x,
                                          MV &y ) const
{
    int num_vectors = MVT::GetNumberVecs(x);
    REQUIRE(MVT::GetNumberVecs(y) == num_vectors);

    // Process each vector in multivec individually (all of the
    //  multivecs have been allocated for a single vec)
    for( int ivec=0; ivec<num_vectors; ++ivec )
    {
        std::vector<int> ind(1,ivec);
        Teuchos::RCP<const MV> xi = MVT::CloneView(x,ind);
        MVT::Assign(*xi,*d_residuals[0]);
        MVT::Assign(*xi,*d_rhss[0]);
        MVT::MvInit(*d_solutions[0],0.0);

        // In a true multigrid V-cycle, the first operation is a
        //  restriction rather than smoothing.  Smoothing on the finest
        //  level is only done at the end of the cycle.  This way if two
        //  V-cycles are stacked back-to-back, only a single smoothing
        //  step is done in the middle.

        for( int ilevel=1; ilevel<d_num_levels; ++ilevel )
        {
            // Restrict residual from previous level
            OPT::Apply(*d_restrictions[ilevel-1],
                       *d_residuals[ilevel-1],
                       *d_rhss[ilevel]);

            // Apply smoother
            MVT::MvInit(*d_solutions[ilevel],0.0);
            d_smoothers[ilevel]->solve(d_solutions[ilevel],d_rhss[ilevel]);

            // Compute residual (except on coarsest level)
            if( ilevel != d_num_levels-1 )
            {
                OPT::Apply(*d_operators[ilevel],
                           *d_solutions[ilevel],
                           *d_residuals[ilevel]);

                MVT::MvAddMv(1.0,*d_rhss[ilevel],-1.0,*d_residuals[ilevel],
                             *d_residuals[ilevel]);
            }
        }

        for( int ilevel=d_num_levels-2; ilevel>=0; --ilevel )
        {
            // Prolong solution vector to next level: x[l] = x[l] + P*x[l-1]
            // Residual is used for tmp storage here
            OPT::Apply(*d_prolongations[ilevel],*d_solutions[ilevel+1],
                       *d_residuals[ilevel]);
            MVT::MvAddMv(1.0,*d_residuals[ilevel],1.0,*d_solutions[ilevel],
                         *d_solutions[ilevel]);

            // Apply smoother
            d_smoothers[ilevel]->solve(d_solutions[ilevel],d_rhss[ilevel]);
        }

        MVT::SetBlock(*d_solutions[0],ind,y);
    }
}

} // end namespace profugus

#endif // SPn_spn_Energy_Multigrid_t_hh

//---------------------------------------------------------------------------//
//                 end of Energy_Multigrid.t.hh
//---------------------------------------------------------------------------//
