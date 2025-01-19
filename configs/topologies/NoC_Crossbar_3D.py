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


class NoC_Crossbar_3D(SimpleTopology):
    description = "NoC_Crossbar_3D"

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a special 3D Mesh
    # assuming an equal number of cache and directory cntrls
    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes
        num_rows = options.mesh_rows  # number of rows in 3D mesh
        num_columns = options.mesh_columns  # number of columns in 3D mesh
        num_layers = options.mesh_layers  # number of layers in 3D mesh
        # ******************************************************
        # Adding some extra middle routers (16 for 64 main routers)
        # These extra routers connect routers in different layers
        num_routers = options.num_cpus + (num_rows * num_columns)
        num_routers_main = options.num_cpus  # routers with CPUs
        # ******************************************************

        # default values for link latency and router latency.
        # Can be over-ridden on a per link/router basis
        link_latency = options.link_latency  # used by simple and garnet
        router_latency = options.router_latency  # only used by garnet

        # There must be an evenly divisible number of cntrls to routers
        # Also, obviously the number or rows must be <= the number of routers
        # ******************************************************
        cntrls_per_router, remainder = divmod(len(nodes), num_routers_main)
        assert num_rows > 0 and num_rows <= num_routers_main
        assert num_rows * num_columns * num_layers == num_routers_main
        # ******************************************************

        # Create the routers in the mesh
        routers = [
            Router(router_id=i, latency=router_latency)
            for i in range(num_routers)
        ]
        network.routers = routers  # Add the routers to the network

        # link counter to set unique link ids
        link_count = 0

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
            cntrl_level, router_id = divmod(i, num_routers_main)
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

        # connect routers in adjacent layers with internal links and
        # also update link_count to its latest value
        link_count = self.connectInterLayerLinks(
            num_layers,
            num_rows,
            num_columns,
            IntLink,
            int_links,
            link_count,
            link_latency,
            routers,
        )

        network.int_links = int_links  # Add the internal links to the network

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
                    south_in = col + (row * num_columns)
                    # id of the down router
                    north_out = col + ((row + 1) * num_columns)
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
                    south_out = col + (row * num_columns)
                    # id of the down router
                    north_in = col + ((row + 1) * num_columns)
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
        int_links,
        IntLink,
        link_latency,
        link_count,
        layer_number,
        router_one,
        router_two,
    ):
        # Add the internal link from router_one to router_two
        int_links.append(
            IntLink(
                link_id=link_count,
                src_node=router_one,
                dst_node=router_two,
                src_outport="Up" + str(layer_number),
                dst_inport="Down" + str(layer_number),
                latency=link_latency,
                weight=3,
            )
        )

        # Add the internal link from router_two to router_one
        int_links.append(
            IntLink(
                link_id=link_count,
                src_node=router_two,
                dst_node=router_one,
                src_outport="Down" + str(layer_number),
                dst_inport="Up" + str(layer_number),
                latency=link_latency,
                weight=3,
            )
        )

    # For connecting routers in two adjacent layers with internal links.
    # The internal links act as vertical links or TSVs.
    def connectInterLayerLinks(
        self,
        num_layers,
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
        # total number of routers with directories (CPUs or NIs)
        num_routers_main = num_rows * num_columns * num_layers

        for i in range(num_routers_layer):
            for j in range(num_layers):
                self.connect_routers(
                    int_links,
                    IntLink,
                    link_latency,
                    link_count,
                    j,
                    routers[i + (j * num_routers_layer)],
                    routers[num_routers_main + i],
                )
                link_count += 2

        return link_count

    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node(
                [i], MemorySize(options.mem_size) // options.num_cpus, i
            )
