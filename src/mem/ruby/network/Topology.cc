/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
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

#include "mem/ruby/network/Topology.hh"

#include <cassert>

#include "base/trace.hh"
#include "debug/RubyNetwork.hh"

#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/BasicLink.hh"
#include "mem/ruby/network/Network.hh"
#include "mem/ruby/slicc_interface/AbstractController.hh"

//=====================================
#include <iostream>
//=====================================


namespace gem5
{

namespace ruby
{

const int INFINITE_LATENCY = 10000; // Yes, this is a big hack

// Note: In this file, we use the first 2*m_nodes SwitchIDs to
// represent the input and output endpoint links.  These really are
// not 'switches', as they will not have a Switch object allocated for
// them. The first m_nodes SwitchIDs are the links into the network,
// the second m_nodes set of SwitchIDs represent the the output queues
// of the network.


//===================================================================
Topology::Topology(uint32_t num_nodes,
                    uint32_t num_routers,
                    uint32_t num_busses,
                    uint32_t num_vnets,
                    const std::vector<BasicExtLink *> &ext_links,
                    const std::vector<BasicBusLink *> &bus_links,
                    const std::vector<BasicIntLink *> &int_links)
                    : m_nodes(MachineType_base_number(MachineType_NUM)),
                    m_number_of_switches(num_routers),
                    m_number_of_busses(num_busses),
                    m_vnets(num_vnets),
                    m_ext_link_vector(ext_links),
                    m_bus_link_vector(bus_links),
                    m_int_link_vector(int_links)
//===================================================================
{
    //std::cout<<"Topology constructor called."<<std::endl;
    // Total nodes/controllers in network
    assert(m_nodes > 1);

    // analyze both the internal and external links, create data structures.
    // The python created external links are bi-directional,
    // and the python created internal links are uni-directional.
    // The networks and topology utilize uni-directional links.
    // Thus each external link is converted to two calls to addLink,
    // one for each direction.
    //
    // External Links
    for (std::vector<BasicExtLink*>::const_iterator i = ext_links.begin();
         i != ext_links.end(); ++i) { //for each ext_link
        //get that ext_link
        BasicExtLink *ext_link = (*i);
        //get the external node for that ext_link (abs_cntrl)
        AbstractController *abs_cntrl = ext_link->params().ext_node;
        //get the internal node for that ext_link (router)
        BasicRouter *router = ext_link->params().int_node;

        //get the machine id for the abs_cntrl
        int machine_base_idx = MachineType_base_number(abs_cntrl->getType());
        //get the port_id of the ext_node that sends link into the network
        int ext_idx1 = machine_base_idx + abs_cntrl->getVersion();
        //get the port_id of the ext_node that represent the output
        //queue of the network (queue that stores network flits to
        //be sent to the ext_node)
        int ext_idx2 = ext_idx1 + m_nodes;
        //get the port_id of the int_node for the ext_link
        int int_idx = router->params().router_id + 2*m_nodes;

        // create the internal uni-directional links in both directions
        // ext to int
        addLink(ext_idx1, int_idx, ext_link); //from ext_node to int_node
        // int to ext
        addLink(int_idx, ext_idx2, ext_link); //from int_node to ext_node
    }

    //==========================================================================
    //==========================================================================
    // Bus Links
    for (std::vector<BasicBusLink*>::const_iterator i = bus_links.begin();
         i != bus_links.end(); ++i) { //for each bus_link
        //get that bus_link
        BasicBusLink *bus_link = (*i);
        //get the external node for that ext_link (abs_cntrl)
        AbstractController *abs_cntrl = bus_link->params().ext_node;
        //get the internal node for that bus_link (bus)
        BasicBus *bus = bus_link->params().int_node;

        //get the machine id for the abs_cntrl
        int machine_base_idx = MachineType_base_number(abs_cntrl->getType());
        //get the port_id of the ext_node that sends link into the network
        int ext_idx1 = machine_base_idx + abs_cntrl->getVersion();
        //get the port_id of the ext_node that represent the output
        //queue of the network (queue that stores network flits to
        //be sent to the ext_node)
        int ext_idx2 = ext_idx1 + m_nodes;
        //get the port_id of the int_node for the bus_link
        int int_idx = bus->params().bus_id + 3*m_nodes;

        // create the internal uni-directional links in both directions
        // ext to int
        addLink(ext_idx1, int_idx, bus_link); //from ext_node to int_node
        // int to ext
        addLink(int_idx, ext_idx2, bus_link); //from int_node to ext_node
    }
    //==========================================================================
    //==========================================================================

    // Internal Links
    for (std::vector<BasicIntLink*>::const_iterator i = int_links.begin();
         i != int_links.end(); ++i) { //for each int_link
        //get the int_link
        BasicIntLink *int_link = (*i);
        //the src_node for that int_link (router_src)
        BasicRouter *router_src = int_link->params().src_node;
        //the dst_node for that int_link (router_dst)
        BasicRouter *router_dst = int_link->params().dst_node;

        //the src_outport for that int_link (e.g., South)
        PortDirection src_outport = int_link->params().src_outport;
        //the dst_inport for that int_link (e.g., East)
        PortDirection dst_inport = int_link->params().dst_inport;

        // Store the IntLink pointers for later
        //push the int_link into m_int_link_vector
        m_int_link_vector.push_back(int_link);

        //the source_router id
        int src = router_src->params().router_id + 2*m_nodes;
        //the destination_router id
        int dst = router_dst->params().router_id + 2*m_nodes;

        // create the internal uni-directional link from src to dst
        addLink(src, dst, int_link, src_outport, dst_inport);
    }

}

//create links for the network
void
Topology::createLinks(Network *net)
{
    // Find maximum switchID
    SwitchID max_switch_id = 0;
    for (LinkMap::const_iterator i = m_link_map.begin();
         i != m_link_map.end(); ++i) {
        //get the src_dst switch (router) pair
        std::pair<SwitchID, SwitchID> src_dest = (*i).first;
        //if the id of the first router in the pair is higher,
        //update max_switch_id
        max_switch_id = std::max(max_switch_id, src_dest.first);
        //if the id of the second router in the pair is higher,
        //update max_switch_id
        max_switch_id = std::max(max_switch_id, src_dest.second);
    }

    // Initialize weight, latency, and inter switched vectors
    int num_switches = max_switch_id+1; //number of switches
    //Matrix for topology link weights
    Matrix topology_weights(m_vnets,
            std::vector<std::vector<int>>(num_switches,
            std::vector<int>(num_switches, INFINITE_LATENCY)));
    //Matrix for latency of topology components
    Matrix component_latencies(num_switches,
            std::vector<std::vector<int>>(num_switches,
            std::vector<int>(m_vnets, -1)));
    //Matrix for inter_switches
    Matrix component_inter_switches(num_switches,
            std::vector<std::vector<int>>(num_switches,
            std::vector<int>(m_vnets, 0)));

    // Set identity weights to zero
    //(weight from a component to itself is 0)
    for (int i = 0; i < topology_weights[0].size(); i++) {
        for (int v = 0; v < m_vnets; v++) {
            topology_weights[v][i][i] = 0;
        }
    }

    // Fill in the topology weights and bandwidth multipliers
    for (auto link_group : m_link_map) {
        std::pair<int, int> src_dest = link_group.first;
        std::vector<bool> vnet_done(m_vnets, 0);
        int src = src_dest.first; //source router (switch)
        int dst = src_dest.second; //dest router (switch)

        // Iterate over all links for this source and destination
        std::vector<LinkEntry> link_entries = link_group.second;
        for (int l = 0; l < link_entries.size(); l++) {
            BasicLink* link = link_entries[l].link;
            if (link->mVnets.size() == 0) {
                for (int v = 0; v < m_vnets; v++) {
                    // Two links connecting same src and destination
                    // cannot carry same vnets.
                    fatal_if(vnet_done[v], "Two links connecting same src"
                    " and destination cannot support same vnets");

                    component_latencies[src][dst][v] = link->m_latency;
                    topology_weights[v][src][dst] = link->m_weight;
                    // vnet_done[v] = true;
                }
            } else { //if the size of the vnet for the link is not zero
                for (int v = 0; v < link->mVnets.size(); v++) {
                    int vnet = link->mVnets[v];
                    fatal_if(vnet >= m_vnets, "Not enough virtual networks "
                             "(setting latency and weight for vnet %d)", vnet);
                    // Two links connecting same src and destination
                    // cannot carry same vnets.
                    fatal_if(vnet_done[vnet], "Two links connecting same src"
                    " and destination cannot support same vnets");

                    component_latencies[src][dst][vnet] = link->m_latency;
                    topology_weights[vnet][src][dst] = link->m_weight;
                    // vnet_done[vnet] = true;
                }
            }
        }
    }

    // Walk topology and hookup the links
    Matrix dist = shortest_path(topology_weights, component_latencies,
                                component_inter_switches);

    for (int i = 0; i < topology_weights[0].size(); i++) {
        for (int j = 0; j < topology_weights[0][i].size(); j++) {
            std::vector<NetDest> routingMap;
            routingMap.resize(m_vnets);

            // Not all sources and destinations are connected
            // by direct links. We only construct the links
            // which have been configured in topology.
            bool realLink = false;

            for (int v = 0; v < m_vnets; v++) {
                int weight = topology_weights[v][i][j];
                if (weight > 0 && weight != INFINITE_LATENCY) {
                    realLink = true;
                    routingMap[v] =
                        shortest_path_to_node(i, j, topology_weights, dist, v);
                }
            }
            // Make one link for each set of vnets between
            // a given source and destination. We do not
            // want to create one link for each vnet.
            if (realLink) {
                makeLink(net, i, j, routingMap);
            }
        }
    }
}

//add an internal link to the topology
void
Topology::addLink(SwitchID src, SwitchID dest, BasicLink* link,
                  PortDirection src_outport_dirn,
                  PortDirection dst_inport_dirn)
{
    //make sure src and dst SwitchIDs are valid
    //=================================================
    assert(src <= m_number_of_switches + (3*m_nodes));
    assert(dest <= m_number_of_switches + (3*m_nodes));
    //=================================================

    //a pair to save the src and dst SwitchIDs
    std::pair<int, int> src_dest_pair;
    src_dest_pair.first = src;
    src_dest_pair.second = dest;
    //to hold the link, and the direction of src_outport
    //and dst_inport
    LinkEntry link_entry;

    link_entry.link = link;
    link_entry.src_outport_dirn = src_outport_dirn;
    link_entry.dst_inport_dirn  = dst_inport_dirn;

    //find our src_dest_pair in m_link_map
    auto lit = m_link_map.find(src_dest_pair);
    //if you could find it (there's already a link between
    //this src and dest)
    if (lit != m_link_map.end()) {
        // HeteroGarnet allows multiple links between
        // same source-destination pair supporting
        // different vnets. If there is a link already
        // between a given pair of source and destination
        // add this new link to it.
        lit->second.push_back(link_entry);
    } else { //no previous links for this src_dest_pair
        //a vector of type LinkEntry
        std::vector<LinkEntry> links;
        //add this link_entry to the created vector
        links.push_back(link_entry);
        //update m_link_map for the src_dest_pair
        m_link_map[src_dest_pair] = links;
    }
}

//make a link (internal or external) and add it to routing table
void
Topology::makeLink(Network *net, SwitchID src, SwitchID dest,
                   std::vector<NetDest>& routing_table_entry)
{
    // Make sure we're not trying to connect two end-point nodes
    // directly together
    assert(src >= 2 * m_nodes || dest >= 2 * m_nodes);
    std::pair<int, int> src_dest;
    LinkEntry link_entry;
    src_dest.first = src;
    src_dest.second = dest;
    std::vector<LinkEntry> links = m_link_map[src_dest];

    //==========================================================
    //==========================================================
    // We either have MakeExtInLink (from NI to Router) 
    // or MakeBusInLink (from NI to Bus)
    if (src < m_nodes) {

      if (dest >= (3*m_nodes) ) { //we have a bus link (from NI to Bus)
        for (int l = 0; l < links.size(); l++) {
            link_entry = links[l];
            std::vector<NetDest> linkRoute;
            linkRoute.resize(m_vnets);
            BasicLink *link = link_entry.link;
            if (link->mVnets.size() == 0) {
                net->makeBusInLink(src, dest - (3 * m_nodes), link,
                                routing_table_entry);
            } else {
                for (int v = 0; v< link->mVnets.size(); v++) {
                    int vnet = link->mVnets[v];
                    linkRoute[vnet] = routing_table_entry[vnet];
                }
                net->makeBusInLink(src, dest - (3 * m_nodes), link,
                                linkRoute);
            }
        }
      
      } else { //we have an ext link (from NI to Router)
        for (int l = 0; l < links.size(); l++) {
            assert(dest >= (2 * m_nodes));
            link_entry = links[l];
            std::vector<NetDest> linkRoute;
            linkRoute.resize(m_vnets);
            BasicLink *link = link_entry.link;
            if (link->mVnets.size() == 0) {
                net->makeExtInLink(src, dest - (2 * m_nodes), link,
                                routing_table_entry);
            } else {
                for (int v = 0; v< link->mVnets.size(); v++) {
                    int vnet = link->mVnets[v];
                    linkRoute[vnet] = routing_table_entry[vnet];
                }
                net->makeExtInLink(src, dest - (2 * m_nodes), link,
                                linkRoute);
            }
        }
      } 
      
    } 
    
    // We either have MakeExtOutLink (from Router to NI) 
    // or MakeBusOutLink (from Bus to NI)    
    else if (dest < (2*m_nodes)) { 
      assert(dest >= m_nodes);
      NodeID node = dest - m_nodes;
      if (src >= (3*m_nodes) ) { //we have a bus link (from Bus to NI) 
        for (int l = 0; l < links.size(); l++) {
            link_entry = links[l];
            std::vector<NetDest> linkRoute;
            linkRoute.resize(m_vnets);
            BasicLink *link = link_entry.link;
            if (link->mVnets.size() == 0) {
                net->makeBusOutLink(src - (3 * m_nodes), node, link,
                                 routing_table_entry);
            } else {
                for (int v = 0; v< link->mVnets.size(); v++) {
                    int vnet = link->mVnets[v];
                    linkRoute[vnet] = routing_table_entry[vnet];
                }
                net->makeBusOutLink(src - (3 * m_nodes), node, link,
                                linkRoute);
            }
        }
      
      } else { //we have an ext link (from Router to NI)
        assert(src >= (2 * m_nodes));
        for (int l = 0; l < links.size(); l++) {
            link_entry = links[l];
            std::vector<NetDest> linkRoute;
            linkRoute.resize(m_vnets);
            BasicLink *link = link_entry.link;
            if (link->mVnets.size() == 0) {
                net->makeExtOutLink(src - (2 * m_nodes), node, link,
                                 routing_table_entry);
            } else {
                for (int v = 0; v< link->mVnets.size(); v++) {
                    int vnet = link->mVnets[v];
                    linkRoute[vnet] = routing_table_entry[vnet];
                }
                net->makeExtOutLink(src - (2 * m_nodes), node, link,
                                linkRoute);
            }
        }
      }         
    }

    else { //we have an internal link to make
      assert(2 * m_nodes <= src && src < 3 * m_nodes);
      assert(2 * m_nodes <= dest && dest < 3 * m_nodes);
      for (int l = 0; l < links.size(); l++) {
          link_entry = links[l];
          std::vector<NetDest> linkRoute;
          linkRoute.resize(m_vnets);
          BasicLink *link = link_entry.link;
          if (link->mVnets.size() == 0) {
              net->makeInternalLink(src - (2 * m_nodes),
                            dest - (2 * m_nodes), link, routing_table_entry,
                            link_entry.src_outport_dirn,
                            link_entry.dst_inport_dirn);
          } else {
              for (int v = 0; v< link->mVnets.size(); v++) {
                  int vnet = link->mVnets[v];
                  linkRoute[vnet] = routing_table_entry[vnet];
              }
              net->makeInternalLink(src - (2 * m_nodes),
                            dest - (2 * m_nodes), link, linkRoute,
                            link_entry.src_outport_dirn,
                            link_entry.dst_inport_dirn);
          }
      }
    }
    //==========================================================
    //==========================================================
}

// The following all-pairs shortest path algorithm is based on the
// discussion from Cormen et al., Chapter 26.1.

void
Topology::extend_shortest_path(Matrix &current_dist, Matrix &latencies,
    Matrix &inter_switches)
{
    int nodes = current_dist[0].size();

    // We find the shortest path for each vnet for a given pair of
    // source and destinations. This is done simply by traversing via
    // all other nodes and finding the minimum distance.
    for (int v = 0; v < m_vnets; v++) {
        // There is a different topology for each vnet. Here we try to
        // build a topology by finding the minimum number of intermediate
        // switches needed to reach the destination
        bool change = true;
        while (change) {
            change = false;
            for (int i = 0; i < nodes; i++) {
                for (int j = 0; j < nodes; j++) {
                    // We follow an iterative process to build the shortest
                    // path tree:
                    // 1. Start from the direct connection (if there is one,
                    // otherwise assume a hypothetical infinite weight link).
                    // 2. Then we iterate through all other nodes considering
                    // new potential intermediate switches. If we find any
                    // lesser weight combination, we set(update) that as the
                    // new weight between the source and destination.
                    // 3. Repeat for all pairs of nodes.
                    // 4. Go to step 1 if there was any new update done in
                    // Step 2.
                    int minimum = current_dist[v][i][j];
                    int previous_minimum = minimum;
                    int intermediate_switch = -1;
                    for (int k = 0; k < nodes; k++) {
                        minimum = std::min(minimum,
                            current_dist[v][i][k] + current_dist[v][k][j]);
                        if (previous_minimum != minimum) {
                            intermediate_switch = k;
                            //we found an intermediate_switch (k) that helps us
                            //to reach the destination faster
                            inter_switches[i][j][v] =
                                inter_switches[i][k][v] +
                                inter_switches[k][j][v] + 1;
                        }
                        //update previous_minimum with new minimum distance
                        previous_minimum = minimum;
                    }
                    if (current_dist[v][i][j] != minimum) {
                        change = true;
                        //update current_dist between nodes i and j
                        //for vnet v in the current_dist Matrix
                        current_dist[v][i][j] = minimum;
                        assert(intermediate_switch >= 0);
                        assert(intermediate_switch < latencies[i].size());
                        //update the latencies Matrix for the latency
                        //to rach from node i to node j
                        latencies[i][j][v] =
                            latencies[i][intermediate_switch][v] +
                            latencies[intermediate_switch][j][v];
                    }
                }
            }
        }
    }
}

//It takes weights, latencies, and inter_switches matrices and
//returns the shortest path matrix
Matrix
Topology::shortest_path(const Matrix &weights, Matrix &latencies,
                        Matrix &inter_switches)
{
    //define a dist matrix and initialize it with weights matrix
    Matrix dist = weights;
    //find the shortest path for all pairs of source and destinations
    extend_shortest_path(dist, latencies, inter_switches);
    //return the shortest_path (updated current_dist) matrix
    return dist;
}

//check if a link is in the shortest path to the given node
//see if SwitchID next is a good intermidiate or not
bool
Topology::link_is_shortest_path_to_node(SwitchID src, SwitchID next,
                                        SwitchID final, const Matrix &weights,
                                        const Matrix &dist, int vnet)
{
    return weights[vnet][src][next] + dist[vnet][next][final] ==
        dist[vnet][src][final];
}

//returns the NetDest of the shortest_path_to_node for a vnet
NetDest
Topology::shortest_path_to_node(SwitchID src, SwitchID next,
                                const Matrix &weights, const Matrix &dist,
                                int vnet)
{
    NetDest result;
    int d = 0;
    int machines;
    int max_machines;

    machines = MachineType_NUM;
    max_machines = MachineType_base_number(MachineType_NUM);

    for (int m = 0; m < machines; m++) {
        for (NodeID i = 0; i < MachineType_base_count((MachineType)m); i++) {
            // we use "d+max_machines" below since the "destination"
            // switches for the machines are numbered
            // [MachineType_base_number(MachineType_NUM)...
            //  2*MachineType_base_number(MachineType_NUM)-1] for the
            // component network
            if (link_is_shortest_path_to_node(src, next, d + max_machines,
                    weights, dist, vnet)) {
                MachineID mach = {(MachineType)m, i};
                result.add(mach);
            }
            d++;
        }
    }

    DPRINTF(RubyNetwork, "Returning shortest path\n"
            "(src-(2*max_machines)): %d, (next-(2*max_machines)): %d, "
            "src: %d, next: %d, vnet:%d result: %s\n",
            (src-(2*max_machines)), (next-(2*max_machines)),
            src, next, vnet, result);

    return result;
}

} // namespace ruby
} // namespace gem5
