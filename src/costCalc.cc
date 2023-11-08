#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <map>
#include <cmath>
#include "costCalc.h"
#include "scoring.h"

using namespace std;

//Bandwidth cost to send the cipher text
unsigned long long sendBandwidth(unsigned long long categories){
 unsigned long long tmp = max((unsigned long long)4096, categories);
 int nciphertexts = 1;
  if (tmp>32768) {
    nciphertexts = tmp/32768 +1;
  }
 return nciphertexts*tmp*2*128/8;
}

//Probability of privacy failure
double privacy_failure(double f, int sizeMPC, unsigned long long totalMPCs, int q){
  double p = exp(-f*sizeMPC);
  p *= pow(2*exp(1)*f,sizeMPC/2);

  return fmin(2*p*totalMPCs*q, 1);
}

//Size of MPC needed for a given privacy requirements and number of MPCs
int updateMPCSize(unsigned long long totalMPCs) {
  double pf;
  for (int i=10;i<=1000;i++) {
      pf = privacy_failure(0.03,i,totalMPCs,1000);
      if (pf<=2e-9) {
          return (i);
      }
  };
  return 0;
}

//Number of committees needed for leveling
int nLevelMaxCommittees(int nCatPerMPC) {
    int mpcs = 0;
    int left = C;
    while (left > 1) {
        mpcs = mpcs+(left/nCatPerMPC)+1;
        left = left+(left/nCatPerMPC)+1;
    }
    return mpcs;
}

//Creates bookkeeping struct
void makeCostDict(unsigned long long  bpu, unsigned long long  bMPC, unsigned long long nMPCs,
unsigned long long  bAgg, unsigned long long  aggComputeTime, unsigned long long  userComputeTime,unsigned long long  mpcComputeTime, struct operationScoreStruct *cost)
{
    cost->bandwidthPerUser = bpu;
    cost->bandwidthMPC = bMPC;
    cost->nMPCs = nMPCs;
    cost->bandwidthAgg = bAgg;
    cost->aggComputeTime = aggComputeTime;
    cost->userComputeTime = userComputeTime;
    cost->mpcComputeTime = mpcComputeTime;
    return;
}

//Bandwidth cost to send shares
unsigned long long  sendShares(unsigned long long numShares) {
  unsigned long long cost = 16*nPerMPC*numShares;
  return cost;
}

//Cost to generate keys 
void keyGenCost(struct operationScoreStruct *costs) {
    unsigned long  long bMPC = 0;
    unsigned long  long cMPC = 0;

    //Keygen
    bMPC += 1e6*116*nPerMPC/5;
    cMPC += 716.7*1000;

    //Send shares of secret key to decryption comittee, and publish public key
    bMPC += sendShares(C);
    bMPC += 2*128*max(C,4096)/8;
    unsigned long long nMPCs = 1;

    makeCostDict(0,bMPC,nMPCs,0,0,0,cMPC,costs);

    return;
}

//This is only for one-hot encoding which does work for other stuff
//see hyptothesis testing and auctions
void encryptionCostUser(struct operationScoreStruct *costs) {
  unsigned long  long bpu = 0;
  unsigned long  long bAggr = 0;
  unsigned long long cpu = 0;
  unsigned long long cAggr = 0;

  //size of ZKP of encryption (including one-hot encoding), for 32768 categories
  bpu += 734;

  if (C > 4096) {
    cpu += 19.6*1000; //encryption
    cpu += 41.4*1000; //ZKP proof generation
    cAggr += 0.01*1000; //ZKP verification
  } else {
    cpu += 0.66*1000; //encryption
    cpu += 6.39*1000; //ZKP proof generation
    cAggr += 0.007*1000; //ZKP verification
  }


  makeCostDict(bpu,0,0,bAggr,cAggr,cpu,0,costs);

  return;
}

//Cost of encryption to the aggregator
void encryptionCostAgg(struct operationScoreStruct *costs) {
  unsigned long long bAggr = 0;
  unsigned long long cAggr = 0;
  if (C > 4096) {
    cAggr += 19.6*1000; //encryption
  } else {
    cAggr += 0.66*1000; //encryption
  }

  makeCostDict(0,0,0,bAggr,cAggr,0,0,costs);

  return;
}

