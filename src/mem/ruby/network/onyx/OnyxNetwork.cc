/*
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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


#include "mem/ruby/network/onyx/OnyxNetwork.hh"

#include <cassert>

#include "base/cast.hh"
#include "base/compiler.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/network/onyx/CommonTypes.hh"
#include "mem/ruby/network/onyx/AckLink.hh"
#include "mem/ruby/network/onyx/OnyxLink.hh"
#include "mem/ruby/network/onyx/InterfaceModule.hh"
#include "mem/ruby/network/onyx/NetLink.hh"
#include "mem/ruby/network/onyx/Switcher.hh"
//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
#include "mem/ruby/network/onyx/Bus.hh"
//&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
#include "mem/ruby/system/RubySystem.hh"
//=====================================
#include <iostream>
//=====================================

namespace gem5
{

namespace ruby
{

namespace onyx
{

/*
 * OnyxNetwork sets up the routers and links and collects stats.
 * Default parameters (OnyxNetwork.py) can be overwritten from command line
 * (see configs/network/Network.py)
 */

// OnyxNetwork class constructor
OnyxNetwork::OnyxNetwork(const Params &p)
    : Chain(p)
{
    m_num_rows = p.num_rows; // number of rows
    m_num_cols = p.num_columns; // number of columns
    m_num_layers = p.num_layers; // number of layers
    m_ni_flit_size = p.ni_flit_size; // flit size
    m_max_vcs_per_vnet = 0; // maximum number of VCs per Vnet
    m_buffers_per_data_vc = p.buffers_per_data_vc; // num of buffers per data VC
    m_buffers_per_ctrl_vc = p.buffers_per_ctrl_vc; // num of buffers per cntrl VC
    m_routing_algorithm = p.routing_algorithm; // routing algorithm for NoC
    m_next_packet_id = 0; //id of the next packet

    // fault_model enable or disable
    m_enable_fault_model = p.enable_fault_model;
    if (m_enable_fault_model)
        fault_model = p.fault_model;

    // for each virtual network, we have a Vnet type (they're equal)
    m_vnet_type.resize(m_virtual_networks);

    // setting the Vnet types for different virtual networks
    for (int i = 0 ; i < m_virtual_networks ; i++) {
        if (m_vnet_type_names[i] == "response")
            m_vnet_type[i] = DATA_VNET_; // carries data (and ctrl) packets
        else
            m_vnet_type[i] = CTRL_VNET_; // carries only ctrl packets
    }

    // record the routers (setting the routers for the network)
    for (std::vector<BasicRouter*>::const_iterator i =  p.routers.begin();
         i != p.routers.end(); ++i) { // for each router
        // get the router
        Switcher* router = safe_cast<Switcher*>(*i);
        // push the router in m_routers vector
        m_routers.push_back(router);
        // initialize the router's network pointers
        router->init_net_ptr(this);
    }

    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    // record the busses (setting the busses for the network)
    for (std::vector<BasicBus*>::const_iterator i =  p.busses.begin();
         i != p.busses.end(); ++i) { // for each bus
        // get the bus
        Bus* bus = safe_cast<Bus*>(*i);
        // push the bus in m_busses vector
        m_busses.push_back(bus);
        // initialize the bus's network pointers
        bus->init_net_ptr(this);
    }
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

    // record the network interfaces (setting the NIs for the network)
    for (std::vector<ClockedObject*>::const_iterator i = p.netifs.begin();
         i != p.netifs.end(); ++i) { // for each NI
        // get the NI
        InterfaceModule *ni = safe_cast<InterfaceModule *>(*i);
        // push the NI into m_nis vector
        m_nis.push_back(ni);
        // initialize the NI's network pointers
        ni->init_net_ptr(this);
    }

    // Print Onyx version
    inform("Onyx version %s\n", onyxVersion);
}

