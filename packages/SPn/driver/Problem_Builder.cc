//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   driver/Problem_Builder.cc
 * \author Thomas M. Evans
 * \date   Wed Mar 12 22:25:22 2014
 * \brief  Problem_Builder member definitions.
 * \note   Copyright (C) 2014 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#include <vector>
#include <numeric>

#include "Teuchos_XMLParameterListHelpers.hpp"

#include "harness/DBC.hh"
#include "utils/Definitions.hh"
#include "xs/XS_Builder.hh"
#include "mesh/Partitioner.hh"
#include "Problem_Builder.hh"

namespace profugus
{

//---------------------------------------------------------------------------//
// CONSTRUCTOR
//---------------------------------------------------------------------------//
/*!
 * \brief Constructor.
 */
Problem_Builder::Problem_Builder()
    : d_comm(Teuchos::DefaultComm<int>::getComm())
{
    // build validator
    d_validator = Teuchos::rcp(new ParameterList("validator"));

    // build sublists
    ParameterList core, assbly, mat, mesh, problem;

    d_validator->set("CORE", core);
    d_validator->set("ASSEMBLIES", core);
    d_validator->set("MATERIAL", core);
    d_validator->set("MESH", core);
    d_validator->set("PROBLEM", core);
}

//---------------------------------------------------------------------------//
// PUBLIC INTERFACE
//---------------------------------------------------------------------------//
/*!
 * \brief Setup the problem.
 */
void Problem_Builder::setup(const std::string &xml_file)
{
    // make the master parameterlist
    auto master = Teuchos::rcp(new ParameterList);

    // read the data on every domain
    Teuchos::updateParametersFromXmlFileAndBroadcast(
        xml_file.c_str(), master.ptr(), *d_comm);

    // validate the parameter list
    Insist (master->isSublist("CORE"),
            "CORE block not defined in input.");
    Insist (master->isSublist("ASSEMBLIES"),
            "ASSEMBLIES block not defined in input.");
    Insist (master->isSublist("MATERIAL"),
            "MATERIAL block not defined in input.");
    Insist (master->isSublist("MESH"),
            "MESH block not defined in input.");
    Insist (master->isSublist("PROBLEM"),
            "PROBLEM block not defined in input.");

    // store the individual parameter lists
    d_coredb   = Teuchos::sublist(master, "CORE");
    d_assblydb = Teuchos::sublist(master, "ASSEMBLIES");
    d_matdb    = Teuchos::sublist(master, "MATERIAL");
    d_meshdb   = Teuchos::sublist(master, "MESH");
    d_db       = Teuchos::sublist(master, "PROBLEM");

    Check (!d_db.is_null());

    // build mesh
    build_mesh();

    // build the material ids on the mesh
    build_matids();

    // build material database
    build_matdb();
}

//---------------------------------------------------------------------------//
// PRIVATE IMPLEMENTATION
//---------------------------------------------------------------------------//
/*!
 * \brief Build/partition the mesh.
 */
void Problem_Builder::build_mesh()
{
    using def::I; using def::J; using def::K;

    Require (d_coredb->isParameter("axial list"));
    Require (d_coredb->isParameter("axial height"));
    Require (d_assblydb->isParameter("assembly list"));
    Require (d_assblydb->isParameter("pin pitch"));
    Require (d_meshdb->isParameter("radial mesh"));
    Require (d_meshdb->isParameter("axial mesh"));
    Require (d_meshdb->isParameter("symmetry"));

    // get the axial core map and heights
    const auto &axial_list   = d_coredb->get<OneDArray_str>("axial list");
    const auto &axial_height = d_coredb->get<OneDArray_dbl>("axial height");
    Check (!axial_list.empty());
    Check (axial_list.size() == axial_height.size());

    // build the mesh dimensions (all axial core maps have the same radial
    // dimensions, so we can just use the first core map here)
    const auto &core_map = d_coredb->get<TwoDArray_int>(axial_list[0]);

    // get the core dimensions (radially in assemblies, axially in levels);
    // remember the twoD arrays are entered [j][i] (i moves fastest in
    // COLUMN-MAJOR---FORTRAN---style, so it goes in the column index)
    int num_axial_levels = axial_list.size();
    d_Na[I]              = core_map.getNumCols();
    d_Na[J]              = core_map.getNumRows();

    // all assemblies have the same radial dimensions, so use the first one to
    // get the core dimensions
    const auto &assbly_list = d_assblydb->get<OneDArray_str>("assembly list");
    const auto &assbly_map  = d_assblydb->get<TwoDArray_int>(assbly_list[0]);

    // get pins (same caveats on ordering as for the core map)
    d_Np[I] = assbly_map.getNumCols();
    d_Np[J] = assbly_map.getNumRows();

    // get the pin pitch
    double pitch = d_assblydb->get<double>("pin pitch");
    Check (pitch > 0.0);

    // get the core dimensions
    double dx = d_Na[I] * d_Np[I] * pitch;
    double dy = d_Na[J] * d_Np[J] * pitch;
    double dz = std::accumulate(axial_height.begin(), axial_height.end(), 0.0);

    // get the mesh dimensions
    int radial_mesh        = d_meshdb->get<int>("radial mesh");
    const auto &axial_mesh = d_meshdb->get<OneDArray_int>("axial mesh");
    Check (axial_mesh.size() == axial_height.size());

    // set the mesh radial mesh dimensions
    int ncx = radial_mesh * d_Np[I] * d_Na[I];
    int ncy = radial_mesh * d_Np[J] * d_Na[J];
    d_db->set("num_cells_i", ncx);
    d_db->set("delta_x", pitch);
    d_db->set("num_cells_j", ncy);
    d_db->set("delta_y", pitch);

    // set the mesh axial dimensions
    int ncz = std::accumulate(axial_mesh.begin(), axial_mesh.end(), 0);
    OneDArray_dbl z_edges(ncz + 1, 0.0);

    // iterate through axial levels and build the mesh
    int ctr      = 0;
    double delta = 0.0;
    for (int k = 0; k < num_axial_levels; ++k)
    {
        // get the width of cells in this level
        delta = axial_height[k] / axial_mesh[k];

        // iterate through axial cells on this level
        for (int n = 0; n < axial_mesh[k]; ++n)
        {
            Check (ctr + 1 < z_edges.size());
            z_edges[ctr+1] = z_edges[ctr] + delta;
            ++ctr;
        }
    }
    Check (ctr == ncz);
    Check (profugus::soft_equiv(dz, z_edges.back(), 1.0e-12));

    // set the axial mesh
    d_db->set("z_edges", z_edges);

    // partition the mesh
    Partitioner p(d_db);
    p.build();

    // assign mesh objects
    d_mesh    = p.get_mesh();
    d_indexer = p.get_indexer();
    d_gdata   = p.get_global_data();

    Ensure (!d_mesh.is_null());
    Ensure (!d_indexer.is_null());
    Ensure (!d_gdata.is_null());
}

//---------------------------------------------------------------------------//
/*!
 * \brief Build the material ids.
 */
void Problem_Builder::build_matids()
{
    using def::I; using def::J; using def::K;

    // size the matids array
    d_matids.resize(d_mesh->num_cells());

    // k-mesh levels for each axial level
    int k_begin = 0, k_end = 0;

    // global radial core map
    TwoDArray_int axial_matids(d_gdata->num_cells(J), d_gdata->num_cells(I), 0);

    // get number of cells per axial level
    const auto &axial_mesh = d_meshdb->get<OneDArray_int>("axial mesh");

    // process the axial levels one at a time
    for (int level = 0; level < axial_mesh.size(); ++level)
    {
        // calculate the global matids in this level
        calc_axial_matids(level, axial_matids);

        // determine the begin/end of the axial mesh for this axial level
        k_begin = k_end;
        k_end   = k_begin + axial_mesh[level];
        Check (k_end - k_begin == axial_mesh[level]);

        // loop over local cells
        for (int k = k_begin; k < k_end; ++k)
        {
            for (int j = 0; j < d_mesh->num_cells_dim(J); ++j)
            {
                for (int i = 0; i < d_mesh->num_cells_dim(I); ++i)
                {
                    // get the global IJ indices
                    auto global = d_indexer->convert_to_global(i, j);
                    Check (global[I] < axial_matids.getNumCols());
                    Check (global[J] < axial_matids.getNumRows());

                    // assign the local matid
                    d_matids[d_indexer->l2l(i, j, k)] =
                        axial_matids(global[J], global[I]);
                }
            }
        }
    }
}

//---------------------------------------------------------------------------//
/*!
 * \brief Calculate material ids in an axial level.
 */
void Problem_Builder::calc_axial_matids(int            level,
                                        TwoDArray_int &matids)
{
    using def::I; using def::J;

    // (x, y) global offsets into the mesh by assembly and pin
    int aoff_x = 0, poff_x = 0;
    int aoff_y = 0, poff_y = 0;

    // get the list of core maps and assembly types
    const auto &axial_list  = d_coredb->get<OneDArray_str>("axial list");
    const auto &assbly_list = d_assblydb->get<OneDArray_str>("assembly list");

    // get the core-map for this axial level
    const auto &core_map = d_coredb->get<TwoDArray_int>(axial_list[level]);
    Check (core_map.getNumCols() == d_Na[I]);
    Check (core_map.getNumRows() == d_Na[J]);

    // mesh cells per pin
    int radial_mesh = d_meshdb->get<int>("radial mesh");
    Check (matids.getNumCols() == d_Na[I] * d_Np[I] * radial_mesh);
    Check (matids.getNumRows() == d_Na[J] * d_Np[J] * radial_mesh);

    // loop over all assemblies, get the pin-maps, and assign the material ids
    // to the matids array (remember, all "core arrays" are ordered
    // COLUMN-MAJOR, which means matids[j, i])

    // material id of a pin
    int matid = 0;

    // loop over assemblies in J
    for (int aj = 0; aj < d_Na[J]; ++aj)
    {
        // set the y-offset for this assembly
        aoff_y = (radial_mesh * d_Np[J]) * aj;

        // loop over assemblies in I
        for (int ai = 0; ai < d_Na[I]; ++ai)
        {
            Check (core_map(aj, ai) < assbly_list.size());
            Check (d_assblydb->isParameter(assbly_list[core_map(aj, ai)]));

            // get the pin-map for this assembly
            const auto &assbly_map = d_assblydb->get<TwoDArray_int>(
                assbly_list[core_map(aj, ai)]);
            Check (assbly_map.getNumCols() == d_Np[I]);
            Check (assbly_map.getNumRows() == d_Np[J]);

            // set the x-offset for this assembly
            aoff_x = (radial_mesh * d_Np[I]) * ai;

            // loop over pins in J
            for (int pj = 0; pj < d_Np[J]; ++pj)
            {
                // set the y-offset for this pin
                poff_y = aoff_y + radial_mesh * pj;

                for (int pi = 0; pi < d_Np[I]; ++pi)
                {
                    // set the x-offset for this pin
                    poff_x = aoff_x + radial_mesh * pi;

                    // get the material id for this pin
                    matid = assbly_map(pj, pi);
                    Check (matid <
                           d_matdb->get<OneDArray_str>("mat list").size());

                    // loop over the mesh cells in this pin
                    for (int j = 0; j < radial_mesh; ++j)
                    {
                        for (int i = 0; i < radial_mesh; ++i)
                        {
                            Check (i + poff_x < matids.getNumCols());
                            Check (j + poff_y < matids.getNumRows());
                            matids(j + poff_y, i + poff_x) = matid;
                        }
                    }
                }
            }
        }
    }
}

//---------------------------------------------------------------------------//
/*!
 * \brief Build the material data.
 *
 * For now, we build all the cross sections in the problem on every domain.
 * For the mini-app, this is not expected to be an overburdening cost.
 */
void Problem_Builder::build_matdb()
{
    typedef XS_Builder::Matid_Map Matid_Map;
    typedef XS_Builder::RCP_XS    RCP_XS;

    Require (d_matdb->isParameter("mat list"));
    Validate (d_matdb->isParameter("xs library"),
              "Inline cross sections not implemented yet.");

    // get the material list off of the database
    const auto &mat_list = d_matdb->get<OneDArray_str>("mat list");

    // convert the matlist to a mat-id map
    Matid_Map matids;
    for (int id = 0, N = mat_list.size(); id < N; ++id)
    {
        matids.insert(Matid_Map::value_type(id, mat_list[id]));
    }
    matids.complete();
    Check (matids.size() == mat_list.size());

    // make a cross section builder
    XS_Builder builder;

    // broadcast the raw cross section data
    builder.open_and_broadcast(d_matdb->get<std::string>("xs library"));

    // get the number of groups and moments in the cross section data
    int Ng_data = builder.num_groups();
    int N_data  = builder.pn_order();

    // determine the moment order of the problem
    int pn_order = d_db->get("Pn_order", N_data);
    Validate (pn_order <= N_data, "Requested Pn scattering order of "
              << pn_order << " is greater than available data Pn order of "
              << N_data);

    // get the number of groups required
    int g_first = d_db->get("g_first", 0);
    int g_last  = d_db->get("g_last", Ng_data - 1);
    Validate (1 + (g_last - g_first) <= Ng_data, "Energy group range exceeds "
              << "number of groups in data, 1 + g_last - g_first = "
              << 1 + (g_last - g_first) << " > " << Ng_data);

    // build the cross sections
    builder.build(matids, pn_order, g_first, g_last);
    RCP_XS xs = builder.get_xs();
    Check (xs->num_mat() == matids.size());
    Check (xs->num_groups() == 1 + (g_last - g_first));

    // build the material database
    d_mat = Teuchos::rcp(new Mat_DB);

    // set the cross sections
    d_mat->set(xs, d_mesh->num_cells());

    // set the matids in the material database
    d_mat->assign(d_matids);
}

} // end namespace profugus

//---------------------------------------------------------------------------//
//                 end of Problem_Builder.cc
//---------------------------------------------------------------------------//
