#include "global.h"
#include "ast.h"
#include "printer.h"
#include "dump.h"

#define MIN_FANOUT 64
#define FANOUT_MULTIPLIER 4
#define MAX_FANOUT 1024

#define TRY_UNROLLING_LOOPS

void generateFreshVariablePrefix(ASTVignette *root, const char *prefix, char *buf)
{
	int nextSuffix = 2;

	strcpy(buf, prefix);
  ASTVariablePrefixFinder *finder = new ASTVariablePrefixFinder(buf);
  while (true) {
  	root->accept(finder);
  	if (!finder->isFound()) {
  		delete finder;
  		break;
  	}

 		delete finder;
    sprintf(buf, "%s%d", buf, nextSuffix++);
    finder = new ASTVariablePrefixFinder(buf);
  }
}

ASTStatement *findEnclosingStatement(ASTBase *peer)
{
  assert(peer);
  ASTBase *parentStatement = peer->getParent();
  while (!parentStatement->isStatement()) {
  	parentStatement = parentStatement->getParent();
  	assert(parentStatement);
  }
  return (ASTStatement*) parentStatement;
}

/*
pipeline:
  - infer sensitivity, monotonicity, and prove DP
  - apply transformations
  - infer range types (which are implementation-level) and secrecy tags
  - segment into vignettes (at the source level)
  - assign crypto types and add conversions if necessary
  - tagging
*/