void
OnyxNetwork::init()
{
    // call the init function of the Network class
    Chain::init();

    // for every node in the network, set the m_toNetQueues and
    // m_fromNetQueues for the NI
    for (int i=0; i < m_nodes; i++) {
        m_nis[i]->addNode(m_toNetQueues[i], m_fromNetQueues[i]);
    }

    // The topology pointer should have already been initialized in the
    // parent network constructor
    assert(m_topology_ptr != NULL);
    // create the links for the OnyxNetwork object
    m_topology_ptr->createLinks(this);

    // Initialize topology specific parameters
    if (getNumRows() > 0)
    {
        // Only for Mesh topology
        // m_num_rows and m_num_cols are only used for
        // implementing XY or custom routing in RoutingUnit.cc
        m_num_rows = getNumRows();
        m_num_layers = getNumLayers();
        // if we have a 2D NoC
        if (m_num_layers == 1)
        {
            m_num_cols = m_routers.size() / m_num_rows;
            // make sure that num_rows * num_cols = num_routers
            assert(m_num_rows * m_num_cols == m_routers.size());
        }
        else
        { // we have a 3D NoC
            m_num_cols = getNumCols();
            //we check this in the config file (commented to use additional routers)
            // assert(m_num_rows * m_num_cols * m_num_layers == m_routers.size());
        }
    }
    else
    {
        m_num_rows = -1;
        m_num_cols = -1;
    }

    // FaultModel: declare each router to the fault model
    if (isFaultModelEnabled()) {
        for (std::vector<Switcher*>::const_iterator i= m_routers.begin();
             i != m_routers.end(); ++i) { // for each router
            // get the router
            Switcher* router = safe_cast<Switcher*>(*i);
            // get the router_id from the fault_model
            [[maybe_unused]] int router_id =
                fault_model->declare_router(router->get_num_inports(),
                                            router->get_num_outports(),
                                            router->get_vc_per_vnet(),
                                            getBuffersPerDataVC(),
                                            getBuffersPerCtrlVC());
            // make sure the router_id of the fault_model and the real
            // id of the router are the same
            assert(router_id == router->get_id());
            // print the aggregate fault probability for the router
            router->printAggregateFaultProbability(std::cout);
            // print the fault vector for the router
            router->printFaultVector(std::cout);
        }
    }

}

