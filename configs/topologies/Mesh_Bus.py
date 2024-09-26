# Copyright (c) 2010 Advanced Micro Devices, Inc.
#               2016 Georgia Institute of Technology
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from common import FileSystemConfig
from topologies.BaseTopology import SimpleTopology

from m5.objects import *
from m5.params import *

# Creates a generic Mesh assuming an equal number of cache
# and directory controllers.
# XY routing is enforced (using link weights)
# to guarantee deadlock freedom.


class Mesh_Bus(SimpleTopology):
    description = "Mesh_Bus"

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a generic mesh
    # assuming an equal number of cache and directory cntrls
    def makeTopology(self, options, network, IntLink, ExtLink, BusLink, Router, Bus):
        nodes = self.nodes  # controllers
        # getting num_cpus from the commandline (some options have default values)
        num_routers = options.num_cpus  # number of routers and cpus are equal
        num_rows = (
            options.mesh_rows
        )  # getting the mesh rows from the commandline

        # default values for link latency and router latency.
        # Can be over-ridden on a per link/router basis
        link_latency = options.link_latency  # used by simple and garnet
        router_latency = options.router_latency  # only used by garnet

        #==============================================================
        bus_latency = 1
        #==============================================================

        # There must be an evenly divisible number of cntrls to routers
        # Also, obviously the number or rows must be <= the number of routers
        # The following divides the number of controllers by number of routers
        # to see if any controller remains (not evenly divisible)
        cntrls_per_router, remainder = divmod(len(nodes), num_routers)
        assert num_rows > 0 and num_rows <= num_routers
        # This is why we don't have mesh_cols parameter for mesh.
        # Number of columns is calculated automatically.
        num_columns = int(num_routers / num_rows)  # Ex: 16/4=4
        assert num_columns * num_rows == num_routers

        # Create the routers in the mesh (here all routers have the same latency)
        routers = [
            Router(router_id=i, latency=router_latency)
            for i in range(num_routers)
        ]
        network.routers = routers  # Add the created routers to the network

        #&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
        # Create the busses
        num_busses = 1
        busses = [
            Bus(bus_id=i, latency=bus_latency)
            for i in range(num_busses)
        ]
        network.busses = busses  # Add the created busses to the network
        #&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&


        # link counter to set unique link ids
        link_count = 0
        # ========================================
        bus_to_router_count = 0
        router_to_bus_count = 0
        # ========================================

        # Add all but the remainder nodes to the list of nodes to be uniformly
        # distributed across the network. We're not adding the remaining cntrls
        # to the network now. We add them as DMA controllers to router 0 later.
        network_nodes = []
        remainder_nodes = []
        for node_index in range(len(nodes)):
            if node_index < (len(nodes) - remainder):
                network_nodes.append(nodes[node_index])
            else:
                remainder_nodes.append(nodes[node_index])

        # Connect each node to the appropriate router (ExternalLinks)
        # We have several level of cntrls (if number of cntrls > num_routers)
        ext_links = []
        for i, n in enumerate(network_nodes):
            # For each controller find its level, and id
            # of the router to connect to.
            cntrl_level, router_id = divmod(i, num_routers)
            assert cntrl_level < cntrls_per_router  # ex: 0,1,2<3
            ext_links.append(
                ExtLink(
                    link_id=link_count,
                    ext_node=n,
                    int_node=routers[router_id],
                    latency=link_latency,
                )
            )
            link_count += 1

        # Connect the remainding nodes to router 0.  These should only be
        # DMA nodes.
        for i, node in enumerate(remainder_nodes):
            assert node.type == "DMA_Controller"
            assert i < remainder
            ext_links.append(
                ExtLink(
                    link_id=link_count,
                    ext_node=node,
                    int_node=routers[0],
                    latency=link_latency,
                )
            )
            link_count += 1

        network.ext_links = ext_links  # Add the external links to the network

        # Create the 2D mesh links.
        int_links = []
        #====================================
        bus_to_router_links = []
        router_to_bus_links = []
        #====================================


        int_links.append(IntLink(link_id=link_count,
                                    src_node=routers[0],
                                    dst_node=routers[1],
                                    src_outport="East",
                                    dst_inport="West",
                                    latency = link_latency,
                                    weight=2))
        link_count += 1

        # int_links.append(IntLink(link_id=link_count,
        #                             src_node=routers[2],
        #                             dst_node=routers[3],
        #                             src_outport="East",
        #                             dst_inport="West",
        #                             latency = link_latency,
        #                             weight=2))
        # link_count += 1


        int_links.append(IntLink(link_id=link_count,
                                    src_node=routers[1],
                                    dst_node=routers[0],
                                    src_outport="West",
                                    dst_inport="East",
                                    latency = link_latency,
                                    weight=2))
        link_count += 1

        # int_links.append(IntLink(link_id=link_count,
        #                             src_node=routers[3],
        #                             dst_node=routers[2],
        #                             src_outport="West",
        #                             dst_inport="East",
        #                             latency = link_latency,
        #                             weight=2))
        # link_count += 1


        int_links.append(IntLink(link_id=link_count,
                                    src_node=routers[0],
                                    dst_node=routers[2],
                                    src_outport="North",
                                    dst_inport="South",
                                    latency = link_latency,
                                    weight=2))
        link_count += 1

        # int_links.append(IntLink(link_id=link_count,
        #                             src_node=routers[1],
        #                             dst_node=routers[3],
        #                             src_outport="North",
        #                             dst_inport="South",
        #                             latency = link_latency,
        #                             weight=2))
        # link_count += 1


        int_links.append(IntLink(link_id=link_count,
                                    src_node=routers[2],
                                    dst_node=routers[0],
                                    src_outport="South",
                                    dst_inport="North",
                                    latency = link_latency,
                                    weight=2))
        link_count += 1

        # int_links.append(IntLink(link_id=link_count,
        #                             src_node=routers[3],
        #                             dst_node=routers[1],
        #                             src_outport="South",
        #                             dst_inport="North",
        #                             latency = link_latency,
        #                             weight=2))
        # link_count += 1

        #=================================================================
        bus_to_router_links.append(
            BusToRouterLink(
                link_id=bus_to_router_count,
                src_node=busses[0],
                dst_node=routers[2],
                src_outport="West", 
                dst_inport="East",
                latency=link_latency,
                weight=1,
            )
        )
        bus_to_router_count += 1

        router_to_bus_links.append(
            RouterToBusLink(
                link_id=router_to_bus_count,
                src_node=routers[2],
                dst_node=busses[0],
                src_outport="East",
                dst_inport="West",
                latency=link_latency,
                weight=1,
            )
        )
        router_to_bus_count += 1


        bus_to_router_links.append(
            BusToRouterLink(
                link_id=bus_to_router_count,
                src_node=busses[0],
                dst_node=routers[3],
                src_outport="East",
                dst_inport="West",
                latency=link_latency,
                weight=1,
            )
        )
        bus_to_router_count += 1

        router_to_bus_links.append(
            RouterToBusLink(
                link_id=router_to_bus_count,
                src_node=routers[3],
                dst_node=busses[0],
                src_outport="West",
                dst_inport="East",
                latency=link_latency,
                weight=1,
            )
        )
        router_to_bus_count += 1

        #=================================================================

        network.int_links = int_links  # Add the internal links to the network

        # ================================================================
        # Add the bus to router links to the network
        network.bus_to_router_links = bus_to_router_links  
        # Add the router to bus links to the network
        network.router_to_bus_links = router_to_bus_links 
        # ================================================================

    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node(
                [i], MemorySize(options.mem_size) // options.num_cpus, i
            )
