#include "scoring.h"
#include "string"
#include "vector"
#include "costCalc.h"
#include "algorithm"
using namespace std;

class ASTScoringVisitor : public ASTDFSVisitorWithSymbolTable
{
public:
	struct scoreStruct results;
  vector<operationScoreStruct> allOps;
  struct bookKeepingStruct bookkeeping;
  bool hasAddedMHT;
  ASTScoringVisitor() : ASTDFSVisitorWithSymbolTable() {
    reset();
  }

  //Reset all bookkeeping 
  void reset() {
    results.avgComputationTimeOnUserDevices = 0;
    results.maxComputationTimeOnUserDevices = 0;
    results.computationTimeOnAggregator = 0;
    results.avgBytesSentByUserDevices = 0;
    results.maxBytesSentByUserDevices = 0;
    results.bytesSentByAgrgegator = 0;

    bookkeeping.prevVignetteLocation = LOC_UNSPECIFIED;
    bookkeeping.currVignetteLocation = LOC_UNSPECIFIED;
    bookkeeping.vignetteTotalVariableSize = 0;
  }

  //Zero out op struct values
  void zeroOp(struct operationScoreStruct *op) {
    op->aggComputeTime = 0;
    op->bandwidthAgg = 0;
    op->bandwidthPerUser =0;
    op->bandwidthMPC=0;
    op->userComputeTime=0;
    op->mpcComputeTime = 0;
    op->nMPCs = 0;
  }


  //Calculate cost of operation in a for loop 
  void forLoopOp(struct operationScoreStruct *op, int nForLoop)
  {
    op->aggComputeTime = op->aggComputeTime * nForLoop;
    op->bandwidthAgg = op->bandwidthAgg * nForLoop;
    op->bandwidthPerUser = op->bandwidthPerUser * nForLoop;
    op->bandwidthMPC= op->bandwidthMPC * nForLoop;
    op->userComputeTime= op->userComputeTime * nForLoop;
    op->mpcComputeTime = op->mpcComputeTime*nForLoop;
  };

  //Calculate cost of operation across each category 
  void perElemOp(struct operationScoreStruct *op, unsigned long long nelem)
  {
    op->aggComputeTime = op->aggComputeTime * nelem;
    op->bandwidthAgg = op->bandwidthAgg * nelem;
    op->bandwidthPerUser = op->bandwidthPerUser * nelem;
    op->bandwidthMPC= op->bandwidthMPC * nelem;
    op->userComputeTime= op->userComputeTime * nelem;
    op->mpcComputeTime = op->mpcComputeTime*nelem;
  };

  //Set encryption type
  void encryptionSet(encryptionType enc) {
    if (bookkeeping.currentEncryption == ENC_UNKNOWN) {
      bookkeeping.currentEncryption = enc;
    }
  }

  //For type consistency 
  virtual void visit(ASTVariable *peer) {
    //does nothing
  };
  
