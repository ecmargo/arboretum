Query planner header files are in include/
Query planner source files are in src/
Example queries written in our input language are in queries/

Examples of how to run queries:
./arboretum queries/input -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20
./arboretum queries/topKinput -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 1000000000 --limit-comp-aggr 20 --debug
./arboretum queries/diffTop2 -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
./arboretum queries/auctionInputv2 -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
./arboretum queries/hypothesisTestingInput -C 2  -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
./arboretum queries/secrecyOfSample -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
./arboretum queries/laplace -N 1000000000 -C 1 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
./arboretum queries/median -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
./arboretum queries/naivebayes -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
./arboretum queries/kmedian -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 3000000000 --limit-comp-aggr 20 --debug

Default values of parameters can be changed in src/main.cc
The optimization metric can also be changed in src/main.cc
To disable branch-and-bound, modify src/main.cc

The costs of vignettes benchmarked on reference hardware can be modified in src/costCalc.cc
Variants such as Gumbel noise vs Traditional EM can be modified/added in src/variant.cc
The interface between the generated vignettes and src/costCalc.cc is src/socring.cc

Output of running the queries (best candidate found + costs) is in figs/
Scripts used to generate graphs are in CostGraphData/
Equations for probability of privacy failure and TFHE costs are in equations.py.
These can also be used to automatically calculate the appropriate committee size