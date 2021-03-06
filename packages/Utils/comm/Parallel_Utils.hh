//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   Utils/comm/Parallel_Utils.hh
 * \author Thomas M. Evans
 * \date   Fri Jul 13 15:19:06 2007
 * \brief  Parallel utility functions.
 * \note   Copyright (C) 2014 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#ifndef Utils_comm_Parallel_Utils_hh
#define Utils_comm_Parallel_Utils_hh

namespace profugus
{

//---------------------------------------------------------------------------//
// PARALLEL EQUIVALENCE CHECKS
//---------------------------------------------------------------------------//

// Integer equivalence check.
bool check_global_equiv(int local_value);

//---------------------------------------------------------------------------//

// Floating point equivalence check.
bool check_global_equiv(double local_value, double eps = 1.0e-8);

//---------------------------------------------------------------------------//
/*!
 * \example comm/test/tstParallel_Utils.cc
 *
 * Test of parallel utilities.
 */
//---------------------------------------------------------------------------//

} // end namespace profugus

#endif // Utils_comm_Parallel_Utils_hh

//---------------------------------------------------------------------------//
//              end of comm/Parallel_Utils.hh
//---------------------------------------------------------------------------//