  /*Check if the operation location and encryption type are consistent 
  if so add the correct costs for the op*/
  virtual void visit(ASTInfixOpExpression *peer) {
    string operation(peer->getOp());
    struct operationScoreStruct op;
    vector<string> compOperators;
    compOperators.push_back(">");
    compOperators.push_back(">=");
    compOperators.push_back("<");
    compOperators.push_back("<=");
    compOperators.push_back("==");
    compOperators.push_back("&&");
    compOperators.push_back("||");
    compOperators.push_back("!");
    compOperators.push_back("!=");
    op.operationName = operation;

    if (operation=="+") {
      switch(bookkeeping.currVignetteLocation) {
        case LOC_COMMITTEE:
				  if (bookkeeping.prevVignetteLocation == bookkeeping.currVignetteLocation) {
						MPCAdditionCost(&op);
					} else {
						MPCAHEAdditionCost(&op);
					}
          break;
        case LOC_USER:
            if(bookkeeping.currentEncryption==ENC_PLAINTEXT) {
              zeroOp(&op);
            } else {
              AHEAdditionCostUser(&op);
            }
            break;
         case LOC_AGGREGATOR:
            if(bookkeeping.currentEncryption==ENC_PLAINTEXT) {
              zeroOp(&op);
            } else {
							AHEAdditionCostAggregator(&op);
							if (!hasAddedMHT) {
								struct operationScoreStruct opAggSend;
								opAggSend.operationName = "Aggmht";
								zeroOp(&opAggSend);
								opAggSend.bandwidthAgg = sendBandwidth(C)*bookkeeping.nForLoop;
								allOps.push_back(opAggSend);
								struct operationScoreStruct opUserMHT;
								opUserMHT.operationName = "usermht";
								zeroOp(&opUserMHT);
								opUserMHT.isParallel = true;
								opUserMHT.nParallel = N;
								AHEAdditionCostUserMHT(&opUserMHT);
								forLoopOp(&opUserMHT, 5);
								allOps.push_back(opUserMHT);
								hasAddedMHT = true;
							}
            }
            break;
          default:
            bookkeeping.infeasible = true;
            zeroOp(&op);
            break;
      }
    } else if (operation=="-") {
        switch(bookkeeping.currVignetteLocation) {
          case LOC_COMMITTEE:
            MPCAdditionCost(&op);
            break;
          case LOC_USER:
            if(bookkeeping.currentEncryption==ENC_PLAINTEXT) {
              zeroOp(&op);
            } else {
              AHEAdditionCostUser(&op);
              bookkeeping.infeasible = true;
            }
            break;
          case LOC_AGGREGATOR:
            if(bookkeeping.currentEncryption==ENC_PLAINTEXT) {
              zeroOp(&op);
            } else {
              AHEAdditionCostAggregator(&op);
              bookkeeping.infeasible = true;
            }
            break;
          default:
            bookkeeping.infeasible = true;
            zeroOp(&op);
            break;
        }
    } else if (operation=="*") {
      if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        ASTExpression* l = peer->getLeftExpression();
        ASTExpression* r = peer->getRightExpression();
        if (!l->getType() || !r->getType() || !l->getType()->getMustBeEncrypted() || !r->getType()->getMustBeEncrypted()) {
          MPCAdditionCost(&op);
          op.operationName = "multAsAdd";
        } else {
          if (bookkeeping.isFirstMultOp && bookkeeping.nForLoop == 1) {
					  MPCMultInitCost(&op);
					  op.operationName = "multInit";
					  bookkeeping.isFirstMultOp = false;
				  } else {
					  MPCMultiplicationCost(&op);
				  }
        }
      } else if (bookkeeping.currentEncryption==ENC_AHE) {
          bookkeeping.infeasible = true;
          zeroOp(&op);
      } else if(bookkeeping.currVignetteLocation==LOC_USER) {
          if(bookkeeping.currentEncryption==ENC_FHE) {
              FHEMultiplicationCostUser(&op);
          } else {
            zeroOp(&op);
          }
      } else if (bookkeeping.currVignetteLocation==LOC_AGGREGATOR) {
          if(bookkeeping.currentEncryption==ENC_FHE) {
                FHEMultiplicationCostAggregator(&op);
          } else {
            zeroOp(&op);
          }
      } else{
        bookkeeping.infeasible = true;
        zeroOp(&op);
      }
    } else if (operation=="/") {
        if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
          MPCDivisionCost(&op);
        } else {
            bookkeeping.infeasible = true;
            zeroOp(&op);
        }
    } else if (find(compOperators.begin(),compOperators.end(),operation)!=compOperators.end()) {
        if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
					 if (bookkeeping.isFirstCompOp && bookkeeping.nForLoop == 1) {
						 MPCComparisonInitCost(&op);
						 op.operationName = "compInit";
						 bookkeeping.isFirstCompOp = false;
					 } else {
						 MPCComparisonCost(&op);
					 }
        } else {
					bookkeeping.infeasible = true;
        }
    }

    if(bookkeeping.nForLoop>1) {
			int tmpNForLoop = bookkeeping.nForLoop;
			if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        ASTExpression* l = peer->getLeftExpression();
        ASTExpression* r = peer->getRightExpression();
				if (operation=="*" && bookkeeping.isFirstMultOp && !(!l->getType() || !r->getType() || !l->getType()->getMustBeEncrypted() || !r->getType()->getMustBeEncrypted())) 
        {
					struct operationScoreStruct compInit;
					compInit.operationName = "firstMultOp";
					compInit.isParallel = bookkeeping.isParallel;
					compInit.nParallel = max(bookkeeping.nParallel, 1);
					MPCMultInitCost(&compInit);
					allOps.push_back(compInit);
				  tmpNForLoop--;
					bookkeeping.isFirstMultOp = false;
				} else if ((find(compOperators.begin(),compOperators.end(),operation)!=compOperators.end()) && bookkeeping.isFirstCompOp) {
					struct operationScoreStruct compInit;
					compInit.operationName = "firstCompOp";
					compInit.isParallel = bookkeeping.isParallel;
					compInit.nParallel = max(bookkeeping.nParallel, 1);
					MPCComparisonInitCost(&compInit);
					allOps.push_back(compInit);
				  tmpNForLoop--;
					bookkeeping.isFirstCompOp = false;
				}
			}
      forLoopOp(&op, tmpNForLoop);
    }
    if(bookkeeping.isParallel) {
      op.isParallel = true;
      op.nParallel = bookkeeping.nParallel;
    } else {
      op.isParallel = false;
      op.nParallel = 1;
    }
    allOps.push_back(op);
    ASTDFSVisitor::visit(peer);
  };

  virtual void visit(ASTUnaryExpression *peer) {
    ASTDFSVisitor::visit(peer);
  };


  //For variable assignmennt determines location and encryption type 
  virtual void visit(ASTLetBinding *peer) {
    struct operationScoreStruct op;
    op.operationName = "let_"+string(peer->getVarToBind()->getName());

    //Have we determined what type of encryption is being used
    encryptionSet(peer->getType()->getEncryptionType());

    //What type of object
    typeCode letType = peer->getType()->whichType();

    //Are we setting an array or single variable (need to check fo loop)
    unsigned long long vtbNIndexes = peer->getVarToBind()->getNumIndexes();
    unsigned long long rhsNIndexes = peer->getType()->getArrayDepth();
    unsigned long long nelem = 1;
    if (rhsNIndexes==1) {
      nelem=(unsigned long long)(peer->getType()->getArrayDim(0));
    } else if (rhsNIndexes==2) {
      if (bookkeeping.isParallel) {
        nelem = (unsigned long long)(peer->getType()->getArrayDim(0));
      } else {
        nelem = (unsigned long long)(peer->getType()->getArrayDim(0)) * (unsigned long long)(peer->getType()->getArrayDim(1));
      }
    }
    if (letType!=TYPE_KEYPAIR || letType!=TYPE_PUBKEY) {
      bookkeeping.nElem = nelem;
    }
    peer->getRightHandExp()->accept(this);

    if (peer->getNext()) {
       peer->getNext()->accept(this);
    }
  };

  /*Checks if a function is occuring the correction location for its encryption
  type, then adds the appropriate costs*/
  virtual void visit(ASTFunctionExpression *peer) {
    operationScoreStruct op;
    string opName(peer->getFunctionName());
    op.operationName = opName;

    if (!opName.compare("GumbelNoise")) {
      if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        gumbelNoiseCost(&op);
      } else {
        zeroOp(&op);
      }
    } else if (!opName.compare("laplaceNoise")) {
      if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        laplaceNoiseCost(&op);
      } else {
        zeroOp(&op);
      }
    } else if (!opName.compare("sampleUniform")) {
      if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      }else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        sampleUniformCost(&op);
      } else {
        zeroOp(&op);
      }
    }
    else if (!opName.compare("exp")) {
      if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        exponentiateCost(&op);
      } else {
        zeroOp(&op);
      }
    }
    else if (!opName.compare("log")) {
      if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        logarithmCost(&op);
      } else {
        zeroOp(&op);
      }
    }
    else if (!opName.compare("abs")) {
        if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
          bookkeeping.infeasible = true;
        } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
          absoluteValueCost(&op);
        } else {
          zeroOp(&op); 
      }
    }
    else if (!opName.compare("clip")) {
      if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        MPCComparisonCost(&op);
        op.bandwidthMPC = op.bandwidthMPC*2; // Because technically we need to do 2 comparisons
      } else {
        zeroOp(&op); 
      }
    }
    else if (!opName.compare("declassify")) {
      if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        revealCost(&op);
      } else {
          zeroOp(&op);  
      }
    } else if (!opName.compare("aheKeygen") || !opName.compare("fheKeygen")) {
       if (bookkeeping.currVignetteLocation!=LOC_COMMITTEE && bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        bookkeeping.infeasible = true;
      } else if (bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        keyGenCost(&op);
      } else {
        zeroOp(&op);  
      }
    } else {
      zeroOp(&op);
    }

    if(bookkeeping.nForLoop>1 && op.operationName.compare("sampleUniform")) {
      forLoopOp(&op, bookkeeping.nForLoop);
    }
    if(bookkeeping.isParallel) {
      op.isParallel = true;
      op.nParallel = bookkeeping.nParallel;
    } else {
      op.isParallel = false;
      op.nParallel = 1;
    }
    allOps.push_back(op);
    ASTDFSVisitor::visit(peer);
  };

  virtual void visit(ASTConstInt *peer) {
    //does nothing
  };
  virtual void visit(ASTConstReal *peer) {
    //does nothing
  };
  virtual void visit(ASTConstBool *peer) {
    //does nothing
  };
  virtual void visit(ASTIfThenElseStatement *peer) {
    ASTDFSVisitor::visit(peer);
  };
  virtual void visit(ASTExpressionStatement *peer) {
    encryptionType expressionEncryptionType = peer->getExpression()->getType()->getEncryptionType();
    encryptionSet(expressionEncryptionType);
    ASTDFSVisitor::visit(peer);
  };
  virtual void visit(ASTForStatement *peer) {
    bookkeeping.nForLoop = bookkeeping.nForLoop*(unsigned long long)(1+peer->getToVal() - peer->getFromVal());

    peer->getBody()->accept(this);

    if (peer->getNext()) {
      peer->getNext()->accept(this);
    }
  };
};

