# NUMASK_setbench

In order to run this, you need to follow SetBench's instructions: https://gitlab.com/trbot86/setbench/-/wikis/home

NUMA is also required, install it with this command: apt-get install libnuma-dev

Try to integrate NUMASK on setbench

This repository should be inside ds folder in setbench

We have built the basic skeleton for this integration, the NUMASK.H has no more compilation errors, but adaptor.h still needs some work. Espeically for the Node<K, V>, in order to make that work in the record_manager, we have to change the node in skiplist.h to template with K and V. 

Throughout the code, we marked some of the TODO in the comments that require more work. 

Most of the changes are supposed to be made in NUMASK.h, and we should most likely leave adapter.h very similar to its current state. 
