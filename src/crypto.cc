#include "global.h"
#include "ast.h"

void ASTCryptoAnalysis::visit(ASTVignette *peer)
{
	currentVignetteLocation = peer->getLocation();
  if (peer->parallel())
    addSymbol(peer->getLoopVariable(), new ASTType(TYPE_INT, peer->getFromVal(), peer->getToVal(), 0, true));

  peer->getStatements()->accept(this);

  if (peer->parallel())
    deleteSymbol(peer->getLoopVariable());

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTCryptoAnalysis::visit(ASTInfixOpExpression *peer)
{
	bool changed = true;
	while (changed) {
    changed = false;

    assert(peer->getType() != NULL);
    if ((peer->getType()->getSensitivity() != 0) && (currentVignetteLocation != LOC_COMMITTEE) && !inExpressionThatMustBeEncrypted) {
    	inExpressionThatMustBeEncrypted = true;
      changed = true;
    }

    bool oldInExpressionThatMustBeEncrypted = inExpressionThatMustBeEncrypted;
    bool oldInExpressionThatNeedsFHE = inExpressionThatNeedsFHE;

    ASTDFSVisitor::visit(peer);

    if ((!oldInExpressionThatMustBeEncrypted && inExpressionThatMustBeEncrypted) ||
    	  (!oldInExpressionThatNeedsFHE && inExpressionThatNeedsFHE))
    	changed = true;

    if (inExpressionThatMustBeEncrypted) {
    	peer->getType()->setMustBeEncrypted(true);
      if (inExpressionThatNeedsFHE)
      	peer->getType()->setMustBeFHE(true);
    }
  }
};

void ASTCryptoAnalysis::visit(ASTLetBinding *peer)
{
	inExpressionThatMustBeEncrypted = false;
	inExpressionThatNeedsFHE = false;

	peer->getRightHandExp()->accept(this);

	if (inExpressionThatMustBeEncrypted) {
		if (inExpressionThatNeedsFHE) {
			peer->getType()->setEncryptionType(ENC_FHE);
		}
		else {
			peer->getType()->setEncryptionType(ENC_AHE);
		}
	}

  addSymbol(peer->getVarToBind()->getName(), peer->getType()->clone());

  if (peer->getNext())
  	peer->getNext()->accept(this);
}

void ASTCryptoAnalysis::visit(ASTVariable *peer)
{
  ASTType *type = findSymbol(peer->getName());
  if (!type)
  	panic("During crypto analysis, found no type for variable '%s'", peer->getName());

  if (type->getSensitivity() != 0)
  	inExpressionThatMustBeEncrypted = true;

  if (inExpressionThatNeedsFHE && (type->getEncryptionType() != ENC_FHE)) {
  	root->accept(new ASTUpgradeVariableToFHEVisitor(peer->getName()));
  	type->setEncryptionType(ENC_FHE);
  }

  peer->getType()->setEncryptionType(type->getEncryptionType());

  if (type->getEncryptionType() != ENC_PLAINTEXT) {
  	inExpressionThatMustBeEncrypted = true;
  	if (type->getEncryptionType() == ENC_FHE)
  		inExpressionThatNeedsFHE = true;
  }

}

void ASTCryptoAnalysis::visit(ASTForStatement *peer)
{
  addSymbol(peer->getLoopVariable(), new ASTType(TYPE_INT, peer->getFromVal(), peer->getToVal(), 0, true));
  peer->getBody()->accept(this);
  deleteSymbol(peer->getLoopVariable());

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTCryptoAnalysis::visit(ASTFunctionExpression *peer)
{
  if (!strcmp(peer->getFunctionName(), "declassify") || !strcmp(peer->getFunctionName(), "output")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }

	  inExpressionThatMustBeEncrypted = false;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "GumbelNoise")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	}else if (!strcmp(peer->getFunctionName(), "laplaceNoise")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "log")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "exp")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "abs")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "max")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "min")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "sampleUniform")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "clip")) {
	  for (int i=0; i<peer->getNumArgs(); i++) {
		  inExpressionThatMustBeEncrypted = false;
		  inExpressionThatNeedsFHE = false;
	  	peer->getArg(i)->accept(this);
	  }
	  inExpressionThatMustBeEncrypted = true;
	  inExpressionThatNeedsFHE = false;
	} else if (!strcmp(peer->getFunctionName(), "aheKeygen") || !strcmp(peer->getFunctionName(), "fheKeygen") || !strcmp(peer->getFunctionName(), "aheExtractPubkey") || !strcmp(peer->getFunctionName(), "fheExtractPubkey")) {
	} else {
		panic("Cannot infer crypto types for function '%s'", peer->getFunctionName());
	}
}