ASTScoringVisitor *scorer;

//given a list of ops and costs, calculates the full cost of a plan 
void calcFullCost(vector<operationScoreStruct> allOps, struct scoreStruct *costs) {
  unsigned long long  aggComputeTimeMS = 0;

	int oldnPerMPC = nPerMPC;
  nPerMPC = updateMPCSize(costs->totalMPCs);
	assert(nPerMPC > 0);
  double reweight = (double)nPerMPC/(double)oldnPerMPC;

  for(vector<operationScoreStruct>::iterator op=begin(allOps); op!=end(allOps);++op) {
    costs->bytesSentByAgrgegator = costs->bytesSentByAgrgegator+op->bandwidthAgg;
    aggComputeTimeMS = aggComputeTimeMS + op->aggComputeTime;

    //Multiply parallel operations 
    if(op->isParallel && op->operationName!="decrypt" && op->operationName!="declassify") {
      costs->avgBytesSentByUserDevices = costs->avgBytesSentByUserDevices + op->bandwidthPerUser*((op->nParallel +N-1)/N);
      costs->avgBytesSentByUserDevices = costs->avgBytesSentByUserDevices + (op->bandwidthMPC * op->nMPCs * reweight * nPerMPC*op->nParallel +N-1)/N;

      costs->avgComputationTimeOnUserDevices = costs->avgComputationTimeOnUserDevices + op->userComputeTime * ((op->nParallel+N-1)/N);
      costs->avgComputationTimeOnUserDevices = costs->avgComputationTimeOnUserDevices + (op->mpcComputeTime * op->nMPCs * reweight * nPerMPC*op->nParallel +N-1)/N;
    } else {
      costs->avgBytesSentByUserDevices = costs->avgBytesSentByUserDevices + ((op->bandwidthPerUser+N-1)/N);
      costs->avgBytesSentByUserDevices = costs->avgBytesSentByUserDevices + (op->bandwidthMPC * op->nMPCs* reweight * nPerMPC +N-1)/N;

			costs->avgComputationTimeOnUserDevices = costs->avgComputationTimeOnUserDevices + ((op->userComputeTime +N-1)/N);
      costs->avgComputationTimeOnUserDevices = costs->avgComputationTimeOnUserDevices + (op->mpcComputeTime * op->nMPCs* reweight *  nPerMPC +N-1)/N;
    }
    costs->maxBytesSentByUserDevices = max(costs->maxBytesSentByUserDevices,op->bandwidthPerUser);
    costs->maxBytesSentByUserDevices = max(costs->maxBytesSentByUserDevices, (unsigned long long) (op->bandwidthMPC*reweight));
    costs->maxComputationTimeOnUserDevices = max(costs->maxComputationTimeOnUserDevices,op->userComputeTime);
    costs->maxComputationTimeOnUserDevices = max(costs->maxComputationTimeOnUserDevices, (unsigned long long) (op->mpcComputeTime*reweight));
  }
  //Convert to core hours
  costs->computationTimeOnAggregator = costs->computationTimeOnAggregator+ ((aggComputeTimeMS + (36e5*nCores) -1)/(36e5*nCores));
}

