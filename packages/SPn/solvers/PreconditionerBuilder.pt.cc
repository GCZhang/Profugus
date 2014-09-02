//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   solvers/PreconditionerBuilder.pt.cc
 * \author Thomas M. Evans
 * \date   Fri Feb 21 14:41:20 2014
 * \brief  PreconditionerBuilder explicit instantiation.
 * \note   Copyright (C) 2014 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#include "Epetra_Operator.h"
#include "Epetra_MultiVector.h"
#include "Tpetra_Operator.hpp"
#include "Tpetra_MultiVector.hpp"
#include "AnasaziEpetraAdapter.hpp"
#include "AnasaziTpetraAdapter.hpp"
#include "PreconditionerBuilder.t.hh"

#include "TpetraTypedefs.hh"

namespace profugus
{

template class PreconditionerBuilder<Epetra_Operator>;
template class PreconditionerBuilder<Tpetra_Operator>;

} // end namespace profugus

//---------------------------------------------------------------------------//
//                 end of PreconditionerBuilder.pt.cc
//---------------------------------------------------------------------------//