void ASTVariantGenerator::visit(ASTForStatement *peer)
{
#ifdef TRY_UNROLLING_LOOPS
  int range = peer->getToVal()-peer->getFromVal()+1;
  if ((range < 10) && !peer->getBody()->isTrivial()) {
    ASTVignette *candidate = root->clone();
    ASTStatement *body = peer->getBody();
    ASTStatement *replacement = NULL;

    for (int i=peer->getFromVal(); i<=peer->getToVal(); i++) {
      ASTStatement *thisIter = body->clone();
      ASTVariableSubstitutionVisitor *avsv = new ASTVariableSubstitutionVisitor(peer->getLoopVariable(), new ASTConstInt(i));
      thisIter->accept(avsv);
      if (!replacement)
        replacement = thisIter;
      else
        replacement->appendStatement(thisIter);
    }

    bool s = candidate->replace(peer->getTag(), replacement);
    assert(s);

    char suffix[20];
    sprintf(suffix, "Unroll%s%d-%d", peer->getLoopVariable(), peer->getFromVal(), peer->getToVal());
    addCandidate(candidate, name, suffix);
  }
#endif
  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTVariantGenerator::visit(ASTFunctionExpression *peer)
{
  /* Candidates for summation */

	if (!strcmp(peer->getFunctionName(), "sum")) {
    assert(peer->getNumArgs() == 1);
    assert(peer->getArg(0)->isVariable());
    for (int fanout=MAX_FANOUT; fanout>=MIN_FANOUT; fanout/=FANOUT_MULTIPLIER) {

      ASTVignette *candidate = root->clone();

      char acc[100], loopVar[100], innerLoopVar[100];
      generateFreshVariablePrefix(root, "acc", acc);
      generateFreshVariablePrefix(root, "i", loopVar);
      generateFreshVariablePrefix(root, "j", innerLoopVar);

      int levels = 0;
      long long coverage = fanout;
      while (coverage < N) {
        levels ++;
        coverage *= fanout;
      }
      assert(levels>0);

      // The for loop will need to use the argument, but with the loop variable as an additional index

      ASTStatement *parentStatement = findEnclosingStatement(candidate->findByTag(peer->getTag()));
      assert(parentStatement);

      ASTStatement *replacement = NULL;
      int loopMax = 1;
      for (int i=0; i<=levels; i++) {
        char thisLoopVar[200], thisAcc[200], prevAcc[200];
        sprintf(thisLoopVar, "%s%d", loopVar, (levels-i+1));
        sprintf(thisAcc, "%s%d", acc, (levels-i+1));
        sprintf(prevAcc, "%s%d", acc, (levels-i));
        ASTVariable *accVar = new ASTVariable(thisAcc);
        if (loopMax>1)
          accVar->addIndex(new ASTVariable(thisLoopVar));

        ASTStatement *loopBody = NULL;
        char thisInnerLoopVar[200];
        sprintf(thisInnerLoopVar, "%s%d", innerLoopVar, (levels-i));
        ASTVariable *arg = (i==levels) ? (ASTVariable*)(peer->getArg(0)->clone()) : new ASTVariable(prevAcc);
        ASTExpression *idx = (fanout>2) ? (ASTExpression*)new ASTVariable(thisInnerLoopVar) : (ASTExpression*)new ASTConstInt(1);
        arg->addIndex(new ASTInfixOpExpression(new ASTInfixOpExpression(new ASTVariable(thisLoopVar), "*", new ASTConstInt(fanout)), "+", idx));
        ASTVariable *arg2 = (i==levels) ? (ASTVariable*)(peer->getArg(0)->clone()) : new ASTVariable(prevAcc);
        arg2->addIndex(new ASTInfixOpExpression(new ASTVariable(thisLoopVar), "*", new ASTConstInt(fanout)));
        loopBody = new ASTLetBinding(accVar, arg2);
        ASTStatement *updateStmt = NULL;
        updateStmt = new ASTLetBinding(accVar->clone(), new ASTInfixOpExpression(accVar->clone(), "+", arg));
        if (fanout > 2)
          loopBody->appendStatement(new ASTForStatement(thisInnerLoopVar, 1, fanout-1, updateStmt));
        else
          loopBody->appendStatement(updateStmt);

        if (i>0) {
          ASTStatement *split = new ASTSplitStatement();
          split->appendStatement(replacement);
          replacement = split;
        }

        long long realLoopMax = (loopMax*N/coverage);
        ASTStatement *thisForLoop = (loopMax>1) ? new ASTForStatement(thisLoopVar, 0, realLoopMax-1, loopBody) : loopBody;
        if (replacement)
          thisForLoop->appendStatement(replacement);
        replacement = thisForLoop;

        loopMax *= fanout;
      }

      replacement->appendStatement(parentStatement->cloneWithoutSuccessors());

      // Replace the statement containing the sum with a sequence of three statements: 1) initialize accumulator to 0, 2) for loop
      // computing the sum, and 3) the original statement containin the sum

      bool s1 = candidate->replace(parentStatement->getTag(), replacement);
      assert(s1);

      // Replace the 'sum' call itself (in the surrounding expression) with the accumulator variable */

      char lastAcc[200];
      sprintf(lastAcc, "%s%d", acc, levels+1);

      bool s2 = candidate->replace(peer->getTag(), new ASTVariable(lastAcc));
      assert(s2);

      char suffix[20];
      sprintf(suffix, "%s%d", peer->getFunctionName(), fanout);
  	  addCandidate(candidate, name, suffix);
    }

    aborted = true;
	}

	if (!strcmp(peer->getFunctionName(), "min") || !strcmp(peer->getFunctionName(), "max") || !strcmp(peer->getFunctionName(), "argmax")) {
    assert(peer->getNumArgs() == 1);
    assert(peer->getArg(0)->isVariable());
    bool isMax = !strcmp(peer->getFunctionName(), "max");
    bool isArgMax = !strcmp(peer->getFunctionName(), "argmax");

    //for (int fanout=(C<32) ? (C-1) : 32; fanout>=2; fanout/=4) {
		for (int fanout=(C<512) ? (C-1) : 512; fanout>=8; fanout/=4) {
      ASTVignette *candidate = root->clone();

      char acc[100], loopVar[100], innerLoopVar[100], bestArg[100];
      generateFreshVariablePrefix(root, "acc", acc);
      generateFreshVariablePrefix(root, "bestArg", bestArg);
      generateFreshVariablePrefix(root, "i", loopVar);
      generateFreshVariablePrefix(root, "j", innerLoopVar);

      int levels = 0;
      long long coverage = fanout;
      while (coverage < C) {
        levels ++;
        coverage *= fanout;
      }
      assert(levels>0);

      // The for loop will need to use the argument, but with the loop variable as an additional index

      ASTStatement *parentStatement = findEnclosingStatement(candidate->findByTag(peer->getTag()));
      assert(parentStatement);

      ASTStatement *replacement = NULL;
      int loopMax = 1;
      for (int i=0; i<=levels; i++) {
        char thisLoopVar[200], thisAcc[200], prevAcc[200], thisBestArg[200], prevBestArg[200];
        sprintf(thisLoopVar, "%s%d", loopVar, (levels-i+1));
        sprintf(thisAcc, "%s%d", acc, (levels-i+1));
        sprintf(prevAcc, "%s%d", acc, (levels-i));
        sprintf(thisBestArg, "%s%d", bestArg, (levels-i+1));
        sprintf(prevBestArg, "%s%d", bestArg, (levels-i));

        ASTExpression *outerLoopVarOr0 = (loopMax>1) ? (ASTExpression*)new ASTVariable(thisLoopVar) : (ASTExpression*)new ASTConstInt(0);

        ASTVariable *accVar = new ASTVariable(thisAcc);
        if (loopMax>1)
          accVar->addIndex(new ASTVariable(thisLoopVar));

        ASTStatement *loopBody = NULL;
        char thisInnerLoopVar[200];
        sprintf(thisInnerLoopVar, "%s%d", innerLoopVar, (levels-i));
        ASTVariable *arg = (i==levels) ? (ASTVariable*)(peer->getArg(0)->clone()) : new ASTVariable(prevAcc);
        ASTExpression *idx = (fanout>2) ? (ASTExpression*)new ASTVariable(thisInnerLoopVar) : (ASTExpression*)new ASTConstInt(1);
        ASTExpression *idx2 = new ASTInfixOpExpression(new ASTInfixOpExpression(outerLoopVarOr0->clone(), "*", new ASTConstInt(fanout)), "+", idx);
        arg->addIndex(idx2);
        ASTVariable *arg2 = (i==levels) ? (ASTVariable*)(peer->getArg(0)->clone()) : new ASTVariable(prevAcc);
        ASTExpression *arg2idx = new ASTInfixOpExpression(outerLoopVarOr0->clone(), "*", new ASTConstInt(fanout));
        arg2->addIndex(arg2idx);
        loopBody = new ASTLetBinding(accVar, arg2);
        if (isArgMax) {
          ASTVariable *lhs = new ASTVariable(thisBestArg);
          if (loopMax>1)
            lhs->addIndex(new ASTVariable(thisLoopVar));
          ASTExpression *rhs = arg2idx->clone();
          if (i != levels) {
            ASTVariable *x = new ASTVariable(prevBestArg);
            x->addIndex(rhs);
            rhs = x;
          }
          loopBody->appendStatement(new ASTLetBinding(lhs, rhs));
        }

        ASTStatement *innerStmt = new ASTLetBinding(accVar->clone(), arg);
        if (isArgMax) {
          ASTVariable *lhs = new ASTVariable(thisBestArg);
          if (loopMax>1)
            lhs->addIndex(new ASTVariable(thisLoopVar));
          ASTExpression *rhs = idx2->clone();
          if (i != levels) {
            ASTVariable *x = new ASTVariable(prevBestArg);
            x->addIndex(rhs);
            rhs = x;
          }
          innerStmt->appendStatement(new ASTLetBinding(lhs, rhs));
        }

        ASTStatement *updateStmt = new ASTIfThenElseStatement(
          new ASTInfixOpExpression(arg, isMax ? "<" : ">", accVar->clone()),
          innerStmt,
          NULL
        );
        if (fanout > 2)
          loopBody->appendStatement(new ASTForStatement(thisInnerLoopVar, 1, fanout-1, updateStmt));
        else
          loopBody->appendStatement(updateStmt);

        if (i>0) {
          ASTStatement *split = new ASTSplitStatement();
          split->appendStatement(replacement);
          replacement = split;
        }

        ASTStatement *thisForLoop = (loopMax>1) ? new ASTForStatement(thisLoopVar, 0, loopMax-1, loopBody) : loopBody;
        if (replacement)
          thisForLoop->appendStatement(replacement);
        replacement = thisForLoop;

        loopMax *= fanout;
      }

      replacement->appendStatement(parentStatement->cloneWithoutSuccessors());

      // Replace the statement containing the sum with a sequence of three statements: 1) initialize accumulator to 0, 2) for loop
      // computing the sum, and 3) the original statement containin the sum

      bool s1 = candidate->replace(parentStatement->getTag(), replacement);
      assert(s1);

      // Replace the 'sum' call itself (in the surrounding expression) with the accumulator variable */

      char resultVar[200];
      sprintf(resultVar, "%s%d", isArgMax ? bestArg : acc, levels+1);

      bool s2 = candidate->replace(peer->getTag(), new ASTVariable(resultVar));
      assert(s2);

      char suffix[20];
      sprintf(suffix, "%s%d", peer->getFunctionName(), fanout);
  	  addCandidate(candidate, name, suffix);
    }

    aborted = true;
	}

  if (!strcmp(peer->getFunctionName(), "em") || !strcmp(peer->getFunctionName(), "emSecret")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());
    if (arg->getType()->getArrayDepth() != 1)
      panic("Trying to invoke 'em' on an argument that is not a one-dimensional array!");

    /* Gumbel noise */

    ASTVignette *candidate = root->clone();

    char vmax[100], vidx[100], vloop[100], vscore[100], vemarg[100];
    generateFreshVariablePrefix(root, "bestIdx", vmax);
    generateFreshVariablePrefix(root, "idx", vidx);
    generateFreshVariablePrefix(root, "i", vloop);
    generateFreshVariablePrefix(root, "noisyScore", vscore);
    generateFreshVariablePrefix(root, "emArg", vemarg);

    ASTStatement *parentStatement = findEnclosingStatement(candidate->findByTag(peer->getTag()));
    assert(parentStatement);

    ASTStatement *replacement = new ASTLetBinding(new ASTVariable(vemarg), arg->clone());

    ASTVariable *vscoreVar = new ASTVariable(vscore);
    vscoreVar->addIndex(new ASTVariable(vloop));
    ASTVariable *noisyExp = new ASTVariable(vemarg);
    noisyExp->addIndex(new ASTVariable(vloop));
    replacement->appendStatement(new ASTForStatement(vloop, 0, arg->getType()->getArrayDim(0),
      new ASTLetBinding(vscoreVar, new ASTInfixOpExpression(noisyExp, "+", new ASTFunctionExpression("GumbelNoise")))
    ));

    ASTFunctionExpression *argmaxCall = new ASTFunctionExpression("argmax");
    argmaxCall->addArgument(new ASTVariable(vscore));
    replacement->appendStatement(new ASTLetBinding(new ASTVariable(vmax), argmaxCall));
    replacement->appendStatement(parentStatement->cloneWithoutSuccessors());

    bool s1 = candidate->replace(parentStatement->getTag(), replacement);
    assert(s1);

    if (!strcmp(peer->getFunctionName(), "em")) {
      ASTFunctionExpression *f = (ASTFunctionExpression*)(candidate->findByTag(peer->getTag()));
      f->setFunctionName("declassify"); // any mechanism should pass its output through a 'declassify' function call. here we do that directly because it's a no-op
      f->removeArguments();
      f->addArgument(new ASTVariable(vmax));
    } else {
      candidate->replace(peer->getTag(), arg->clone());
    }
    addCandidate(candidate, name, "emGumbel");

    /* Tao variant */

    candidate = root->clone();
    char vcutoff[200], vsprime[200], vdenom[200], vrand[200], vcumulative[200], vout[200], vi1[200], vaggp[200], vBest[200];
    generateFreshVariablePrefix(root, "cutoff", vcutoff);
    generateFreshVariablePrefix(root, "sprime", vsprime);
    generateFreshVariablePrefix(root, "denom", vdenom);
    generateFreshVariablePrefix(root, "aggp", vaggp);
    generateFreshVariablePrefix(root, "score", vscore);
    generateFreshVariablePrefix(root, "rand", vrand);
    generateFreshVariablePrefix(root, "cumulative", vcumulative);
    generateFreshVariablePrefix(root, "out", vout);
    generateFreshVariablePrefix(root, "i", vi1);
    replacement = new ASTLetBinding(new ASTVariable(vcutoff), new ASTConstInt(207));
    replacement->appendStatement(new ASTLetBinding(new ASTVariable(vaggp), peer->getArg(0)->clone()));
    ASTFunctionExpression *fexp = new ASTFunctionExpression("max");
    fexp->addArgument(new ASTVariable(vaggp));
    replacement->appendStatement(new ASTLetBinding(new ASTVariable(vsprime), fexp));
    ASTVariable *aggidx = new ASTVariable(vaggp);
    aggidx->addIndex(new ASTVariable(vi1));
    ASTFunctionExpression *exp = new ASTFunctionExpression("exp");
    exp->addArgument(new ASTInfixOpExpression(new ASTConstReal(epsilon), "*",
      new ASTInfixOpExpression(aggidx->clone(), "-", new ASTInfixOpExpression(new ASTVariable(vsprime), "+", new ASTVariable(vcutoff))))
    );
    replacement->appendStatement(new ASTForStatement(vi1, 0, C, new ASTLetBinding(aggidx, exp)));
    ASTFunctionExpression *thesum = new ASTFunctionExpression("sum");
    thesum->addArgument(new ASTVariable(vaggp));
    replacement->appendStatement(new ASTLetBinding(new ASTVariable(vdenom), thesum));
    ASTFunctionExpression *sample = new ASTFunctionExpression("sampleUniform");
    sample->addArgument(new ASTConstInt(0));
    sample->addArgument(new ASTVariable(vdenom));
    replacement->appendStatement(new ASTLetBinding(new ASTVariable(vrand), sample));
    replacement->appendStatement(new ASTLetBinding(new ASTVariable(vcumulative), new ASTConstInt(0)));
    replacement->appendStatement(new ASTLetBinding(new ASTVariable(vout), new ASTConstInt(0)));
    ASTStatement *loop2 = new ASTIfThenElseStatement(
      new ASTInfixOpExpression(
        new ASTInfixOpExpression(new ASTVariable(vcumulative), ">=", new ASTVariable(vrand)),
        "&&",
        new ASTInfixOpExpression(new ASTVariable(vcumulative), "<", new ASTInfixOpExpression(new ASTVariable(vrand), "+", aggidx->clone()))
      ),
      new ASTLetBinding(new ASTVariable(vout), new ASTVariable(vi1)),
      NULL
    );
    loop2->appendStatement(new ASTLetBinding(new ASTVariable(vcumulative), new ASTInfixOpExpression(new ASTVariable(vcumulative), "+", aggidx->clone())));
    replacement->appendStatement(new ASTForStatement(vi1, 0, C, loop2));

    replacement->appendStatement(parentStatement->cloneWithoutSuccessors());

    parentStatement = findEnclosingStatement(candidate->findByTag(peer->getTag()));
    assert(parentStatement);
    s1 = candidate->replace(parentStatement->getTag(), replacement);
    assert(s1);

    if (!strcmp(peer->getFunctionName(), "em")) {
      ASTFunctionExpression *f = (ASTFunctionExpression*)(candidate->findByTag(peer->getTag()));
      assert(f);
      f->setFunctionName("declassify"); // any mechanism should pass its output through a 'declassify' function call. here we do that directly because it's a no-op
      f->removeArguments();
      f->addArgument(new ASTVariable(vout));
    } else {
      candidate->replace(peer->getTag(), new ASTVariable(vout));
    }

    addCandidate(candidate, name, "emTao");

    aborted = true;
  }
}
