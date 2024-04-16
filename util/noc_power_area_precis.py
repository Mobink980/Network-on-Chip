import re
import sys
import numpy as np

# a list containing the total dynamic power of all routers
routers_dynamic_power = []
# a list containing the total leakage power of all routers
routers_leakage_power = []
#=============================================================
# a list containing the buffer area of all routers
routers_buffer_area = []
# a list containing the crossbar area of all routers
routers_crossbar_area = []
# a list containing the switch allocator area of all routers
routers_sa_area = []
# a list containing the other areas of all routers
routers_other_area = []
#=============================================================
# a list containing the dynamic power of all internal links
intlinks_dynamic_power = []
# a list containing the leakage power of all intenal links
intlinks_leakage_power = []
#=============================================================
# a list containing the dynamic power of all external links
extlinks_dynamic_power = []
# a list containing the leakage power of all external links
extlinks_leakage_power = []


#open and read the file 
f = open("m5out/power_detailed.txt", "r")
# print(f.read())

# read all content from a file using read()
lines = f.readlines()

count = 0
for line in lines:
    count += 1
    if re.match("Total Power:", line):
        dynamic_power_line = lines[count]
        leakage_power_line = lines[count + 1]
        routers_dynamic_power.append(float(dynamic_power_line.split()[3]))
        routers_leakage_power.append(float(leakage_power_line.split()[4]))

count = 0
for line in lines:
    count += 1
    if re.match("Area:", line):
        buffer_area_line = lines[count]
        crossbar_area_line = lines[count + 1]
        sa_area_line = lines[count + 2]
        other_area_line = lines[count + 3]
        routers_buffer_area.append(float(buffer_area_line.split()[2]))
        routers_crossbar_area.append(float(crossbar_area_line.split()[2]))
        routers_sa_area.append(float(sa_area_line.split()[3]))
        routers_other_area.append(float(other_area_line.split()[2]))

count = 0
for line in lines:
    count += 1
    if re.match("Internal Link", line):
        dynamic_power_line = lines[count + 3]
        leakage_power_line = lines[count + 4]
        intlinks_dynamic_power.append(float(dynamic_power_line.split()[3]))
        intlinks_leakage_power.append(float(leakage_power_line.split()[3]))
     
count = 0
for line in lines:
    count += 1
    if re.match("External Link", line):
        dynamic_power_line = lines[count + 3]
        leakage_power_line = lines[count + 4]
        extlinks_dynamic_power.append(float(dynamic_power_line.split()[3]))
        extlinks_leakage_power.append(float(leakage_power_line.split()[3]))


# close the file
f.close()

# a = np.array(routers_buffer_area)
# print(routers_buffer_area)
# print(a.sum())


total_router_dynamic_power = np.array(routers_dynamic_power).sum()
total_router_leakage_power = np.array(routers_leakage_power).sum()

total_router_buffer_area = np.array(routers_buffer_area).sum()
total_router_crossbar_area = np.array(routers_crossbar_area).sum()
total_router_sa_area = np.array(routers_sa_area).sum()
total_router_other_area = np.array(routers_other_area).sum()

total_intLink_dynamic_power = np.array(intlinks_dynamic_power).sum()
total_intLink_leakage_power = np.array(intlinks_leakage_power).sum()

total_extLink_dynamic_power = np.array(extlinks_dynamic_power).sum()
total_extLink_leakage_power = np.array(extlinks_leakage_power).sum()

# Network in whole
network_dynamic_power = total_router_dynamic_power + total_intLink_dynamic_power \
                        + total_extLink_dynamic_power

network_leakage_power = total_router_leakage_power + total_intLink_leakage_power \
                        + total_extLink_leakage_power

network_power = network_dynamic_power + network_leakage_power

network_area = total_router_buffer_area + total_router_crossbar_area + \
               total_router_sa_area + total_router_other_area

# create the txt file in the current directory
f = open("m5out/power_summary.txt", "w") 
# print the content into the text file instead of cmd
sys.stdout = f
print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("@@@@@@@@@@@@@ Router Power and Area @@@@@@@@@@@@@@@")
print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("============================================")
print("Dynamic power of all the routers:")
print("============================================")
print(total_router_dynamic_power, "W")
print()
print("============================================")
print("Leakage power of all the routers:")
print("============================================")
print(total_router_leakage_power, "W")
print()
print("============================================")
print("Buffer area of all the routers:")
print("============================================")
print(total_router_buffer_area, "m^2")
print()
print("============================================")
print("Crossbar area of all the routers:")
print("============================================")
print(total_router_crossbar_area, "m^2")
print()
print("============================================")
print("Switch Allocator area of all the routers:")
print("============================================")
print(total_router_sa_area, "m^2")
print()
print("============================================")
print("Other areas of all the routers:")
print("============================================")
print(total_router_other_area, "m^2")
print()

print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("@@@@@@@@@@@@@@ Internal Links Power @@@@@@@@@@@@@@@")
print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("============================================")
print("Dynamic power of all the internal links:")
print("============================================")
print(total_intLink_dynamic_power, "W")
print()
print("============================================")
print("Leakage power of all the internal links:")
print("============================================")
print(total_intLink_leakage_power, "W")
print()

print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("@@@@@@@@@@@@@@ External Links Power @@@@@@@@@@@@@@@")
print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("============================================")
print("Dynamic power of all the external links:")
print("============================================")
print(total_extLink_dynamic_power, "W")
print()
print("============================================")
print("Leakage power of all the external links:")
print("============================================")
print(total_extLink_leakage_power, "W")
print()

print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("@@@@@@@@@@@@@ Power & Area of the NoC @@@@@@@@@@@@@")
print("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@")
print("============================================")
print("Network-on-Chip Dynamic Power:")
print("============================================")
print(network_dynamic_power, "W")
print()
print("============================================")
print("Network-on-Chip Leakage Power:")
print("============================================")
print(network_leakage_power, "W")
print()
print("============================================")
print("Network-on-Chip Power (leakage + dynamic):")
print("============================================")
print(network_power, "W")
print()
print("============================================")
print("Network-on-Chip Area:")
print("============================================")
print(network_area, "m^2")
print()

# again, switch to cmd for printing
sys.stdout = sys.__stdout__
# close the file
f.close()
print("A summary of power and area was generated in the m5out directory.")