void initScoring()
{
  scorer = new ASTScoringVisitor();
}

//Given a vignette sequence, gets a list of atomic operations in order 
void getOps(ASTVignette *vignetteSequence,vector<operationScoreStruct> *allOps) {
  initScoring();
  scorer->bookkeeping.previousEncryption = ENC_PLAINTEXT;
  scorer->bookkeeping.currentEncryption = ENC_UNKNOWN;

	ASTVignette *ptr = vignetteSequence;
  int nCommittees;

	while (ptr) {
    assert(ptr->getLocation() != LOC_UNSPECIFIED);

   //Init bookkeeping for vigneete
		scorer->hasAddedMHT = false;
		scorer->bookkeeping.isFirstCompOp = true;
    scorer->bookkeeping.isFirstMultOp = true;
    scorer->bookkeeping.vignetteTotalVariableSize = 0;
    scorer->bookkeeping.isForLoop = false;
    scorer->bookkeeping.nForLoop = 1;
    scorer->bookkeeping.nElem = 0;
    nCommittees = 0;
    scorer->bookkeeping.currVignetteLocation = ptr->getLocation();
    scorer->bookkeeping.isParallel = ptr->parallel();
    scorer->bookkeeping.infeasible = false;
    if (!scorer->bookkeeping.isParallel) {
      scorer->bookkeeping.nParallel = 1;
    } else {
      scorer->bookkeeping.nParallel = ptr->getToVal()-ptr->getFromVal();
      scorer->bookkeeping.iLoop = (string)ptr->getLoopVariable();
    }


    ASTStatement *statements = ptr->getStatements();

    statements->accept(scorer);

    struct operationScoreStruct sendCostOp;
    struct operationScoreStruct fwdCostOp;
    sendCostOp.operationName="sendingToNextVignette";
    scorer->zeroOp(&sendCostOp);
    sendCostOp.isParallel = scorer->bookkeeping.isParallel;
    if (!scorer->bookkeeping.isParallel) {
      sendCostOp.nParallel = 1;
    } else {
        sendCostOp.nParallel = scorer->bookkeeping.nParallel;
    }
    switch(scorer->bookkeeping.currVignetteLocation) {
      case LOC_COMMITTEE:
        sendCostOp.bandwidthMPC = sendShares(scorer->bookkeeping.nElem);
        sendCostOp.nMPCs = 1;
        scorer->allOps.push_back(sendCostOp);

        fwdCostOp.operationName="aggregatorFwdToNextVignette";
        scorer->zeroOp(&fwdCostOp);
        fwdCostOp.bandwidthAgg = sendShares(scorer->bookkeeping.nElem) * sendCostOp.nParallel;
        fwdCostOp.isParallel = false;
        fwdCostOp.nParallel = 1;
        scorer->allOps.push_back(fwdCostOp);
        break;
      case LOC_UNSPECIFIED:
        break;
      default:
        unsigned long long  send = sendBandwidth(scorer->bookkeeping.nElem);
        if (scorer->bookkeeping.currVignetteLocation==LOC_USER) {
          sendCostOp.bandwidthPerUser = send;
        } else if (scorer->bookkeeping.currVignetteLocation==LOC_AGGREGATOR) {
          if (ptr->getNext() && ptr->getNext()->getLocation()!=LOC_AGGREGATOR) {
            sendCostOp.bandwidthAgg = send;
          }
        }
        scorer->allOps.push_back(sendCostOp);
        break;
    }


    if (scorer->bookkeeping.prevVignetteLocation != LOC_COMMITTEE &&
      scorer->bookkeeping.previousEncryption != ENC_PLAINTEXT &&
      scorer->bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        struct operationScoreStruct decryptOp;
        decryptOp.operationName="decryptionCommittee";
        decryptionPhaseOneCost(&decryptOp);
        struct operationScoreStruct decryptOp2;
        decryptionPhaseTwoCost(&decryptOp2);
        decryptOp2.operationName = "decryptPhase2";
        decryptOp.isParallel = false;
        decryptOp.nParallel = 1;
        decryptOp2.isParallel = false;
        decryptOp2.nParallel = 1;
        scorer->allOps.push_back(decryptOp);
        scorer->allOps.push_back(decryptOp2);
    }
    if (scorer->bookkeeping.currVignetteLocation!=LOC_COMMITTEE &&
    scorer->bookkeeping.previousEncryption==ENC_PLAINTEXT &&
    scorer->bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
      struct operationScoreStruct encryptOp;
      if (scorer->bookkeeping.currVignetteLocation== LOC_USER)
      { encryptOp.operationName = "userEncrypt";
        encryptionCostUser(&encryptOp);
        if(scorer->bookkeeping.isParallel) {
          encryptOp.isParallel = true;
          encryptOp.nParallel = scorer->bookkeeping.nParallel;
          encryptOp.aggComputeTime = encryptOp.aggComputeTime*encryptOp.nParallel;
        } else {
          encryptOp.isParallel = false;
          encryptOp.nParallel = 1;
        }
        scorer->allOps.push_back(encryptOp);
      }
      else if (scorer->bookkeeping.currVignetteLocation==LOC_AGGREGATOR) {
          encryptOp.operationName = "aggEncrypt";
          encryptionCostAgg(&encryptOp);
          scorer->allOps.push_back(encryptOp);
      }
    }
    if (scorer->bookkeeping.prevVignetteLocation==LOC_COMMITTEE &&
    scorer->bookkeeping.currVignetteLocation!=LOC_COMMITTEE &&
    scorer->bookkeeping.currentEncryption!=ENC_PLAINTEXT &&
    scorer->bookkeeping.previousEncryption!=ENC_PLAINTEXT) {
      struct operationScoreStruct mpcEncryptOp;
      mpcEncryptOp.operationName = "mpcReencrypt";
      reencryptionCost(&mpcEncryptOp); //need to modify to size and MPC specific
      mpcEncryptOp.isParallel = false;
      mpcEncryptOp.nParallel = 1;
      scorer->allOps.push_back(mpcEncryptOp);
    }
    if(scorer->bookkeeping.previousEncryption!=scorer->bookkeeping.currentEncryption &&
    scorer->bookkeeping.previousEncryption!=ENC_PLAINTEXT && scorer->bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        struct operationScoreStruct decryptOp;
        struct operationScoreStruct mpcEncryptOp;
        struct operationScoreStruct decryptOp2;
        decryptOp.operationName="decryptionCommittee";
         decryptOp2.operationName = "decryptPhase2";
         mpcEncryptOp.operationName = "mpcReencrypt";

        decryptionPhaseOneCost(&decryptOp);
        decryptionPhaseTwoCost(&decryptOp2);
        reencryptionCost(&mpcEncryptOp); 
        mpcEncryptOp.isParallel = false;
        mpcEncryptOp.nParallel = 1;

        decryptOp.isParallel = false;
        decryptOp.nParallel = 1;
        decryptOp2.isParallel = false;
        decryptOp2.nParallel = 1;
        scorer->allOps.push_back(decryptOp);
        scorer->allOps.push_back(decryptOp2);
        scorer->allOps.push_back(mpcEncryptOp);
    }
    scorer->bookkeeping.prevVignetteLocation = ptr->getLocation();
    scorer->bookkeeping.previousEncryption = scorer->bookkeeping.currentEncryption;
    scorer->bookkeeping.currentEncryption = ENC_UNKNOWN;
		ptr = ptr->getNext();
  }
  for(vector<operationScoreStruct>::iterator op=begin(scorer->allOps); op!=end(scorer->allOps);++op) {
    allOps->push_back(*op);
  }
	delete(scorer);
}

