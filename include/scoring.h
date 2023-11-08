#ifndef __scoring_h__
#define __scoring_h__

#include "ast.h"
#include "string"
#include "vector"

struct scoreStruct {
  unsigned long long avgComputationTimeOnUserDevices;
  unsigned long long maxComputationTimeOnUserDevices;
  unsigned long long computationTimeOnAggregator;
  unsigned long long avgBytesSentByUserDevices;
  unsigned long long maxBytesSentByUserDevices;
  unsigned long long avgBytesReceivedByUserDevices;
  unsigned long long maxBytesReceivedByUserDevices;
  unsigned long long bytesSentByAgrgegator;
  unsigned long long bytesReceivedByAgrgegator;
  unsigned long long totalMPCs;
  bool infeasible;
};

struct operationScoreStruct {
  std::string operationName;
  unsigned long long  bandwidthPerUser;
  unsigned long long  bandwidthMPC;
  int nMPCs;
  unsigned long long  bandwidthAgg;
  unsigned long long  aggComputeTime;
  unsigned long long  userComputeTime;
  unsigned long long  mpcComputeTime;
  bool isParallel;
  int nParallel;

};

struct bookKeepingStruct {
  vignetteLocation prevVignetteLocation;
  vignetteLocation currVignetteLocation;
  unsigned long long vignetteTotalVariableSize;
  encryptionType previousEncryption;
  encryptionType currentEncryption;
  bool isParallel;
  int nParallel;
  bool infeasible;
  bool isForLoop;
  unsigned long long nForLoop;
  unsigned long long nElem;
  std::string iLoop;
  bool isFirstCompOp;
  bool isFirstMultOp;
};

void score(ASTVignette *vignetteSequence, struct scoreStruct *costs, bool &mht);
void getOps(ASTVignette *vignetteSequence,std::vector<operationScoreStruct> *allOps);
void initScoring();

#endif /* defined(__scoring_h__) */
