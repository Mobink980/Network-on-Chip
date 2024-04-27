/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/BusRoutingUnit.hh"

#include "base/cast.hh"
#include "base/compiler.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/BusInputUnit.hh"
#include "mem/ruby/network/garnet/Bus.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

//BusRoutingUnit constructor
BusRoutingUnit::BusRoutingUnit(Bus *bus)
{
    m_bus = bus; //set the router for this RoutingUnit
    m_routing_table.clear(); //clear m_routing_table vector
    m_weight_table.clear(); //clear m_weight_table vector
}

//add a routing_table_entry (a route) to the routing table
void
BusRoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    //update the size of m_routing_table according to routing_table_entry
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    //update m_routing_table with the elements of routing_table_entry
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

//add the link weight to m_weight_table 
void
BusRoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

//Returns true if vnet is present in the vector of vnets, 
//or if the vector supports all vnets.
bool
BusRoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
BusRoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates.
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes).
    // For unordered vnet, randomly choose any of the links.
    // To have a strict ordering between links, they should be given
    // different weights in the topology file.

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    //go through all the elements of m_routing_table associated with vnet 
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        //if the destination of the message and the links in 
        //m_routing_table[vnet] has an intersection, then that link
        //is a candidate
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        //identify the minimum weight among the candidates link
        //(which one to choose from all the output links that have
        //intersection with msg_destination) 
        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            //if the candidate link's weight is minimum, then that's 
            //a real candidate (we're choosing a link with minimum weight)
            if (m_weight_table[link] == min_weight) {
                num_candidates++; 
                output_link_candidates.push_back(link);
            }
        }
    }

    //if no link has an intersection with the message destination
    //(no candidate link was found)
    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Bus.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0; //for ordered vnet, we choose the first
    //for unordered vnet we choose a random link from our list of candidates
    if (!(m_bus->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    //the output_link we choose to send the flit
    output_link = output_link_candidates.at(candidate);
    return output_link;
}

//add inport direction to the routing table
void
BusRoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx; //inport id
    m_inports_idx2dirn[inport_idx]  = inport_dirn; //inport direction
}

//add outport direction to the routing table
void
BusRoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx; //outport id
    m_outports_idx2dirn[outport_idx]  = outport_dirn; //outport direction
}

// outportCompute() is called by the InputUnit.
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.
int
BusRoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn)
{
    //the outport we want to send the flit to
    int outport = -1; 

    //if the flit has reached to the destination router 
    //(it needs to be ejected from the network)
    if (route.dest_router == m_bus->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        //sending from the right router output_link to be received 
        //by the right NI 
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_bus->get_net_ptr()->getRoutingAlgorithm();

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        // any custom algorithm
        case CUSTOM_: outport =
            outportComputeCustom(route, inport, inport_dirn); break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    //make sure we chose an output_link (outport is computed)
    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions.
// Only for reference purpose in a Mesh.
// By default Garnet uses the routing table.
int
BusRoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    //the outport we want to send the flit to
    //PortDirection is basically a string (ex: north, west, etc.)
    PortDirection outport_dirn = "Unknown";

    //number of rows in the mesh topology
    [[maybe_unused]] int num_rows = m_bus->get_net_ptr()->getNumRows();
    //number of collumns in the mesh topology
    int num_cols = m_bus->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0); 

    //router_id of the current router
    int my_id = m_bus->get_id();
    int my_x = my_id % num_cols; //x_position of the current router
    int my_y = my_id / num_cols; //y_position of the current router

    //router_id of the destination router
    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols; //x_position of the dest router
    int dest_y = dest_id / num_cols; //y_position of the dest router

    int x_hops = abs(dest_x - my_x); //how many hops in the x direction
    int y_hops = abs(dest_y - my_y); //how many hops in the y direction

    bool x_dirn = (dest_x >= my_x); //if true, we need to go to the right
    bool y_dirn = (dest_y >= my_y); //if true, we need to go upward

    // already checked that in outportCompute() function
    //ensure we're not already in the destination router
    assert(!(x_hops == 0 && y_hops == 0)); 

    if (x_hops > 0) { //we have horizontal hops
        if (x_dirn) { //if we need to go rightward
            //ensure the flit is either coming from the NI or the west inport
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East"; //the outport to go is east
        } else { //if we need to go leftward
            //ensure the flit is either coming from the NI or the east inport
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West"; //the outport to go is west
        }
    } else if (y_hops > 0) { //we have vertical hops
        if (y_dirn) { //if we need to go upward
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North"; //the outport to go is north
        } else { //if we need to go downward
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South"; //the outport to go is south
        }
    } else { //we have neither horizontal nor vertical hops
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    //return the outport we computed but in a dirn2idx format
    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
BusRoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
    panic("%s placeholder executed", __FUNCTION__);
}

} // namespace garnet
} // namespace ruby
} // namespace gem5

