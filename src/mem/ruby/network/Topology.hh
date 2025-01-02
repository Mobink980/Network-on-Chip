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

/*
 * The topology here is configurable; it can be a hierachical (default
 * one) or a 2D torus or a 2D torus with half switches killed. I think
 * all input port has a one-input-one-output switch connected just to
 * control and bandwidth, since we don't control bandwidth on input
 * ports.  Basically, the class has a vector of nodes and edges. First
 * 2*m_nodes elements in the node vector are input and output
 * ports. Edges are represented in two vectors of src and dest
 * nodes. All edges have latency.
 */

#ifndef __MEM_RUBY_NETWORK_TOPOLOGY_HH__
#define __MEM_RUBY_NETWORK_TOPOLOGY_HH__

#include <iostream>
#include <string>
#include <vector>

#include "mem/ruby/common/TypeDefines.hh"
#include "mem/ruby/network/BasicLink.hh"
#include "mem/ruby/protocol/LinkDirection.hh"

namespace gem5
{

namespace ruby
{

//NetDest specifies the network destination of a Message
class NetDest;
//Network inherites from ClockedObject
class Network;

/*
 * We use a three-dimensional vector matrix for calculating
 * the shortest paths for each pair of source and destination
 * and for each type of virtual network. The three dimensions
 * represent the source ID, destination ID, and vnet number.
 */
//3D Matrix of type int
typedef std::vector<std::vector<std::vector<int>>> Matrix;
//port number in an RLInterface
typedef int PortNumber;
//PortDirection is a string
typedef std::string PortDirection;

struct LinkEntry
{
    //network link
    BasicLink *link;
    //src_outport direction
    PortDirection src_outport_dirn;
    //dst_inport direction
    PortDirection dst_inport_dirn;
};

//LinkMap combines <SwitchID, SwitchID> pair with LinkEntry
typedef std::map<std::pair<SwitchID, SwitchID>,
             std::vector<LinkEntry>> LinkMap;

class Topology
{
  public:
    //Topology constructor (for router-based)
    //====================================================================
    Topology(uint32_t num_nodes, uint32_t num_routers, uint32_t num_busses,
            uint32_t num_vnets,
            const std::vector<BasicExtLink *> &ext_links,
            const std::vector<BasicBusLink *> &bus_links,
            const std::vector<BasicIntLink *> &int_links);
    //====================================================================

    //returns the number of switches in the topology
    uint32_t numSwitches() const { return m_number_of_switches; }
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    //returns the number of busses in the topology
    uint32_t numBusses() const { return m_number_of_busses; }
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    //create links for the network
    void createLinks(Network *net);
    //print the name of the topology (ex: Mesh)
    void print(std::ostream& out) const { out << "[Topology]"; }

  private:
    //add an internal link to the topology
    void addLink(SwitchID src, SwitchID dest, BasicLink* link,
                 PortDirection src_outport_dirn = "",
                 PortDirection dest_inport_dirn = "");
    //make a link (internal or external) and add it to routing table
    void makeLink(Network *net, SwitchID src, SwitchID dest,
                  std::vector<NetDest>& routing_table_entry);

    // Helper functions based on chapter 29 of Cormen et al.

    //Find the shortest path for each vnet for a given pair of
    //source and destinations. This is done simply by traversing via
    //all other nodes and finding the minimum distance.
    void extend_shortest_path(Matrix &current_dist, Matrix &latencies,
                              Matrix &inter_switches);

    //returns the shortest path matrix
    Matrix shortest_path(const Matrix &weights,
            Matrix &latencies, Matrix &inter_switches);

    //check if a link is in the shortest path to the given node
    bool link_is_shortest_path_to_node(SwitchID src, SwitchID next,
            SwitchID final, const Matrix &weights, const Matrix &dist,
            int vnet);

    //returns the NetDest of the shortest_path_to_node for a vnet
    NetDest shortest_path_to_node(SwitchID src, SwitchID next,
                                  const Matrix &weights, const Matrix &dist,
                                  int vnet);

    //number of nodes in the network topology
    const uint32_t m_nodes;
    //number of switches in the network topology
    const uint32_t m_number_of_switches;
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    //number of busses in the network topology
    const uint32_t m_number_of_busses;
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    //number of vnets in the network (depends on the cache coherence protocol)
    int m_vnets;
    //vector containing external links of the topology
    std::vector<BasicExtLink*> m_ext_link_vector;
    //======================================================
    //vector containing bus links of the topology
    std::vector<BasicBusLink*> m_bus_link_vector;   
    //======================================================
    //vector containing internal links of the topology
    std::vector<BasicIntLink*> m_int_link_vector;

    //LinkMap for finding the shortest path
    LinkMap m_link_map;
};

//for printing the topology object
inline std::ostream&
operator<<(std::ostream& out, const Topology& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

} // namespace ruby
} // namespace gem5

#endif // __MEM_RUBY_NETWORK_TOPOLOGY_HH__
