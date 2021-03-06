//----------------------------------*-C++-*----------------------------------//
/*!
 * \file   MC/mc/Fission_Matrix_Processor.cc
 * \author Thomas M. Evans
 * \date   Fri Sep 12 11:08:38 2014
 * \brief  Fission_Matrix_Processor member definitions.
 * \note   Copyright (C) 2014 Oak Ridge National Laboratory, UT-Battelle, LLC.
 */
//---------------------------------------------------------------------------//

#include "Fission_Matrix_Processor.hh"

#include "comm/global.hh"

namespace profugus
{

//---------------------------------------------------------------------------//
// CONSTRUCTOR
//---------------------------------------------------------------------------//
/*!
 * \brief Constructor.
 */
Fission_Matrix_Processor::Fission_Matrix_Processor()
    : d_type(INTERNAL)
    , d_parent(NONE)
    , d_children(2, NONE)
    , d_node(profugus::node())
    , d_nodes(profugus::nodes())
{
    // calculate the children and parents
    d_parent      = (d_node - 1) / 2;
    d_children[1] = (d_node + 1) * 2;
    d_children[0] = d_children[1] - 1;

    // node 0 is the root
    if (d_node == 0)
        d_parent = NONE;

    // check children
    if (d_children[0] > d_nodes - 1)
    {
        d_children = NONE;
        d_type     = EXTERNAL;
    }
    else if (d_children[1] > d_nodes - 1)
    {
        d_children[1] = NONE;
    }
}

//---------------------------------------------------------------------------//
// PUBLIC FUNCTIONS
//---------------------------------------------------------------------------//
/*!
 * \brief Globally reduce and build the fission matrix.
 *
 * This builds a flattened fission matrix.  The order is stored in the graph.
 */
void Fission_Matrix_Processor::build_matrix(
    const Sparse_Matrix &local_matrix,
    const Denominator   &local_denominator)
{
    REQUIRE(!local_matrix.empty());
    REQUIRE(local_matrix.size() <= local_denominator.size() *
            local_denominator.size());

    // reset the internal storage
    reset();

    // the NxN fission matrix size is determined by the size of the
    // Denominator, which equals N
    d_N = local_denominator.size();

    // on every domain, write the local matrix into an ordered graph
    Ordered_Graph local_graph(local_matrix.size());
    auto itr = local_graph.begin();
    for (const auto &element : local_matrix)
    {
        *itr = element.first;
        ++itr;
    }

    // sort the local graph
    std::sort(local_graph.begin(), local_graph.end());

    // initialize the global graph on this domain with the local graph
    d_graph.insert(d_graph.end(), local_graph.begin(), local_graph.end());
    CHECK(d_graph.size() == local_graph.size());

    // do a parallel merge/sort on the global graph
    reduce();

    // broadcast the graph
    int size = d_graph.size();
    profugus::broadcast(&size, 1, 0);

    // resize the graph on the work nodes
    if (d_node > 0)
    {
        Ordered_Graph g(size);
        std::swap(d_graph, g);
    }
    CHECK(d_graph.size() == size);

    // broadcast the graph
    profugus::broadcast(&d_graph[0].first, size * 2, 0);

    // walk the local graph and write local data into the global matrix
    d_matrix.resize(size);
    {
        // get iterators to each data field
        auto g = d_graph.begin();
        auto l = local_graph.begin();
        auto m = d_matrix.begin();

        // since all of the graphs (local and global) are ordered, we can
        // traverse the local graph and simply advance the global graph until
        // we hit the correct index in the local graph; this algorithm is O(N)
        // where N is the number of entries in the global graph
        while (l != local_graph.end())
        {
            CHECK(m != d_matrix.end());
            CHECK(g != d_graph.end());

            if (*l == *g)
            {
                CHECK(local_matrix.find(*l) != local_matrix.end());
                CHECK(local_matrix.find(*l)->first == *l);

                *m = local_matrix.find(*l)->second;
                ++l; ++g; ++m;
            }
            else
            {
                ++g; ++m;
            }
        }
    }

    // write the local denominator into the global denominator
    Denominator denominator(local_denominator.begin(), local_denominator.end());

    // now do a global reduction on the matrix and denominator
    profugus::global_sum(&d_matrix[0], size);
    profugus::global_sum(&denominator[0], d_N);

    // finally normalize the fission matrix
    for (int n = 0, j = 0; n < size; ++n)
    {
        // get the j column index (source cell index in the denominator)
        j = d_graph[n].second;
        CHECK(j < denominator.size());
        CHECK(denominator[j] > 0.0);

        // normalize this element of the fission matrix (i,j) with the
        // starting source weight in j
        d_matrix[n] /= denominator[j];
    }
}

//---------------------------------------------------------------------------//
/*!
 * \brief Reset internal fission matrix memory.
 */
void Fission_Matrix_Processor::reset()
{
    Ordered_Graph  g;
    Ordered_Matrix m;

    std::swap(g, d_graph);
    std::swap(m, d_matrix);

    ENSURE(d_graph.empty());
    ENSURE(d_matrix.empty());
}

//---------------------------------------------------------------------------//
// PRIVATE FUNCTIONS
//---------------------------------------------------------------------------//
/*!
 * \brief Parallel merge/sort of global graph.
 */
void Fission_Matrix_Processor::reduce()
{
    // if this is an internal node wait get data from children
    if (d_type == INTERNAL)
    {
        // always receive from at least the left child if this is an internal
        // node
        receive_and_merge(d_children[0]);

        // check to see if we need to receive from the right child, which may
        // not exist
        if (d_children[1] != NONE)
        {
            receive_and_merge(d_children[1]);
        }
    }

    // send the graph to the parent
    if (d_parent != NONE)
    {
        // number of elements in the graph
        int size = d_graph.size();

        profugus::send(&size, 1, d_parent, 800);
        profugus::send(&d_graph[0].first, 2 * size, d_parent, 801);
    }
}

//---------------------------------------------------------------------------//
/*!
 * \brief Merge at each step in the parallel merge sort.
 */
void Fission_Matrix_Processor::receive_and_merge(int child_node)
{
    // receive the size from the child
    int size = 0;
    profugus::receive(&size, 1, child_node, 800);
    CHECK(size >= 0);

    // make a local container to receive the data and add it to the graph
    {
        Ordered_Graph child_data(size);

        // receive the data
        profugus::receive(&child_data[0].first, 2 * size, child_node, 801);

        // now, add this data to the local graph
        d_graph.insert(d_graph.end(), child_data.begin(), child_data.end());
    }

    // sort the graph
    std::sort(d_graph.begin(), d_graph.end());

    // uniqueify the graph
    auto end = std::unique(d_graph.begin(), d_graph.end());
    REMEMBER(int n = end - d_graph.begin());

    // write into a new vector to preserve memory if we have lots of
    // duplicates
    if (end - d_graph.begin() < d_graph.size())
    {
        Ordered_Graph clean(d_graph.begin(), end);
        std::swap(clean, d_graph);
    }

    ENSURE(d_graph.size() == n);
}

} // end namespace profugus

//---------------------------------------------------------------------------//
//                 end of Fission_Matrix_Processor.cc
//---------------------------------------------------------------------------//
