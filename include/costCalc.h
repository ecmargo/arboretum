#ifndef __cost_h__
#define __cost_h__

#include "ast.h"
#include "global.h"

using namespace std;

unsigned long long sendBandwidth(unsigned long long elements);
unsigned long long sendShares(unsigned long long numShares);
double privacy_failure(double f, int sizeMPC, unsigned long long totalMPCs, int q);
int updateMPCSize(unsigned long long totalMPCs);
int nLevelMaxCommittees(int nCatPerMPC);
void makeCostDict(unsigned long long bpu, unsigned long long bMPC, unsigned long long nMPCs,
unsigned long long bAgg, unsigned long long aggComputeTime, unsigned long long userComputeTime,unsigned long long mpcComputeTime, struct operationScoreStruct *cost);
void keyGenCost(struct operationScoreStruct *costs);
void encryptionCostUser(struct operationScoreStruct *costs);
void encryptionCostAgg(struct operationScoreStruct *costs);
void reencryptionCost(struct operationScoreStruct *costs);
void decryptionPhaseOneCost(struct operationScoreStruct *costs);
void decryptionPhaseTwoCost(struct operationScoreStruct *costs);
void revealCost (struct operationScoreStruct *costs);
void AHEAdditionCostUser(struct operationScoreStruct *costs);
void AHEAdditionCostUserMHT(struct operationScoreStruct *costs);
void AHEAdditionCostAggregator(struct operationScoreStruct *costs);
void MPCAdditionCost(struct operationScoreStruct *costs);
void MPCAHEAdditionCost(struct operationScoreStruct *costs);
void FHEMultiplicationCostUser(struct operationScoreStruct *costs);
void FHEMultiplicationCostAggregator(struct operationScoreStruct *costs);
void MPCMultiplicationCost(struct operationScoreStruct *costs);
void HENegateCost(struct operationScoreStruct *costs);
void MPCNegateCost(struct operationScoreStruct *costs);
void exponentiateCost(struct operationScoreStruct *costs);
void logarithmCost(struct operationScoreStruct *costs);
void absoluteValueCost(struct operationScoreStruct *costs);
void gumbelNoiseCost(struct operationScoreStruct *costs);
void laplaceNoiseCost(struct operationScoreStruct *costs);
void sampleUniformCost(struct operationScoreStruct *costs);
void MPCMultInitCost(struct operationScoreStruct *costs);
void MPCComparisonInitCost(struct operationScoreStruct *costs);
void MPCComparisonCost(struct operationScoreStruct *costs);
void MPCMaxCost(struct operationScoreStruct *costs);
void MPCDivisionCost(struct operationScoreStruct *costs);
#endif /* defined(__cost_h__) */