void score(ASTVignette *vignetteSequence, struct scoreStruct *costs, bool &mht)
{
	costs->avgComputationTimeOnUserDevices = 0;
	costs->maxComputationTimeOnUserDevices = 0;
	costs->computationTimeOnAggregator = 0;
	costs->avgBytesSentByUserDevices = 0;
	costs->maxBytesSentByUserDevices = 0;
	costs->bytesSentByAgrgegator = 0;
  costs->avgBytesReceivedByUserDevices=0;
  costs->maxBytesReceivedByUserDevices=0;
  costs->bytesReceivedByAgrgegator=0;
  costs->infeasible = false;
	costs->totalMPCs = 0;

  initScoring();
  scorer->bookkeeping.previousEncryption = ENC_PLAINTEXT;
  scorer->bookkeeping.currentEncryption = ENC_UNKNOWN;


	ASTVignette *ptr = vignetteSequence;
  int nCommittees;

	while (ptr) {
    assert(ptr->getLocation() != LOC_UNSPECIFIED);

    //Init bookkeeping for vigneete
		scorer->hasAddedMHT = false;
		scorer->bookkeeping.isFirstCompOp = true;
    scorer->bookkeeping.isFirstMultOp = true;
    scorer->bookkeeping.vignetteTotalVariableSize = 0;
    scorer->bookkeeping.isForLoop = false;
    scorer->bookkeeping.nForLoop = 1;
    scorer->bookkeeping.nElem = 0;
    nCommittees = 0;
    scorer->bookkeeping.currVignetteLocation = ptr->getLocation();
    scorer->bookkeeping.isParallel = ptr->parallel();
    scorer->bookkeeping.infeasible = false;
    if (!scorer->bookkeeping.isParallel) {
      scorer->bookkeeping.nParallel = 1;
    } else {
      scorer->bookkeeping.nParallel = ptr->getToVal()-ptr->getFromVal();
      scorer->bookkeeping.iLoop = (string)ptr->getLoopVariable();
    }


    ASTStatement *statements = ptr->getStatements();

    statements->accept(scorer);

    //Mark candidate as infeasible
    if (scorer->bookkeeping.infeasible) {
      costs->infeasible = true;
      break;
    }
    struct operationScoreStruct sendCostOp;
    struct operationScoreStruct fwdCostOp;
    sendCostOp.operationName="sendingToNextVignette";
    scorer->zeroOp(&sendCostOp);
    sendCostOp.isParallel = scorer->bookkeeping.isParallel;
    if (!scorer->bookkeeping.isParallel) {
      sendCostOp.nParallel = 1;
    } else {
        sendCostOp.nParallel = scorer->bookkeeping.nParallel;
    }
    switch(scorer->bookkeeping.currVignetteLocation) {
      case LOC_COMMITTEE:
        sendCostOp.bandwidthMPC = sendShares(scorer->bookkeeping.nElem);
        sendCostOp.nMPCs = 1;
        scorer->allOps.push_back(sendCostOp);

        fwdCostOp.operationName="aggregatorFwdToNextVignette";
        scorer->zeroOp(&fwdCostOp);
        fwdCostOp.bandwidthAgg = sendShares(scorer->bookkeeping.nElem) * sendCostOp.nParallel;
        fwdCostOp.isParallel = false;
        fwdCostOp.nParallel = 1;
        scorer->allOps.push_back(fwdCostOp);
        break;
      case LOC_UNSPECIFIED:
        break;
      default:
        unsigned long long  send = sendBandwidth(scorer->bookkeeping.nElem);
        if (scorer->bookkeeping.currVignetteLocation==LOC_USER) {
          sendCostOp.bandwidthPerUser = send;
        } else if (scorer->bookkeeping.currVignetteLocation==LOC_AGGREGATOR) {
          if (ptr->getNext() && ptr->getNext()->getLocation()!=LOC_AGGREGATOR) {
            sendCostOp.bandwidthAgg = send;
          }
        }
        scorer->allOps.push_back(sendCostOp);
        break;
    }


    if (scorer->bookkeeping.prevVignetteLocation != LOC_COMMITTEE &&
      scorer->bookkeeping.previousEncryption != ENC_PLAINTEXT &&
      scorer->bookkeeping.currVignetteLocation==LOC_COMMITTEE) {
        struct operationScoreStruct decryptOp;
        decryptOp.operationName="decryptionCommittee";
        decryptionPhaseOneCost(&decryptOp);
        struct operationScoreStruct decryptOp2;
        decryptionPhaseTwoCost(&decryptOp2);
        decryptOp2.operationName = "decryptPhase2";
        decryptOp.isParallel = false;
        decryptOp.nParallel = 1;
        decryptOp2.isParallel = false;
        decryptOp2.nParallel = 1;
        scorer->allOps.push_back(decryptOp);
        scorer->allOps.push_back(decryptOp2);
				costs->totalMPCs += decryptOp.nMPCs;
				costs->totalMPCs += decryptOp2.nMPCs;
    }
    if (scorer->bookkeeping.currVignetteLocation!=LOC_COMMITTEE &&
    scorer->bookkeeping.previousEncryption==ENC_PLAINTEXT &&
    scorer->bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
      struct operationScoreStruct encryptOp;
      if (scorer->bookkeeping.currVignetteLocation== LOC_USER)
      { encryptOp.operationName = "userEncrypt";
        encryptionCostUser(&encryptOp);
        if(scorer->bookkeeping.isParallel) {
          encryptOp.isParallel = true;
          encryptOp.nParallel = scorer->bookkeeping.nParallel;
          encryptOp.aggComputeTime = encryptOp.aggComputeTime*encryptOp.nParallel;
        } else {
          encryptOp.isParallel = false;
          encryptOp.nParallel = 1;
        }
        scorer->allOps.push_back(encryptOp);
      }
      else if (scorer->bookkeeping.currVignetteLocation==LOC_AGGREGATOR) {
          encryptOp.operationName = "aggEncrypt";
          encryptionCostAgg(&encryptOp);
          scorer->allOps.push_back(encryptOp);
      }
    }
    if (scorer->bookkeeping.prevVignetteLocation==LOC_COMMITTEE &&
    scorer->bookkeeping.currVignetteLocation!=LOC_COMMITTEE &&
    scorer->bookkeeping.currentEncryption!=ENC_PLAINTEXT &&
    scorer->bookkeeping.previousEncryption!=ENC_PLAINTEXT) {
      struct operationScoreStruct mpcEncryptOp;
      mpcEncryptOp.operationName = "mpcReencrypt";
      reencryptionCost(&mpcEncryptOp); 
      mpcEncryptOp.isParallel = false;
      mpcEncryptOp.nParallel = 1;
      scorer->allOps.push_back(mpcEncryptOp);
			costs->totalMPCs += mpcEncryptOp.nMPCs;
    }
    if(scorer->bookkeeping.previousEncryption!=scorer->bookkeeping.currentEncryption &&
    scorer->bookkeeping.previousEncryption!=ENC_PLAINTEXT && scorer->bookkeeping.currentEncryption!=ENC_PLAINTEXT) {
        struct operationScoreStruct decryptOp;
        struct operationScoreStruct mpcEncryptOp;
        struct operationScoreStruct decryptOp2;
        decryptOp.operationName="decryptionCommittee";
        decryptOp2.operationName = "decryptPhase2";
        mpcEncryptOp.operationName = "mpcReencrypt";

        decryptionPhaseOneCost(&decryptOp);
        decryptionPhaseTwoCost(&decryptOp2);
        reencryptionCost(&mpcEncryptOp);
        mpcEncryptOp.isParallel = false;
				costs->totalMPCs += decryptOp.nMPCs;
				costs->totalMPCs += decryptOp2.nMPCs;
				costs->totalMPCs += mpcEncryptOp.nMPCs;

        mpcEncryptOp.nParallel = 1;

        decryptOp.isParallel = false;
        decryptOp.nParallel = 1;
        decryptOp2.isParallel = false;
        decryptOp2.nParallel = 1;
        scorer->allOps.push_back(decryptOp);
        scorer->allOps.push_back(decryptOp2);
        scorer->allOps.push_back(mpcEncryptOp);
    }
    scorer->bookkeeping.prevVignetteLocation = ptr->getLocation();
    scorer->bookkeeping.previousEncryption = scorer->bookkeeping.currentEncryption;
    scorer->bookkeeping.currentEncryption = ENC_UNKNOWN;
		if (scorer->bookkeeping.currVignetteLocation == LOC_COMMITTEE) {
			costs->totalMPCs = costs->totalMPCs + scorer->bookkeeping.nParallel;
		}
		ptr = ptr->getNext();
  }

  calcFullCost(scorer->allOps,costs);
  mht = scorer->hasAddedMHT;
	delete(scorer);
}