/*
 * This function creates a link from the Network Interface (NI)
 * into the Network.
 * It creates a Network Link from the NI to a Router and a Credit Link from
 * the Router to the NI
*/
void
OnyxNetwork::makeExtInLink(NodeID global_src, SwitchID dest, BasicLink* link,
                             std::vector<NetDest>& routing_table_entry)
{
    // get the local_id of the Node (or NI)
    // local_id may be equal with global_id if we only have one network
    NodeID local_src = getLocalNodeID(global_src);
    // make sure the local_id is less than the total number of nodes
    assert(local_src < m_nodes);

    // create an onyx external link
    OnyxExtLink* onyx_link = safe_cast<OnyxExtLink*>(link);

    // OnyxExtLink is bi-directional (EXT_IN_ means from NI to Router)
    NetLink* net_link = onyx_link->m_network_links[LinkDirection_In];
    net_link->setType(EXT_IN_); // set the type of the network link to EXT_IN_
    AckLink* credit_link = onyx_link->m_credit_links[LinkDirection_In];

    m_networklinks.push_back(net_link); // add net_link to network links
    m_creditlinks.push_back(credit_link); // add credit_link to credit links

    // destination inport direction: means the dest of the link is router
    PortDirection dst_inport_dirn = "Local";

    // set the maximum number of VCs per Vnet
    m_max_vcs_per_vnet = std::max(m_max_vcs_per_vnet,
                             m_routers[dest]->get_vc_per_vnet());

    /*
     * We check if a bridge was enabled at any end of the link.
     * The bridge is enabled if either of clock domain
     * crossing (CDC) or Serializer-Deserializer(SerDes) unit is
     * enabled for the link at each end. The bridge encapsulates
     * the functionality for both CDC and SerDes and is a Consumer
     * object similiar to a NetworkLink.
     *
     * If a bridge was enabled we connect the NI and Routers to
     * bridge before connecting the link. Example, if an external
     * bridge is enabled, we would connect:
     * NI--->NetworkBridge--->OnyxExtLink---->Router
     */

    // add an outport at the side of the NI (for the OnyxExtLink)
    if (onyx_link->extBridgeEn) {
        DPRINTF(RubyNetwork, "Enable external bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->extNetBridge[LinkDirection_In];
        m_nis[local_src]->
        addOutPort(n_bridge,
                   onyx_link->extCredBridge[LinkDirection_In],
                   dest, m_routers[dest]->get_vc_per_vnet());
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_nis[local_src]->addOutPort(net_link, credit_link, dest,
            m_routers[dest]->get_vc_per_vnet());
    }

    // add an inport at the side of the router (for the OnyxExtLink)
    if (onyx_link->intBridgeEn) {
        DPRINTF(RubyNetwork, "Enable internal bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->intNetBridge[LinkDirection_In];
        m_routers[dest]->
            addInPort(dst_inport_dirn,
                      n_bridge,
                      onyx_link->intCredBridge[LinkDirection_In]);
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
    }

}

/*
 * This function creates a link from the Network to a NI.
 * It creates a Network Link from a Router to the NI and
 * a Credit Link from NI to the Router
*/
void
OnyxNetwork::makeExtOutLink(SwitchID src, NodeID global_dest,
                              BasicLink* link,
                              std::vector<NetDest>& routing_table_entry)
{
    // get the local_id of the node (or NI)
    NodeID local_dest = getLocalNodeID(global_dest);
    // make sure the local_id is less than the total number of nodes
    assert(local_dest < m_nodes);
    // id of the router must be less than total number of routers
    assert(src < m_routers.size());
    // make sure we have a router object for the id, and not a NULL
    assert(m_routers[src] != NULL);

    // create an onyx external link
    OnyxExtLink* onyx_link = safe_cast<OnyxExtLink*>(link);

    // OnyxExtLink is bi-directional (EXT_OUT_ means from Router to NI)
    NetLink* net_link = onyx_link->m_network_links[LinkDirection_Out];
    net_link->setType(EXT_OUT_); // set the type of the network link to EXT_OUT_
    AckLink* credit_link = onyx_link->m_credit_links[LinkDirection_Out];

    m_networklinks.push_back(net_link); // add net_link to network links
    m_creditlinks.push_back(credit_link); // add credit_link to credit links

    // source outport direction: means the src of the link is router
    PortDirection src_outport_dirn = "Local";

    // set the maximum number of VCs per Vnet
    m_max_vcs_per_vnet = std::max(m_max_vcs_per_vnet,
                             m_routers[src]->get_vc_per_vnet());

    /*
     * We check if a bridge was enabled at any end of the link.
     * The bridge is enabled if either of clock domain
     * crossing (CDC) or Serializer-Deserializer(SerDes) unit is
     * enabled for the link at each end. The bridge encapsulates
     * the functionality for both CDC and SerDes and is a Consumer
     * object similiar to a NetworkLink.
     *
     * If a bridge was enabled we connect the NI and Routers to
     * bridge before connecting the link. Example, if an external
     * bridge is enabled, we would connect:
     * NI<---NetworkBridge<---OnyxExtLink<----Router
     */

    // add an inport at the side of the NI (for the OnyxExtLink)
    if (onyx_link->extBridgeEn) {
        DPRINTF(RubyNetwork, "Enable external bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->extNetBridge[LinkDirection_Out];
        m_nis[local_dest]->
            addInPort(n_bridge, onyx_link->extCredBridge[LinkDirection_Out]);
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_nis[local_dest]->addInPort(net_link, credit_link);
    }

    // add an outport at the side of the router (for the OnyxExtLink)
    if (onyx_link->intBridgeEn) {
        DPRINTF(RubyNetwork, "Enable internal bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->intNetBridge[LinkDirection_Out];
        m_routers[src]->
            addOutPort(src_outport_dirn,
                       n_bridge,
                       routing_table_entry, link->m_weight,
                       onyx_link->intCredBridge[LinkDirection_Out],
                       m_routers[src]->get_vc_per_vnet());
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_routers[src]->
            addOutPort(src_outport_dirn, net_link,
                       routing_table_entry,
                       link->m_weight, credit_link,
                       m_routers[src]->get_vc_per_vnet());
    }
}

//=====================================================================
//=====================================================================
/*
 * This function creates a link from the Network Interface (NI)
 * to Bus.
 * It creates a Network Link from the NI to a Bus and a Credit Link from
 * the Bus to the NI
*/
void
OnyxNetwork::makeBusInLink(NodeID global_src, SwitchID dest, BasicLink* link,
                             std::vector<NetDest>& routing_table_entry)
{
    // get the local_id of the Node (or NI)
    // local_id may be equal with global_id if we only have one network
    NodeID local_src = getLocalNodeID(global_src);
    // make sure the local_id is less than the total number of nodes
    assert(local_src < m_nodes);

    // create an onyx bus link
    OnyxBusLink* onyx_link = safe_cast<OnyxBusLink*>(link);

    // OnyxBusLink is bi-directional (BUS_IN_ means from NI to Bus)
    NetLink* net_link = onyx_link->m_network_links[LinkDirection_In];
    net_link->setType(BUS_IN_); // set the type of the network link to BUS_IN_
    AckLink* credit_link = onyx_link->m_credit_links[LinkDirection_In];

    m_networklinks.push_back(net_link); // add net_link to network links
    m_creditlinks.push_back(credit_link); // add credit_link to credit links

    // destination inport direction: means the dest of the link is bus
    PortDirection dst_inport_dirn = "Local";

    // set the maximum number of VCs per Vnet
    m_max_vcs_per_vnet = std::max(m_max_vcs_per_vnet,
                             m_busses[dest]->get_vc_per_vnet());

    /*
     * We check if a bridge was enabled at any end of the link.
     * The bridge is enabled if either of clock domain
     * crossing (CDC) or Serializer-Deserializer(SerDes) unit is
     * enabled for the link at each end. The bridge encapsulates
     * the functionality for both CDC and SerDes and is a Consumer
     * object similiar to a NetworkLink.
     *
     * If a bridge was enabled we connect the NI and Routers to
     * bridge before connecting the link. Example, if an external
     * bridge is enabled, we would connect:
     * NI--->NetworkBridge--->OnyxExtLink---->Router
     */

    // add an outport at the side of the NI (for the OnyxBusLink)
    if (onyx_link->extBridgeEn) {
        DPRINTF(RubyNetwork, "Enable external bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->extNetBridge[LinkDirection_In];
        m_nis[local_src]->
        addNetworkOutport(n_bridge,
                          onyx_link->extCredBridge[LinkDirection_In],
                          dest, m_busses[dest]->get_vc_per_vnet());
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_nis[local_src]->addNetworkOutport(net_link, credit_link, dest,
                        m_busses[dest]->get_vc_per_vnet());
    }

    // add an inport at the side of the bus (for the OnyxBusLink)
    if (onyx_link->intBridgeEn) {
        DPRINTF(RubyNetwork, "Enable internal bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->intNetBridge[LinkDirection_In];
        m_busses[dest]->addInPort(dst_inport_dirn, n_bridge,   
                             onyx_link->intCredBridge[LinkDirection_In]);
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_busses[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
    }

}

/*
 * This function creates a link from Bus to NI.
 * It creates a Network Link from a Bus to the NI and
 * a Credit Link from NI to Bus
*/
void
OnyxNetwork::makeBusOutLink(SwitchID src, NodeID global_dest,
                              BasicLink* link,
                              std::vector<NetDest>& routing_table_entry)
{
    // get the local_id of the node (or NI)
    NodeID local_dest = getLocalNodeID(global_dest);
    // make sure the local_id is less than the total number of nodes
    assert(local_dest < m_nodes);
    // id of the bus must be less than total number of busses
    assert(src < m_busses.size());
    // make sure we have a router object for the id, and not a NULL
    assert(m_busses[src] != NULL);

    // create an onyx bus link
    OnyxBusLink* onyx_link = safe_cast<OnyxBusLink*>(link);

    // OnyxBusLink is bi-directional (BUS_OUT_ means from Bus to NI)
    NetLink* net_link = onyx_link->m_network_links[LinkDirection_Out];
    net_link->setType(BUS_OUT_); // set the type of the network link to BUS_OUT_
    AckLink* credit_link = onyx_link->m_credit_links[LinkDirection_Out];

    m_networklinks.push_back(net_link); // add net_link to network links
    m_creditlinks.push_back(credit_link); // add credit_link to credit links

    // source outport direction: means the src of the link is router
    PortDirection src_outport_dirn = "Local";

    // set the maximum number of VCs per Vnet
    m_max_vcs_per_vnet = std::max(m_max_vcs_per_vnet,
                             m_busses[src]->get_vc_per_vnet());

    /*
     * We check if a bridge was enabled at any end of the link.
     * The bridge is enabled if either of clock domain
     * crossing (CDC) or Serializer-Deserializer(SerDes) unit is
     * enabled for the link at each end. The bridge encapsulates
     * the functionality for both CDC and SerDes and is a Consumer
     * object similiar to a NetworkLink.
     *
     * If a bridge was enabled we connect the NI and Routers to
     * bridge before connecting the link. Example, if an external
     * bridge is enabled, we would connect:
     * NI<---NetworkBridge<---OnyxExtLink<----Router
     */

    // add an inport at the side of the NI (for the OnyxBusLink)
    if (onyx_link->extBridgeEn) {
        DPRINTF(RubyNetwork, "Enable external bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->extNetBridge[LinkDirection_Out];
        m_nis[local_dest]->
            addNetworkInport(n_bridge, onyx_link->extCredBridge[LinkDirection_Out]);
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_nis[local_dest]->addNetworkInport(net_link, credit_link);
    }

    // add an outport at the side of the bus (for the OnyxBusLink)
    if (onyx_link->intBridgeEn) {
        DPRINTF(RubyNetwork, "Enable internal bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->intNetBridge[LinkDirection_Out];
        m_busses[src]->
            addOutPort(src_outport_dirn,
                       n_bridge,
                       routing_table_entry, link->m_weight,
                       onyx_link->intCredBridge[LinkDirection_Out],
                       m_busses[src]->get_vc_per_vnet());
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_busses[src]->
            addOutPort(src_outport_dirn, net_link,
                       routing_table_entry,
                       link->m_weight, credit_link,
                       m_busses[src]->get_vc_per_vnet());
    }
}
//=====================================================================
//=====================================================================

/*
 * This function creates an internal network link between two routers.
 * It adds both the network link and an opposite credit link.
*/
void
OnyxNetwork::makeInternalLink(SwitchID src, SwitchID dest, BasicLink* link,
                                std::vector<NetDest>& routing_table_entry,
                                PortDirection src_outport_dirn,
                                PortDirection dst_inport_dirn)
{
    // create an onyx internal link
    OnyxIntLink* onyx_link = safe_cast<OnyxIntLink*>(link);

    // OnyxIntLink is unidirectional (INT_ means from router to router)
    NetLink* net_link = onyx_link->m_network_link;
    net_link->setType(INT_);
    AckLink* credit_link = onyx_link->m_credit_link;

    m_networklinks.push_back(net_link); // add net_link to network links
    m_creditlinks.push_back(credit_link); // add credit_link to credit links

    // set the maximum number of VCs per Vnet
    m_max_vcs_per_vnet = std::max(m_max_vcs_per_vnet,
                             std::max(m_routers[dest]->get_vc_per_vnet(),
                             m_routers[src]->get_vc_per_vnet()));

    /*
     * We check if a bridge was enabled at any end of the link.
     * The bridge is enabled if either of clock domain
     * crossing (CDC) or Serializer-Deserializer(SerDes) unit is
     * enabled for the link at each end. The bridge encapsulates
     * the functionality for both CDC and SerDes and is a Consumer
     * object similiar to a NetworkLink.
     *
     * If a bridge was enabled we connect the NI and Routers to
     * bridge before connecting the link. Example, if a source
     * bridge is enabled, we would connect:
     * Router--->NetworkBridge--->OnyxIntLink---->Router
     */

    // add an inport to the destination router (for the OnyxIntLink)
    if (onyx_link->dstBridgeEn) {
        DPRINTF(RubyNetwork, "Enable destination bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->dstNetBridge;
        m_routers[dest]->addInPort(dst_inport_dirn, n_bridge,
                                   onyx_link->dstCredBridge);
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_routers[dest]->addInPort(dst_inport_dirn, net_link, credit_link);
    }

    // add an outport to the source router (for the OnyxIntLink)
    if (onyx_link->srcBridgeEn) {
        DPRINTF(RubyNetwork, "Enable source bridge for %s\n",
            onyx_link->name());
        NetBridge *n_bridge = onyx_link->srcNetBridge;
        m_routers[src]->
            addOutPort(src_outport_dirn, n_bridge,
                       routing_table_entry,
                       link->m_weight, onyx_link->srcCredBridge,
                       m_routers[dest]->get_vc_per_vnet());
        m_networkbridges.push_back(n_bridge);
    } else {
        // without NetworkBridge
        m_routers[src]->addOutPort(src_outport_dirn, net_link,
                        routing_table_entry,
                        link->m_weight, credit_link,
                        m_routers[dest]->get_vc_per_vnet());
    }
}


// Total routers in the network
int
OnyxNetwork::getNumRouters()
{
    return m_routers.size();
}

// Get ID of router connected to a NI.
int
OnyxNetwork::get_router_id(int global_ni, int vnet)
{
    // get the local_id of the NI that has global_ni global_id
    NodeID local_ni = getLocalNodeID(global_ni);
    // return the router_id of the router connected to this NI
    // for the given vnet
    return m_nis[local_ni]->get_router_id(vnet);
}

//===========================================================
//===========================================================
// Get ID of bus connected to a NI.
int
OnyxNetwork::get_bus_id(int global_ni, int vnet)
{
    // get the local_id of the NI that has global_ni global_id
    NodeID local_ni = getLocalNodeID(global_ni);
    // return the router_id of the router connected to this NI
    // for the given vnet
    return m_nis[local_ni]->get_bus_id(vnet);
}
// Total busses in the network
int
OnyxNetwork::getNumBusses()
{
    return m_busses.size();
}
//===========================================================
//===========================================================

// for registering the OnyxNetwork stats
void
OnyxNetwork::regStats()
{
    // call the regStats() function from the parent class
    Chain::regStats();

    // Packets
    m_packets_received
        .init(m_virtual_networks)
        .name(name() + ".packets_received")
        .flags(statistics::pdf | statistics::total | statistics::nozero |
            statistics::oneline)
        ;

    m_packets_injected
        .init(m_virtual_networks)
        .name(name() + ".packets_injected")
        .flags(statistics::pdf | statistics::total | statistics::nozero |
            statistics::oneline)
        ;

    m_packet_network_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_network_latency")
        .flags(statistics::oneline)
        ;

    m_packet_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".packet_queueing_latency")
        .flags(statistics::oneline)
        ;

    for (int i = 0; i < m_virtual_networks; i++) {
        m_packets_received.subname(i, csprintf("vnet-%i", i));
        m_packets_injected.subname(i, csprintf("vnet-%i", i));
        m_packet_network_latency.subname(i, csprintf("vnet-%i", i));
        m_packet_queueing_latency.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_packet_vnet_latency
        .name(name() + ".average_packet_vnet_latency")
        .flags(statistics::oneline);
    m_avg_packet_vnet_latency =
        m_packet_network_latency / m_packets_received;

    m_avg_packet_vqueue_latency
        .name(name() + ".average_packet_vqueue_latency")
        .flags(statistics::oneline);
    m_avg_packet_vqueue_latency =
        m_packet_queueing_latency / m_packets_received;

    m_avg_packet_network_latency
        .name(name() + ".average_packet_network_latency");
    m_avg_packet_network_latency =
        sum(m_packet_network_latency) / sum(m_packets_received);

    m_avg_packet_queueing_latency
        .name(name() + ".average_packet_queueing_latency");
    m_avg_packet_queueing_latency
        = sum(m_packet_queueing_latency) / sum(m_packets_received);

    m_avg_packet_latency
        .name(name() + ".average_packet_latency");
    m_avg_packet_latency
        = m_avg_packet_network_latency + m_avg_packet_queueing_latency;

    // Flits
    m_flits_received
        .init(m_virtual_networks)
        .name(name() + ".flits_received")
        .flags(statistics::pdf | statistics::total | statistics::nozero |
            statistics::oneline)
        ;

    m_flits_injected
        .init(m_virtual_networks)
        .name(name() + ".flits_injected")
        .flags(statistics::pdf | statistics::total | statistics::nozero |
            statistics::oneline)
        ;

    m_flit_network_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_network_latency")
        .flags(statistics::oneline)
        ;

    m_flit_queueing_latency
        .init(m_virtual_networks)
        .name(name() + ".flit_queueing_latency")
        .flags(statistics::oneline)
        ;

    for (int i = 0; i < m_virtual_networks; i++) {
        m_flits_received.subname(i, csprintf("vnet-%i", i));
        m_flits_injected.subname(i, csprintf("vnet-%i", i));
        m_flit_network_latency.subname(i, csprintf("vnet-%i", i));
        m_flit_queueing_latency.subname(i, csprintf("vnet-%i", i));
    }

    m_avg_flit_vnet_latency
        .name(name() + ".average_flit_vnet_latency")
        .flags(statistics::oneline);
    m_avg_flit_vnet_latency = m_flit_network_latency / m_flits_received;

    m_avg_flit_vqueue_latency
        .name(name() + ".average_flit_vqueue_latency")
        .flags(statistics::oneline);
    m_avg_flit_vqueue_latency =
        m_flit_queueing_latency / m_flits_received;

    m_avg_flit_network_latency
        .name(name() + ".average_flit_network_latency");
    m_avg_flit_network_latency =
        sum(m_flit_network_latency) / sum(m_flits_received);

    m_avg_flit_queueing_latency
        .name(name() + ".average_flit_queueing_latency");
    m_avg_flit_queueing_latency =
        sum(m_flit_queueing_latency) / sum(m_flits_received);

    m_avg_flit_latency
        .name(name() + ".average_flit_latency");
    m_avg_flit_latency =
        m_avg_flit_network_latency + m_avg_flit_queueing_latency;


    // Hops (avg_hops= total_hops/total_received_flits)
    m_avg_hops.name(name() + ".average_hops");
    m_avg_hops = m_total_hops / sum(m_flits_received);

    // Links
    m_total_ext_in_link_utilization
        .name(name() + ".ext_in_link_utilization");
    m_total_ext_out_link_utilization
        .name(name() + ".ext_out_link_utilization");
    //==================================================
    m_total_bus_in_link_utilization
        .name(name() + ".bus_in_link_utilization");
    m_total_bus_out_link_utilization
        .name(name() + ".bus_out_link_utilization");
    //==================================================
    m_total_int_link_utilization
        .name(name() + ".int_link_utilization");
    m_average_link_utilization
        .name(name() + ".avg_link_utilization");
    m_average_ext_link_utilization
        .name(name() + ".avg_ext_link_utilization");
    m_average_int_link_utilization
        .name(name() + ".avg_int_link_utilization");
    m_average_vc_load
        .init(m_virtual_networks * m_max_vcs_per_vnet)
        .name(name() + ".avg_vc_load")
        .flags(statistics::pdf | statistics::total | statistics::nozero |
            statistics::oneline)
        ;

    // Traffic distribution
    for (int source = 0; source < m_routers.size(); ++source) {
        m_data_traffic_distribution.push_back(
            std::vector<statistics::Scalar *>());
        m_ctrl_traffic_distribution.push_back(
            std::vector<statistics::Scalar *>());

        for (int dest = 0; dest < m_routers.size(); ++dest) {
            statistics::Scalar *data_packets = new statistics::Scalar();
            statistics::Scalar *ctrl_packets = new statistics::Scalar();

            data_packets->name(name() + ".data_traffic_distribution." + "n" +
                    std::to_string(source) + "." + "n" + std::to_string(dest));
            m_data_traffic_distribution[source].push_back(data_packets);

            ctrl_packets->name(name() + ".ctrl_traffic_distribution." + "n" +
                    std::to_string(source) + "." + "n" + std::to_string(dest));
            m_ctrl_traffic_distribution[source].push_back(ctrl_packets);
        }
    }
}

// Function for updating statistics of OnyxNetwork in stats.txt
void
OnyxNetwork::collateStats()
{
    // get the ruby_system
    RubySystem *rs = params().ruby_system;
    // the time that has passed from the start of the simulation
    double time_delta = double(curCycle() - rs->getStartCycle());

    int ext_links_count = 0;
    int int_links_count = 0;
    int total_extlink_activity = 0;
    //============================================
    int bus_links_count = 0;
    int total_buslink_activity = 0;
    //============================================
    int total_intlink_activity = 0;
    // for every network link
    for (int i = 0; i < m_networklinks.size(); i++) {
        link_type type = m_networklinks[i]->getType();
        int activity = m_networklinks[i]->getLinkUtilization();

        // update link utilization stats
        if (type == EXT_IN_) {
            m_total_ext_in_link_utilization += activity;
            total_extlink_activity += activity;
            ext_links_count++;
        }
        else if (type == EXT_OUT_) {
            m_total_ext_out_link_utilization += activity;
            total_extlink_activity += activity;
            ext_links_count++;
        }
        else if (type == BUS_IN_) {
            m_total_bus_in_link_utilization += activity;
            total_buslink_activity += activity;
            bus_links_count++;
        }
        else if (type == BUS_OUT_) {
            m_total_bus_out_link_utilization += activity;
            total_buslink_activity += activity;
            bus_links_count++;
        }
        else if (type == INT_) {
            m_total_int_link_utilization += activity;
            total_intlink_activity += activity;
            int_links_count++;
        }

        //utilization of links over time
        m_average_link_utilization +=
            (double(activity) / time_delta);

        // update the vc_load stats for vcs
        std::vector<unsigned int> vc_load = m_networklinks[i]->getVcLoad();
        for (int j = 0; j < vc_load.size(); j++) {
            // average vc load for a vc over time
            m_average_vc_load[j] += ((double)vc_load[j] / time_delta);
        }
    }

    //Average utilization of an external link
    m_average_ext_link_utilization = total_extlink_activity/ext_links_count;
    //Average utilization of an internal link
    m_average_int_link_utilization = total_intlink_activity/int_links_count;
    //====================================================================
    //Average utilization of an internal link
    m_average_bus_link_utilization = total_buslink_activity/bus_links_count;
    //====================================================================

    // Ask the routers to collate their statistics
    for (int i = 0; i < m_routers.size(); i++) {
        m_routers[i]->collateStats();
    }
    //===================================================
    // Ask the busses to collate their statistics
    for (int i = 0; i < m_busses.size(); i++) {
        m_busses[i]->collateStats();
    }
    //===================================================
}

// Reset statistics for routers, network links, and credit links
// in OnyxNetwork
void
OnyxNetwork::resetStats()
{
    for (int i = 0; i < m_routers.size(); i++) {
        m_routers[i]->resetStats();
    }
    //==============================================
    for (int i = 0; i < m_busses.size(); i++) {
        m_busses[i]->resetStats();
    }
    //==============================================
    for (int i = 0; i < m_networklinks.size(); i++) {
        m_networklinks[i]->resetStats();
    }
    for (int i = 0; i < m_creditlinks.size(); i++) {
        m_creditlinks[i]->resetStats();
    }
}

// for printing the OnyxNetwork object
void
OnyxNetwork::print(std::ostream& out) const
{
    out << "[OnyxNetwork]";
}

// Keep track of the data traffic and control traffic
void
OnyxNetwork::update_traffic_distribution(RouteInfo route)
{
    int src_node = route.src_router; // source router
    int dest_node = route.dest_router; // destination router
    int vnet = route.vnet; // Vnet in which the traffic goes

    // the type of the Vnet is either DATA or Cntrl
    if (m_vnet_type[vnet] == DATA_VNET_)
        (*m_data_traffic_distribution[src_node][dest_node])++;
    else
        (*m_ctrl_traffic_distribution[src_node][dest_node])++;
}

// Function for performing a functional read. The return value
// indicates the number of messages that were read.
bool
OnyxNetwork::functionalRead(Packet *pkt, WriteMask &mask)
{
    bool read = false;
    for (unsigned int i = 0; i < m_routers.size(); i++) {
        if (m_routers[i]->functionalRead(pkt, mask))
            read = true;
    }

    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    for (unsigned int i = 0; i < m_busses.size(); i++) {
        if (m_busses[i]->functionalRead(pkt, mask))
            read = true;
    }
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

    for (unsigned int i = 0; i < m_nis.size(); ++i) {
        if (m_nis[i]->functionalRead(pkt, mask))
            read = true;
    }

    for (unsigned int i = 0; i < m_networklinks.size(); ++i) {
        if (m_networklinks[i]->functionalRead(pkt, mask))
            read = true;
    }

    for (unsigned int i = 0; i < m_networkbridges.size(); ++i) {
        if (m_networkbridges[i]->functionalRead(pkt, mask))
            read = true;
    }

    return read;
}

// Function for performing a functional write. The return value
// indicates the number of messages that were written.
uint32_t
OnyxNetwork::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;

    for (unsigned int i = 0; i < m_routers.size(); i++) {
        num_functional_writes += m_routers[i]->functionalWrite(pkt);
    }

    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
    for (unsigned int i = 0; i < m_busses.size(); i++) {
        num_functional_writes += m_busses[i]->functionalWrite(pkt);
    }
    //&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&

    for (unsigned int i = 0; i < m_nis.size(); ++i) {
        num_functional_writes += m_nis[i]->functionalWrite(pkt);
    }

    for (unsigned int i = 0; i < m_networklinks.size(); ++i) {
        num_functional_writes += m_networklinks[i]->functionalWrite(pkt);
    }

    return num_functional_writes;
}

} // namespace onyx
} // namespace ruby
} // namespace gem5
