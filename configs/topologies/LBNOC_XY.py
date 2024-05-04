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
import math

# Creates a generic loop-based NoC assuming an equal number of cache
# and directory controllers.
# XY routing is enforced (using link weights)
# to guarantee deadlock freedom.

class LBNOC_XY(SimpleTopology):
    description='LBNOC_XY'

    def __init__(self, controllers):
        self.nodes = controllers

    # Makes a generic loop-based NoC
    # assuming an equal number of cache and directory cntrls

    #========================================================================
    def makeTopology(
        self, options, network, IntLink, 
        BusToRouterLink, RouterToBusLink, ExtLink, 
        Router, Bus
    ):
    #========================================================================

        nodes = self.nodes # controllers
        # getting num_cpus from the commandline (some options have default values)
        num_routers = options.num_cpus # number of routers and cpus are equal
        num_rows = options.mesh_rows # getting the loop-based NoC rows from the commandline

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
   
        # Number of columns is calculated automatically. 
        num_columns = int(num_routers / num_rows) # Ex: 16/4=4
        assert(num_columns * num_rows == num_routers)
        assert(num_columns == num_rows)

        # Create the routers in the loop-based NoC (here all routers have the same latency)
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

        #*******************************************************************
        #********************** FIND LBNOC LOOPS ***************************
        #*******************************************************************
        def findLoops(num_routers):
            """
            Find the loops for an NxN NoC. 
            It takes the number of routers in the network, and 
            returns the loops and their associated directions.
            """
            num_rows = int(math.sqrt(num_routers))
            num_columns = int(math.sqrt(num_routers))

            # at least for routers are required
            assert(num_routers >= 4)
            # for now the topology only supports even number of rows and columns 
            assert(num_rows % 2 == 0 and num_columns % 2 == 0)
            # for now topology only supports same number of rows and columns
            assert(num_rows * num_columns == num_routers)

            # set of all the loops for the network
            loops = []
            # direction of each loop in the network
            loops_direction = []

            # we find the loops from the center router
            center_row = int(num_rows/2) - 1
            center_col = int(num_columns/2) - 1

            # We start from finding the loops of 2x2 NoC 
            # and then move to bigger NoCs. 
            stage_cores = 4

            # for rotation and reversing of loops
            count = 0

            # Keep going until we reach the size of the network.
            while (stage_cores <= num_routers):
                
                # rows and columns of this stage
                stage_rows = int(math.sqrt(stage_cores))
                stage_cols = int(math.sqrt(stage_cores))

                # These two variables help us find the four corners
                # of this stage. 
                left = int(stage_rows/2) - 1
                right = int(stage_cols/2)

                # north_west coordinates (to find north_west corner)
                nw_row = center_row - left
                nw_col = center_col - left
                # north_east coordinates (to find north_east corner)
                ne_row = center_row - left
                ne_col = center_col + right
                # south_west coordinates (to find south_west corner)
                sw_row = center_row + right
                sw_col = center_col - left
                # south_east coordinates (to find south_east corner)
                se_row = center_row + right
                se_col = center_col + right

                # calculating four corners for each stage
                north_west = (nw_row * num_columns) + nw_col
                north_east = (ne_row * num_columns) + ne_col
                south_west = (sw_row * num_columns) + sw_col
                south_east = (se_row * num_columns) + se_col

                # print("north_west:", north_west)
                # print("north_east:", north_east)
                # print("south_west:", south_west)
                # print("south_east:", south_east)

                if stage_cores == 4:
                    # for saving one loop
                    loop = []
                    # We only have two loops for this stage
                    # (one clockwise and the other anticlockwise).
                    loop.append(north_west)
                    loop.append(north_east)
                    loop.append(south_east)
                    loop.append(south_west)
                    loop.append(north_west)
                    loops.append(loop)
                    loops_direction.append("clockwise")
                    loops.append(list(reversed(loop)))
                    loops_direction.append("anticlockwise")
                    stage_cores = stage_cores + (4 * int(math.sqrt(stage_cores))) + 4

                else:
                    # For other stages, we have four groups of loops.
                    #=======================
                    # Group A
                    #=======================
                    # for saving one loop
                    loop = []
                    # traverse from north west to south west
                    for j in range(north_west, south_west, num_columns):
                        loop.append(j)
                    # traverse from south west to south east
                    for i in range(south_west, south_east, 1):
                        loop.append(i)
                    # traverse from south east to north east
                    for j in range(south_east, north_east, -num_columns):
                        loop.append(j)
                    # traverse from north east to north west
                    for i in range(north_east, north_west - 1, -1):
                        loop.append(i)

                    if count % 2 == 1:
                        loops.append(loop)
                        loops_direction.append("anticlockwise")
                    else:
                        loops.append(list(reversed(loop)))
                        loops_direction.append("clockwise")

                    #=======================
                    # Group B
                    #=======================
                    # for saving one loop
                    loop = []
                    if count % 2 == 1:
                        for right_north in range(north_west + 1, north_east, 1):
                            right_south = right_north + ((stage_cols - 1) * num_columns)
                            for i in range(north_west, right_north, 1):
                                loop.append(i)
                            for j in range(right_north, right_south, num_columns):
                                loop.append(j) 
                            for i in range(right_south, south_west, -1):
                                loop.append(i)
                            for j in range(south_west, north_west - 1, -num_columns):
                                loop.append(j)
                            loops.append(loop)
                            loops_direction.append("clockwise")
                            loop = []
                            
                    else: 
                        for down_west in range(north_west + num_columns, south_west, num_columns):
                            down_east = down_west + stage_cols - 1
                            for i in range(north_west, north_east, 1):
                                loop.append(i)
                            for j in range(north_east, down_east, num_columns):
                                loop.append(j) 
                            for i in range(down_east, down_west, -1):
                                loop.append(i)
                            for j in range(down_west, north_west - 1, -num_columns):
                                loop.append(j)
                            loops.append(list(reversed(loop)))
                            loops_direction.append("anticlockwise")
                            loop = []

                    #=======================
                    # Group C
                    #=======================
                    # for saving one loop
                    loop = []
                    if count % 2 == 1:
                        for left_north in range(north_east - 1, north_west, -1):
                            left_south = left_north + ((stage_cols - 1) * num_columns)
                            for i in range(north_east, left_north, -1):
                                loop.append(i)
                            for j in range(left_north, left_south, num_columns):
                                loop.append(j) 
                            for i in range(left_south, south_east, 1):
                                loop.append(i)
                            for j in range(south_east, north_east - 1, -num_columns):
                                loop.append(j)
                            loops.append(list(reversed(loop)))
                            loops_direction.append("clockwise")
                            loop = []   

                    else: 
                        for up_west in range(south_west - num_columns, north_west, -num_columns):
                            up_east = up_west + stage_cols - 1
                            for i in range(south_west, south_east, 1):
                                loop.append(i)
                            for j in range(south_east, up_east, -num_columns):
                                loop.append(j) 
                            for i in range(up_east, up_west, -1):
                                loop.append(i)
                            for j in range(up_west, south_west + 1, num_columns):
                                loop.append(j)
                            loops.append(loop)
                            loops_direction.append("anticlockwise")
                            loop = []

                    #=======================
                    # Group D
                    #=======================
                    # for saving one loop
                    loop = []
                    if count % 2 == 1:
                        for left_north in range(north_west, south_west, num_columns):
                            left_south = left_north + num_columns
                            right_north = left_north + stage_cols - 1
                            right_south = right_north + num_columns
                            for i in range(left_north, right_north + 1, 1):
                                loop.append(i)
                            for i in range(right_south, left_south - 1, -1):
                                loop.append(i)
                            loop.append(left_north)
                            loops.append(loop)
                            loops_direction.append("clockwise")
                            loop = []

                    else: 
                        for left_north in range(north_west, north_east, 1):
                            left_south = left_north + ((stage_cols - 1) * num_columns)
                            right_north = left_north + 1
                            right_south = left_south + 1
                            for j in range(left_north, left_south + 1, num_columns):
                                loop.append(j)
                            for j in range(right_south, right_north - 1, -num_columns):
                                loop.append(j)
                            loop.append(left_north)
                            loops.append(loop)
                            loops_direction.append("anticlockwise")
                            loop = []


                    stage_cores = stage_cores + (4 * int(math.sqrt(stage_cores))) + 4
                    count+=1

            return loops, loops_direction

        def findUnoptimizedLoops(num_routers):
            """
            Find the loops for an NxN NoC. 
            It takes the number of routers in the network, and 
            returns the loops and their associated directions.
            """
            num_rows = int(math.sqrt(num_routers))
            num_columns = int(math.sqrt(num_routers))

            # at least for routers are required
            assert(num_routers >= 4)
            # for now the topology only supports even number of rows and columns 
            assert(num_rows % 2 == 0 and num_columns % 2 == 0)
            # for now topology only supports same number of rows and columns
            assert(num_rows * num_columns == num_routers)

            # set of all the loops for the network
            loops = []
            # direction of each loop in the network
            loops_direction = []

            # we find the loops from the center router
            center_row = int(num_rows/2) - 1
            center_col = int(num_columns/2) - 1

            # We start from finding the loops of 2x2 NoC 
            # and then move to bigger NoCs. 
            stage_cores = 4

            # Keep going until we reach the size of the network.
            while (stage_cores <= num_routers):

                # rows and columns of this stage
                stage_rows = int(math.sqrt(stage_cores))
                stage_cols = int(math.sqrt(stage_cores))

                # These two variables help us find the four corners
                # of this stage. 
                left = int(stage_rows/2) - 1
                right = int(stage_cols/2)

                # north_west coordinates (to find north_west corner)
                nw_row = center_row - left
                nw_col = center_col - left
                # north_east coordinates (to find north_east corner)
                ne_row = center_row - left
                ne_col = center_col + right
                # south_west coordinates (to find south_west corner)
                sw_row = center_row + right
                sw_col = center_col - left
                # south_east coordinates (to find south_east corner)
                se_row = center_row + right
                se_col = center_col + right

                # calculating four corners for each stage
                north_west = (nw_row * num_columns) + nw_col
                north_east = (ne_row * num_columns) + ne_col
                south_west = (sw_row * num_columns) + sw_col
                south_east = (se_row * num_columns) + se_col

                # print("north_west:", north_west)
                # print("north_east:", north_east)
                # print("south_west:", south_west)
                # print("south_east:", south_east)

                if stage_cores == 4:
                    # for saving one loop
                    loop = []
                    # We only have two loops for this stage
                    # (one clockwise and the other anticlockwise).
                    loop.append(north_west)
                    loop.append(north_east)
                    loop.append(south_east)
                    loop.append(south_west)
                    loop.append(north_west)
                    loops.append(loop)
                    loops_direction.append("clockwise")
                    loops.append(list(reversed(loop)))
                    loops_direction.append("anticlockwise")
                    stage_cores = stage_cores + (4 * int(math.sqrt(stage_cores))) + 4

                else:
                    # For other stages, we have four groups of loops.
                    #=======================
                    # Group A
                    #=======================
                    # for saving one loop
                    loop = []
                    # traverse from north west to south west
                    for j in range(north_west, south_west, num_columns):
                        loop.append(j)
                    # traverse from south west to south east
                    for i in range(south_west, south_east, 1):
                        loop.append(i)
                    # traverse from south east to north east
                    for j in range(south_east, north_east, -num_columns):
                        loop.append(j)
                    # traverse from north east to north west
                    for i in range(north_east, north_west - 1, -1):
                        loop.append(i)

                    loops.append(loop)
                    loops_direction.append("anticlockwise")

                    #=======================
                    # Group B
                    #=======================
                    # for saving one loop
                    loop = []
                    for right_north in range(north_west + 1, north_east, 1):
                        right_south = right_north + ((stage_cols - 1) * num_columns)
                        for i in range(north_west, right_north, 1):
                            loop.append(i)
                        for j in range(right_north, right_south, num_columns):
                            loop.append(j) 
                        for i in range(right_south, south_west, -1):
                            loop.append(i)
                        for j in range(south_west, north_west - 1, -num_columns):
                            loop.append(j)
                        loops.append(loop)
                        loops_direction.append("clockwise")
                        loop = []

                    #=======================
                    # Group C
                    #=======================
                    # for saving one loop
                    loop = []
                    for left_north in range(north_east - 1, north_west, -1):
                        left_south = left_north + ((stage_cols - 1) * num_columns)
                        for i in range(north_east, left_north, -1):
                            loop.append(i)
                        for j in range(left_north, left_south, num_columns):
                            loop.append(j) 
                        for i in range(left_south, south_east, 1):
                            loop.append(i)
                        for j in range(south_east, north_east - 1, -num_columns):
                            loop.append(j)
                        loops.append(list(reversed(loop)))
                        loops_direction.append("clockwise")
                        loop = []

                    #=======================
                    # Group D
                    #=======================
                    # for saving one loop
                    loop = []
                    for left_north in range(north_west, south_west, num_columns):
                        left_south = left_north + num_columns
                        right_north = left_north + stage_cols - 1
                        right_south = right_north + num_columns
                        for i in range(left_north, right_north + 1, 1):
                            loop.append(i)
                        for i in range(right_south, left_south - 1, -1):
                            loop.append(i)
                        loop.append(left_north)
                        loops.append(loop)
                        loops_direction.append("clockwise")
                        loop = []

                    stage_cores = stage_cores + (4 * int(math.sqrt(stage_cores))) + 4

            return loops, loops_direction

        #*******************************************************************
        #*******************************************************************
        #*******************************************************************

        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        loops, loops_direction = findLoops(num_routers)
        # loops, loops_direction = findUnoptimizedLoops(num_routers)
        #@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        # Create the LBNOC links.
        int_links = []   

        for i in range(len(loops_direction)):
            loop_num = i # loop number
            loop = loops[i] # loop (a list of nodes)
            direc = loops_direction[i] # loop direction
            src_out = "" 
            dst_in = ""
            if direc == "clockwise":
                src_out = "East" + str(loop_num)
                dst_in = "West" + str(loop_num)
            else:
                src_out = "West" + str(loop_num)
                dst_in = "East" + str(loop_num)

            for k in range(len(loop) - 1):

                int_links.append(IntLink(link_id=link_count,
                                         src_node=routers[loop[k]],
                                         dst_node=routers[loop[k + 1]],
                                         src_outport=src_out,
                                         dst_inport=dst_in,
                                         latency = link_latency,
                                         weight=1,
                                         loop=loop_num,
                                         direction=direc))
                link_count += 1

        network.int_links = int_links # Add the internal links to the network


    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node([i],
                    MemorySize(options.mem_size) // options.num_cpus, i)
