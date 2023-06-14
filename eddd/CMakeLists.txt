# Create a custom library
add_library(
        network-topology-creator
        network-topology-creator.cpp
)

add_library(
        legitimate-traffic-application
        legitimate-traffic-application.cc
)

add_library(
        syn-flood-attacker
        syn-flood-attacker.cc
)

add_library(
        icmp-flood-attacker
        icmp-flood-attacker.cc
)

# Link the custom library to the scratch executable
build_exec(
        EXECNAME eddd
        SOURCE_FILES eddd.cc
        LIBRARIES_TO_LINK network-topology-creator legitimate-traffic-application syn-flood-attacker icmp-flood-attacker
        ${libcore} ${ns3-libs} ${ns3-contrib-libs}
        # use "${ns3-libs}" "${ns3-contrib-libs}" in case you want to link to all
        # enabled modules
        EXECUTABLE_DIRECTORY_PATH ${CMAKE_OUTPUT_DIRECTORY}/scratch/eddd
)
