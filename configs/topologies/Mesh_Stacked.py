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
# XYZ routing is enforced (using link weights)
# to guarantee deadlock freedom.


class Mesh_Stacked(SimpleTopology):
    description = "Mesh_Stacked"

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a generic mesh
    # assuming an equal number of cache and directory cntrls

    # def makeTopology(self, options, network, IntLink, ExtLink, Router):
    # ========================================================================
    def makeTopology(
        self,
        options,
        network,
        IntLink,
        BusToRouterLink,
        RouterToBusLink,
        ExtLink,
        Router,
        Bus,
    ):
        # ========================================================================

        nodes = self.nodes
        num_rows = options.mesh_rows  # number of rows in 3D mesh
        num_columns = options.mesh_columns  # number of columns in 3D mesh
        num_layers = options.mesh_layers  # number of layers in 3D mesh
        # ******************************************************
        # Adding some extra middle routers (16 for 64 main routers)
        # These extra routers connect routers in different layers
        num_routers = options.num_cpus 
        num_busses = num_rows * num_columns
        # ******************************************************

        # default values for link latency and router latency.
        # Can be over-ridden on a per link/router basis
        link_latency = options.link_latency  # used by simple and garnet
        router_latency = options.router_latency  # only used by garnet
        #==============================================================
        bus_latency = 1
        #==============================================================

        # There must be an evenly divisible number of cntrls to routers
        # Also, obviously the number or rows must be <= the number of routers
        # ******************************************************
        cntrls_per_router, remainder = divmod(len(nodes), num_routers)
        assert num_rows > 0 and num_rows <= num_routers
        assert num_rows * num_columns * num_layers == num_routers
        # ******************************************************

        # Create the routers in the mesh
        routers = [
            Router(router_id=i, latency=router_latency)
            for i in range(num_routers)
        ]
        network.routers = routers  # Add the routers to the network

        #&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&
        # Create the busses
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
        # distributed across the network.
        network_nodes = []
        remainder_nodes = []
        for node_index in range(len(nodes)):
            if node_index < (len(nodes) - remainder):
                network_nodes.append(nodes[node_index])
            else:
                remainder_nodes.append(nodes[node_index])

        # Connect each node to the appropriate router
        ext_links = []
        for i, n in enumerate(network_nodes):
            # For each cache find its level, and id
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

        # Create the 3D mesh links.
        int_links = []
        #====================================
        bus_to_router_links = []
        router_to_bus_links = []
        #====================================

        # connect routers in each layer with internal links and
        # also update link_count to its latest value
        for layer in range(num_layers):
            link_count = self.connectIntraLayerLinks(
                layer,
                num_rows,
                num_columns,
                IntLink,
                int_links,
                link_count,
                link_latency,
                routers,
            )

        # connect routers in different layers via Bus
        self.connectInterLayerLinks(
            num_layers,
            num_rows,
            num_columns,
            BusToRouterLink,
            RouterToBusLink,
            bus_to_router_links,
            router_to_bus_links,
            bus_to_router_count,
            router_to_bus_count,
            link_latency,
            routers,
            busses
        )

        network.int_links = int_links  # Add the internal links to the network
        # ================================================================
        # Add the bus to router links to the network
        network.bus_to_router_links = bus_to_router_links  
        # Add the router to bus links to the network
        network.router_to_bus_links = router_to_bus_links 
        # ================================================================

    # For connecting the routers via internal links
    # in each layer.
    def connectIntraLayerLinks(
        self,
        layer,
        num_rows,
        num_columns,
        IntLink,
        int_links,
        link_count,
        link_latency,
        routers,
    ):
        # number of routers in one layer
        num_routers_layer = num_rows * num_columns

        # East outport to West inport links (weight = 1)
        # It means link from east output port of the left router
        # to the west input port of the right router.
        for row in range(num_rows):
            for col in range(num_columns):
                if col + 1 < num_columns:
                    # id of the left router
                    east_out = col + (row * num_columns)
                    # id of the right router
                    west_in = (col + 1) + (row * num_columns)
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[
                                east_out + (layer * num_routers_layer)
                            ],
                            dst_node=routers[
                                west_in + (layer * num_routers_layer)
                            ],
                            src_outport="East",
                            dst_inport="West",
                            latency=link_latency,
                            weight=1,
                        )
                    )
                    link_count += 1

        # West outport to East inport links (weight = 1)
        # It means link from west output port of the right router
        # to the east input port of the left router.
        for row in range(num_rows):
            for col in range(num_columns):
                if col + 1 < num_columns:
                    # id of the left router
                    east_in = col + (row * num_columns)
                    # id of the right router
                    west_out = (col + 1) + (row * num_columns)
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[
                                west_out + (layer * num_routers_layer)
                            ],
                            dst_node=routers[
                                east_in + (layer * num_routers_layer)
                            ],
                            src_outport="West",
                            dst_inport="East",
                            latency=link_latency,
                            weight=1,
                        )
                    )
                    link_count += 1

        # North outport to South inport links (weight = 2)
        # It means link from north output port of the down router
        # to the south input port of the up router.
        for col in range(num_columns):
            for row in range(num_rows):
                if row + 1 < num_rows:
                    # id of the up router
                    south_in = col + ((row + 1) * num_columns)
                    # id of the down router
                    north_out = col + (row * num_columns)
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[
                                north_out + (layer * num_routers_layer)
                            ],
                            dst_node=routers[
                                south_in + (layer * num_routers_layer)
                            ],
                            src_outport="North",
                            dst_inport="South",
                            latency=link_latency,
                            weight=2,
                        )
                    )
                    link_count += 1

        # South outport to North inport links (weight = 2)
        # It means link from south output port of the up router
        # to the north input port of the down router.
        for col in range(num_columns):
            for row in range(num_rows):
                if row + 1 < num_rows:
                    # id of the up router
                    south_out = col + ((row + 1) * num_columns)
                    # id of the down router
                    north_in = col + (row * num_columns)
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[
                                south_out + (layer * num_routers_layer)
                            ],
                            dst_node=routers[
                                north_in + (layer * num_routers_layer)
                            ],
                            src_outport="South",
                            dst_inport="North",
                            latency=link_latency,
                            weight=2,
                        )
                    )
                    link_count += 1

        return link_count

    # For connecting two routers via an IntLink
    def connect_routers(
        self,
        bus_to_router_links,
        router_to_bus_links,
        BusToRouterLink,
        RouterToBusLink,
        link_latency,
        bus_to_router_count,
        router_to_bus_count,
        layer_number,
        router,
        bus
    ):
        # Add the bus to router link from bus to router
        bus_to_router_links.append(
            BusToRouterLink(
                link_id=bus_to_router_count,
                src_node=bus,
                dst_node=router,
                src_outport="Down" + str(layer_number),
                dst_inport="Up",
                latency=link_latency,
                weight=3,
            )
        )

        # Add the router to bus link from router to bus
        router_to_bus_links.append(
            RouterToBusLink(
                link_id=router_to_bus_count,
                src_node=router,
                dst_node=bus,
                src_outport="Up",
                dst_inport="Down" + str(layer_number),
                latency=link_latency,
                weight=3,
            )
        )

    # For connecting routers in different layers via Bus.
    def connectInterLayerLinks(
        self,
        num_layers,
        num_rows,
        num_columns,
        BusToRouterLink,
        RouterToBusLink,
        bus_to_router_links,
        router_to_bus_links,
        bus_to_router_count,
        router_to_bus_count,
        link_latency,
        routers,
        busses
    ):
        # number of routers in one layer
        num_routers_layer = num_rows * num_columns

        for i in range(num_routers_layer):
            for j in range(num_layers):
                self.connect_routers(
                    bus_to_router_links,
                    router_to_bus_links,
                    BusToRouterLink,
                    RouterToBusLink,
                    link_latency,
                    bus_to_router_count,
                    router_to_bus_count,
                    j,
                    routers[i + (j * num_routers_layer)],
                    busses[i],
                )
                bus_to_router_count += 1
                router_to_bus_count += 1



    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node(
                [i], MemorySize(options.mem_size) // options.num_cpus, i
            )
