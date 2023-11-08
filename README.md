# Arboretum 

## Authors
- Karan Newatia 'knewatia at seas.upenn.edu'
- Eli Margolin 'ecmargo at seas.upenn.edu'
- Andreas Haeberlen 'ahae at cis.upenn.edu'

## Description 
A query planner for distributed differentially private queries 

## Usage 
Example queries written in our input language are in queries/

```
Usage: ./arboretum <PATH_TO_QUERY> <FLAGS>

Flags:
  N                    number of Users in the System
  C                    number of categories
  max-prefixes         maximum number of prefixes to score when planning
  limit-avg-sent-user  limit on the bandwidth of the average user
  limit-max-sent-user  limit on maximum user bandwidth
  limit-avg-comp-user  limit on the compute time of the average user
  limit-max-comp-user  limit on the maximum user compute time
  limit-comp-aggr      limit on the aggregator compute time
  limit-sent-aggr      limt on the aggregator bandwidth
  deebug               prints out all intermediate plans being scored
```

Examples of how to run queries:
```
./arboretum queries/input -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20

./arboretum queries/hypothesisTestingInput -C 2  -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug

./arboretum queries/secrecyOfSample -N 1000000000 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug

./arboretum queries/laplace -N 1000000000 -C 1 --limit-avg-sent-user 100000000 --limit-max-sent-user 2000000000 --limit-comp-aggr 20 --debug
```

## Repo Setup 
- Query planner header files are in include/
- Query planner source files are in src/
- Default values of parameters can be changed in src/main.cc
- The optimization metric can also be changed in src/main.cc
- The costs of vignettes benchmarked on reference hardware can be modified in src/costCalc.cc
- The interface between the generated vignettes and src/costCalc.cc is src/scoring.cc