//Cost of reenecryption in the MPC
void reencryptionCost(struct operationScoreStruct *costs) {
    unsigned long long bMPC = 0;
    unsigned long long cMPC = 0;

    bMPC += 1e6*1218*nPerMPC/40;
    cMPC += 909.1*1000; 

    bMPC += sendShares(C);
    unsigned long long nMPCs = 1;

    makeCostDict(0,bMPC,nMPCs,0,0,0,cMPC,costs);

    return;
}

//First phase of decryption done by a single MPC
void decryptionPhaseOneCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //first step, for 32768 bins
  bMPC += 1e6*61*nPerMPC/40;
  cMPC += 139.8*1000;

  //Resharing cost
  bMPC += sendShares(C);
  unsigned long long nMPCs = 1;

  makeCostDict(0,bMPC,nMPCs,0,0,0,cMPC,costs);

  return;
}

//Second phase of decryption which is done over multiple committees
void decryptionPhaseTwoCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //decrypt 100 bins
  bMPC += 1e6*403*nPerMPC/40;
  cMPC += 143.2*1000;

  //Resharing cost
  bMPC += sendShares(100);

  unsigned long long decrypt_mpcs = C/100;
  if (C % 100 != 0) {
    decrypt_mpcs += 1;
  }

  makeCostDict(0,bMPC,decrypt_mpcs,0,0,0,cMPC,costs);

  return;
}

//Cost of revealing 
void revealCost (struct operationScoreStruct *costs) {
  unsigned long long nMPCs = 1;
  makeCostDict(0,0,nMPCs,0,0,0,0,costs);
  return;
}

//Cost of addition under AHE to user 
void AHEAdditionCostUser(struct operationScoreStruct *costs) {
  int ctSize = C;
  if (ctSize < 4096) {
    ctSize = 4096;
  }

  double scale;
  unsigned long long cpu;
  if (C > 32768) {
    scale = ctSize/32768;
    cpu = (0.0225*1000)*scale;
  }
  else {
    scale = 32768/ctSize;
    cpu = (0.0225*1000)/scale;
  }

  //for 32768 categories
  unsigned long long bpu = 734;
  cpu += 3.98*1000; //ZKP proof generation
  unsigned long long cAggr = 0.01*1000; //ZKP verification

  makeCostDict(bpu,0,0,0,cAggr,cpu,0,costs);
  return;
}

//Cost of user of MHT needed for addition under AHE 
void AHEAdditionCostUserMHT(struct operationScoreStruct *costs) {
  int ctSize = C;
  if (ctSize < 4096) {
    ctSize = 4096;
  }

  double scale;
  unsigned long long cpu;
  if (C > 32768) {
    scale = ctSize/32768;
    cpu = (0.0225*1000)*scale;
  }
  else {
    scale = 32768/ctSize;
    cpu = (0.0225*1000)/scale;
  }

  makeCostDict(0,0,0,0,0,cpu,0,costs);
  return;
}

//Cost of AHE addition done with MPC 
void MPCAHEAdditionCost(struct operationScoreStruct *costs) {
  int ctSize = C;
  if (ctSize < 4096) {
    ctSize = 4096;
  }

  double scale;
  unsigned long long cpu;
  if (C > 32768) {
    scale = ctSize/32768;
    cpu = (0.0225*1000)*scale/nPerMPC;
  }
  else {
    scale = 32768/ctSize;
    cpu = ((0.0225*1000)/scale)/nPerMPC;
  }

  unsigned long long bpu = 734/nPerMPC;
  cpu += 3.98*1000; //ZKP proof generation
  unsigned long long cAggr = 0.01*1000; //ZKP verification

  makeCostDict(0,bpu,1,0,cAggr,0,cpu,costs);
  return;
}

