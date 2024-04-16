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

from m5.params import *
from m5.objects import *
from common import FileSystemConfig
from topologies.BaseTopology import SimpleTopology
import networkx as nx
import random

# Creates a generic small-world assuming an equal number of cache
# and directory controllers.
# XY routing is enforced (using link weights)
# to guarantee deadlock freedom.

class SWNoC_3D(SimpleTopology):
    description='SWNoC_3D'

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a generic small-world
    # assuming an equal number of cache and directory cntrls

    def makeTopology(self, options, network, IntLink, ExtLink, Router):

        nodes = self.nodes # controllers
        # getting num_cpus from the commandline (some options have default values)
        num_routers = options.num_cpus # number of routers and cpus are equal
        num_rows = options.mesh_rows # getting the small-world rows from the commandline
        num_columns = options.mesh_columns # number of columns in 3D small-world
        num_layers = options.mesh_layers # number of layers in 3D small-world

        # default values for link latency and router latency.
        # Can be over-ridden on a per link/router basis
        link_latency = options.link_latency # used by simple and garnet
        router_latency = options.router_latency # only used by garnet
        
        # There must be an evenly divisible number of cntrls to routers
        # Also, obviously the number or rows must be <= the number of routers
        # The following divides the number of controllers by number of routers
        # to see if any controller remains (not evenly divisible)
        cntrls_per_router, remainder = divmod(len(nodes), num_routers)
        assert(num_rows > 0 and num_rows <= num_routers)
        assert(num_columns * num_rows * num_layers == num_routers)

        # Create the routers in the small-world (here all routers have the same latency)
        routers = [Router(router_id=i, latency = router_latency) \
            for i in range(num_routers)]
        network.routers = routers # Add the created routers to the network

        # link counter to set unique link ids
        link_count = 0

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
        for (i, n) in enumerate(network_nodes):
            # For each controller find its level, and id 
            # of the router to connect to. 
            cntrl_level, router_id = divmod(i, num_routers)
            assert(cntrl_level < cntrls_per_router) # ex: 0,1,2<3
            ext_links.append(ExtLink(link_id=link_count, ext_node=n,
                                    int_node=routers[router_id],
                                    latency = link_latency))
            link_count += 1

        # Connect the remainding nodes to router 0.  These should only be
        # DMA nodes.
        for (i, node) in enumerate(remainder_nodes):
            assert(node.type == 'DMA_Controller')
            assert(i < remainder)
            ext_links.append(ExtLink(link_id=link_count, ext_node=node,
                                    int_node=routers[0],
                                    latency = link_latency))
            link_count += 1

        network.ext_links = ext_links # Add the external links to the network

        # Create the 3D small-world links.
        int_links = []
        # connect routers in each layer with internal links and 
        # also update link_count to its latest value
        for layer in range(num_layers):
            link_count = self.connectIntraLayerLinks(layer, num_rows, num_columns, 
                                                         IntLink, int_links, link_count, 
                                                         link_latency, routers)

        # connect routers in adjacent layers with internal links and 
        # also update link_count to its latest value
        link_count = self.connectInterLayerLinks(num_layers, num_rows, num_columns,
                                                     IntLink, int_links, link_count,
                                                     link_latency, routers)

        network.int_links = int_links # Add the internal links to the network


    # For connecting the routers via internal links
    # in each layer.
    def connectIntraLayerLinks(self, layer, num_rows, num_columns, IntLink, 
                                        int_links, link_count, link_latency, routers):
        # number of routers in one layer
        num_routers_layer = num_rows * num_columns
        # so always one network is generated for a probability
        random.seed(246)
        # Use the networkx library to create a small-world network 
        # n = number of nodes
        # k = neighbors connected to each node
        # p = rewiring probability  
        G = nx.watts_strogatz_graph(n = num_routers_layer, k = 4, p=0.15)
        my_edges = list(G.edges)

        for i in range(len(my_edges)):
            first_node = my_edges[i][0]
            second_node = my_edges[i][1]

            int_links.append(IntLink(link_id=link_count,
                                    src_node=routers[first_node + (layer * num_routers_layer)],
                                    dst_node=routers[second_node + (layer * num_routers_layer)],
                                    src_outport="East" + str(i),
                                    dst_inport= "East" + str(i),
                                    latency = link_latency,
                                    weight=1))
            link_count += 1

            int_links.append(IntLink(link_id=link_count,
                                    src_node=routers[second_node + (layer * num_routers_layer)],
                                    dst_node=routers[first_node + (layer * num_routers_layer)],
                                    src_outport="East" + str(i),
                                    dst_inport= "East" + str(i),
                                    latency = link_latency,
                                    weight=1))
            link_count += 1        

        return link_count


    # For connecting routers in two adjacent layers with internal links.
    # The internal links act as vertical links or TSVs.
    def connectInterLayerLinks(self, num_layers, num_rows, num_columns, IntLink, 
                                        int_links, link_count, link_latency, routers):
        # number of routers in one layer
        num_routers_layer = num_rows * num_columns

        #Up outport to Down inport (upward links)
        for layer in range(num_layers - 1):
            for row in range(num_rows):
                for col in range(num_columns):
                    # id of the layer(n) router
                    up_out = (col + (layer * num_routers_layer)) + \
                                                    (row * num_columns)
                    # id of the layer(n+1) router
                    down_in = (col + ((layer + 1) * num_routers_layer)) + \
                                                    (row * num_columns)
                    int_links.append(IntLink(link_id = link_count,
                                             src_node = routers[up_out],
                                             dst_node = routers[down_in],
                                             src_outport = "Up",
                                             dst_inport = "Down",
                                             latency = link_latency,
                                             weight = 3))
                    link_count += 1


        #Down outport to Up inport (downward links)
        for layer in range(num_layers - 1):
            for row in range(num_rows):
                for col in range(num_columns):
                    # id of the layer(n) router
                    up_in = (col + (layer * num_routers_layer)) + \
                                                    (row * num_columns)
                    # id of the layer(n+1) router
                    down_out = (col + ((layer + 1) * num_routers_layer)) + \
                                                    (row * num_columns)
                    int_links.append(IntLink(link_id = link_count,
                                             src_node = routers[down_out],
                                             dst_node = routers[up_in],
                                             src_outport = "Down",
                                             dst_inport = "Up",
                                             latency = link_latency,
                                             weight = 3))
                    link_count += 1

        return link_count


    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node([i],
                    MemorySize(options.mem_size) // options.num_cpus, i)