//Cost of addition under AHE to aggregator
void AHEAdditionCostAggregator(struct operationScoreStruct *costs) {
  int ctSize = C;
  if (ctSize < 4096) {
    ctSize = 4096;
  }

  double scale;
  unsigned long long cAggr;
  if (C > 32768) {
    scale = ctSize/32768;
    cAggr = (0.0225*1000)*scale;
  }
  else {
    scale = 32768/ctSize;
    cAggr = (0.0225*1000)/scale;
  }

  makeCostDict(0,0,0,0,cAggr,0,0,costs);
  return;
}

//Cost of addition done in MPC 
void MPCAdditionCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;

  //sending shares
  bMPC += sendShares(1);
  unsigned long long cMPC = 1;

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost to user of FHE multiplication
void FHEMultiplicationCostUser(struct operationScoreStruct *costs) {
  unsigned long long cpu = 28.004*1000;

  makeCostDict(0,0,0,0,0,cpu,0,costs);
  return;
}

//Cost to aggregator of FHE multiplication
void FHEMultiplicationCostAggregator(struct operationScoreStruct *costs) {
  unsigned long long cAggr = 28.004*1000;

  makeCostDict(0,0,0,0,cAggr,0,0,costs);
  return;
}

//Cost of generating Beaver triples for multiplication in MPC
void MPCMultInitCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  bMPC += sendShares(1);

  bMPC += 1e6*8*nPerMPC/5;
  cMPC += 17.4*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of multiplication in MPC 
void MPCMultiplicationCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  bMPC += 1e6*3*nPerMPC/40;
  cMPC += 2.0*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of negating in MPC
void MPCNegateCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;

  bMPC += sendShares(1);

  makeCostDict(0,bMPC,1,0,0,0,0,costs);

  return;
}

//Cost of exponentiating - only done in MPC
void exponentiateCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  bMPC += sendShares(1);

  bMPC += 1e6*18*nPerMPC/5;
  cMPC += 50.3*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of taking log - only done in MPC
void logarithmCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  bMPC += sendShares(1);

  bMPC += 1e6*15*nPerMPC/5;
  cMPC += 40.1*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of absolute value - only done in MPC
void absoluteValueCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  bMPC += sendShares(1);

  bMPC += 1e6*8*nPerMPC/5;
  cMPC += 17.3*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of adding gumbel noise
void gumbelNoiseCost(struct operationScoreStruct *costs) {
    unsigned long long bMPC = 0;
    unsigned long long cMPC = 0;

    //Noising Committee Adds Noise
    bMPC += 1e6*23*nPerMPC/5;
    cMPC += 67.1*1000; //for a size 40 MPC

    bMPC += sendShares(1);

    makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

    return;
}

//Cost of adding laplace noise
void laplaceNoiseCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //Noising Committee Adds Noise
  bMPC += 1e6*22*nPerMPC/5;
  cMPC += 67.3*1000; //for a size 40 MPC

  //Send shares
  bMPC += sendShares(1);

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of uniform sampling in MPC
void sampleUniformCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //Noising Committee Adds Noise
  bMPC += 1e6*8*nPerMPC/5;
  cMPC += 16.3*1000; //for a size 40 MPC

  //Send shares
  bMPC += sendShares(1);

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Set up generation of beaver triples for comparison in an MPC 
void MPCComparisonInitCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //sending shares
  bMPC += sendShares(1);

  bMPC += 1e6*8*nPerMPC/5;
  cMPC += 18.0*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of comparison in an MPC 
void MPCComparisonCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //sending shares
  bMPC += sendShares(1);

  bMPC += 1e6*3*nPerMPC/40;
  cMPC += 2.0*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost for taking the max of 10 values in an MPC
void MPCMaxCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //costs for max over 10 values/categories

  //Receving and sending shares
  bMPC += sendShares(2);

  bMPC += 1e6*11*nPerMPC/5;
  cMPC += 27.8*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}

//Cost of division in an MPC
void MPCDivisionCost(struct operationScoreStruct *costs) {
  unsigned long long bMPC = 0;
  unsigned long long cMPC = 0;

  //sending shares
  bMPC += sendShares(1);

  bMPC += 1e6*11*nPerMPC/5;
  cMPC += 29.0*1000; //for a size 40 MPC

  makeCostDict(0,bMPC,1,0,0,0,cMPC,costs);

  return;
}