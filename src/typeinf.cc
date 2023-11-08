#include "ast.h"
#include "dump.h"
#include "printer.h"
#include "global.h"

void ASTInitialTypeInference::visit(ASTConstInt *peer)
{
  ASTType *theType = new ASTType(TYPE_INT, peer->getValue(), peer->getValue(), 0, true);
  peer->setType(theType);
}

void ASTInitialTypeInference::visit(ASTConstReal *peer)
{
  ASTType *theType = new ASTType(TYPE_REAL, peer->getValue(), peer->getValue(), 0, true);
  peer->setType(theType);
}

void ASTInitialTypeInference::visit(ASTConstBool *peer)
{
  ASTType *theType = new ASTType(TYPE_BOOL, 0, 1, 0, true);
  peer->setType(theType);
}

void ASTInitialTypeInference::visit(ASTLetBinding *peer)
{
  peer->getRightHandExp()->accept(this);

  if (!peer->getRightHandExp()->getType())
    panic("Cannot infer type of '%s'", peer->getVarToBind()->getName());

  ASTType *resultType = peer->getRightHandExp()->getType()->clone();
  for (int i=0; i<peer->getVarToBind()->getNumIndexes(); i++) {
    peer->getVarToBind()->getIndex(i)->accept(this);
    ASTType *indexType = peer->getVarToBind()->getIndex(i)->getType();
    resultType->addArrayDim(indexType->getMaxValue()+1);
  }
  peer->setType(resultType);
  addSymbol(peer->getVarToBind()->getName(), resultType);

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTInitialTypeInference::visit(ASTForStatement *peer)
{
  addSymbol(peer->getLoopVariable(), new ASTType(TYPE_INT, peer->getFromVal(), peer->getToVal(), 0, true));
  peer->getBody()->accept(this);
  deleteSymbol(peer->getLoopVariable());

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTInitialTypeInference::visit(ASTArrayExpression *peer)
{
  int min = 9999999, max = -9999999;
  for (int i=0; i<peer->getNumElements(); i++) {
    peer->getElement(i)->accept(this);
    ASTType *type = peer->getElement(i)->getType();
    assert(type);
    if (type->whichType() != TYPE_INT)
      panic("Array expression with non-integer elements?!?");
    if (type->getMinValue() < min)
      min = type->getMinValue();
    if (type->getMaxValue() > max)
      max = type->getMaxValue();
  }

  ASTType *theType = new ASTType(TYPE_INT, min, max, 0, true);
  theType->addArrayDim(peer->getNumElements());
  peer->setType(theType);
}

void ASTInitialTypeInference::visit(ASTVignette *peer)
{
  if (peer->parallel())
    addSymbol(peer->getLoopVariable(), new ASTType(TYPE_INT, peer->getFromVal(), peer->getToVal(), 0, true));

  peer->getStatements()->accept(this);

  if (peer->parallel())
    deleteSymbol(peer->getLoopVariable());

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTInitialTypeInference::visit(ASTVariable *peer)
{
  for (int i=0; i<numSymbols; i++) {
    if (!strcmp(symbolTable[i].varName, peer->getName())) {
      ASTType *resultType = symbolTable[i].type->clone();
      for (int i=0; i<peer->getNumIndexes(); i++)
        resultType->removeArrayDim();
      peer->setType(resultType);
      return;
    }
  }

  panic("Unknown variable: '%s'", peer->getName());
}

void ASTDFSVisitorWithSymbolTable::dumpSymbolTable()
{
  Printer *prn = new Printer(stdout);
  ASTDumpVisitor *astDumper = new ASTDumpVisitor(prn);
  for (int i=0; i<numSymbols; i++) {
    prn->print("#%d: %s : ", i, symbolTable[i].varName);
    astDumper->visit(symbolTable[i].type);
    prn->println();
  }
}

void ASTInitialTypeInference::visit(ASTFunctionExpression *peer)
{
  for (int i=0; i<peer->getNumArgs(); i++)
    peer->getArg(i)->accept(this);

  if (!strcmp(peer->getFunctionName(), "sum") || !strcmp(peer->getFunctionName(), "min") || !strcmp(peer->getFunctionName(), "max") || !strcmp(peer->getFunctionName(), "argmax")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    ASTType *argType = arg->getType()->clone();
    if ((argType->whichType() != TYPE_INT) || !argType->getArrayDepth())
      panic("Cannot infer type of 'sum'");

    argType->removeArrayDim();
    peer->setType(argType);
  } else if (!strcmp(peer->getFunctionName(), "GumbelNoise")) {
    peer->setType(new ASTType(TYPE_REAL, 0, 999999999, 0, false));
  } else if (!strcmp(peer->getFunctionName(), "declassify")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    ASTType *argType = arg->getType()->clone();
    argType->setSensitivity(0);
    peer->setType(argType);
  } else if (!strcmp(peer->getFunctionName(), "output")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    peer->setType(arg->getType()->clone());
  } else if (!strcmp(peer->getFunctionName(), "em") || !strcmp(peer->getFunctionName(), "emSecret") || !strcmp(peer->getFunctionName(), "doEM")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    if (!arg->getType()->isMonotonic())
      panic("Attempting to call 'em' on an argument that isn't monotonic in 'db'");
    if (arg->getType()->getArrayDepth() != 1)
      panic("Attempting to call 'em' on an argument that isn't a one-dimensional array");
    if (arg->getType()->whichType() != TYPE_INT)
      panic("Attempting to call 'em' on an argument that isn't an array of integers");

    peer->setType(new ASTType(TYPE_INT, 0, arg->getType()->getArrayDim(0)-1, ASTType::INF, false));
  } else if (!strcmp(peer->getFunctionName(), "GumbelNoise")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    ASTType *argType = arg->getType()->clone();
    if (argType->whichType() != TYPE_INT)
      panic("Cannot infer type of 'GumbelNoise'");
    peer->setType(new ASTType(TYPE_REAL, 0, 999999999, 0, false));
  } else if (!strcmp(peer->getFunctionName(), "log")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    ASTType *argType = arg->getType()->clone();
    peer->setType(new ASTType(TYPE_REAL, 0, 999999999, 0, false));
  } else if (!strcmp(peer->getFunctionName(), "exp")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    ASTType *argType = arg->getType()->clone();
    peer->setType(new ASTType(TYPE_REAL, 0, 999999999, 0, false));
  } else if (!strcmp(peer->getFunctionName(), "abs")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    ASTType *argType = arg->getType()->clone();
    peer->setType(argType);
  } else if (!strcmp(peer->getFunctionName(), "laplaceNoise")) {
    ASTExpression *arg = peer->getArg(0);
    assert(arg);
    assert(arg->getType());

    ASTType *argType = arg->getType()->clone();
    // peer->setType(argType);
    peer->setType(new ASTType(TYPE_REAL, 0, 999999999, 0, false));
  } else if (!strcmp(peer->getFunctionName(), "sampleUniform")) {
    ASTExpression *arg1 = peer->getArg(0);
    assert(arg1);
    assert(arg1->getType());
    ASTType *argType1 = arg1->getType()->clone();
    peer->setType(argType1);

    ASTExpression *arg2 = peer->getArg(1);
    assert(arg2);
    assert(arg2->getType());
    ASTType *argType2 = arg2->getType()->clone();
    peer->setType(argType2);
  } else if (!strcmp(peer->getFunctionName(), "clip")) {
    ASTExpression *arg1 = peer->getArg(0);
    assert(arg1);
    assert(arg1->getType());
    ASTType *argType1 = arg1->getType()->clone();
    peer->setType(argType1);

    ASTExpression *arg2 = peer->getArg(1);
    assert(arg2);
    assert(arg2->getType());
    ASTType *argType2 = arg2->getType()->clone();
    peer->setType(argType2);

    ASTExpression *arg3 = peer->getArg(2);
    assert(arg3);
    assert(arg3->getType());
    ASTType *argType3 = arg3->getType()->clone();
    peer->setType(argType3);
  } else {
    panic("Cannot infer type of function '%s'", peer->getFunctionName());
  }
}

void ASTInitialTypeInference::visit(ASTInfixOpExpression *peer)
{
  if (!strcmp(peer->getOp(), "+") || !strcmp(peer->getOp(), "-")) {
    peer->getLeftExpression()->accept(this);
    peer->getRightExpression()->accept(this);

    ASTType *leftType = peer->getLeftExpression()->getType();
    ASTType *rightType = peer->getRightExpression()->getType();
    assert(leftType && rightType);

    bool hasAtLeastOneReal = ((leftType->whichType() == TYPE_REAL) || (rightType->whichType() == TYPE_REAL));
    bool hasAnythingOtherThanIntOrReal =
      ((leftType->whichType()!=TYPE_INT)&&(leftType->whichType()!=TYPE_REAL)) || ((rightType->whichType()!=TYPE_INT)&&(rightType->whichType()!=TYPE_REAL));

    if (!hasAnythingOtherThanIntOrReal) {
      if (leftType->getArrayDepth() != rightType->getArrayDepth())
        panic("Trying to add or subtract arrays of different depths");
      for (int i=0; i<leftType->getArrayDepth(); i++) {
        if (leftType->getArrayDim(i) != rightType->getArrayDim(i))
          panic("Trying to add or subtract arrays of different dimensions");
      }

      int leftSens = leftType->getSensitivity(), rightSens = rightType->getSensitivity();
      int resultSens = ((leftSens == ASTType::INF) || (rightSens == ASTType::INF)) ? ASTType::INF : (leftSens+rightSens);
      long long theMin = (!strcmp(peer->getOp(), "+")) ? leftType->getMinValue()+rightType->getMinValue() : leftType->getMinValue()-rightType->getMaxValue();
      long long theMax = (!strcmp(peer->getOp(), "+")) ? leftType->getMaxValue()+rightType->getMaxValue() : leftType->getMaxValue()-rightType->getMinValue();

      ASTType *resultType = new ASTType(hasAtLeastOneReal ? TYPE_REAL : TYPE_INT, theMin, theMax, resultSens, leftType->isMonotonic()&&rightType->isMonotonic());
      for (int i=0; i<leftType->getArrayDepth(); i++)
        resultType->addArrayDim(leftType->getArrayDim(i));

      peer->setType(resultType);
    } else {
      panic("Cannot infer type for addition or subtraction with arguments other than integers");
    }
  } else if (!strcmp(peer->getOp(), "*")) {
    peer->getLeftExpression()->accept(this);
    peer->getRightExpression()->accept(this);

    ASTType *leftType = peer->getLeftExpression()->getType();
    ASTType *rightType = peer->getRightExpression()->getType();
    assert(leftType && rightType);

    if (leftType->getArrayDepth() || rightType->getArrayDepth())
      panic("Trying to multiply arrays");

    if ((leftType->whichType() == TYPE_INT) && (rightType->whichType() == TYPE_INT)) {
      peer->setType(new ASTType(TYPE_INT, leftType->getMinValue()*rightType->getMinValue(), leftType->getMaxValue()*rightType->getMaxValue(), ASTType::INF, leftType->isMonotonic()&&rightType->isMonotonic()));
    } else if (((leftType->whichType() == TYPE_INT) && (rightType->whichType() == TYPE_REAL)) ||
               ((leftType->whichType() == TYPE_REAL) && (rightType->whichType() == TYPE_REAL)) ||
               ((leftType->whichType() == TYPE_REAL) && (rightType->whichType() == TYPE_INT))) {
      peer->setType(new ASTType(TYPE_REAL, leftType->getMinValue()*rightType->getMinValue(), leftType->getMaxValue()*rightType->getMaxValue(), ASTType::INF, leftType->isMonotonic()&&rightType->isMonotonic()));
    } else {
      panic("Cannot infer type for multiplication with arguments other than integers (%d, %d)", leftType->whichType(), rightType->whichType());
    }
  } else if (!strcmp(peer->getOp(), "/")) {
    peer->getLeftExpression()->accept(this);
    peer->getRightExpression()->accept(this);

    ASTType *leftType = peer->getLeftExpression()->getType();
    ASTType *rightType = peer->getRightExpression()->getType();
    assert(leftType && rightType);

    if (leftType->getArrayDepth() || rightType->getArrayDepth())
      panic("Trying to divide arrays");

    if ((leftType->whichType() == TYPE_INT) && (rightType->whichType() == TYPE_INT)) {
      peer->setType(new ASTType(TYPE_INT, leftType->getMinValue()*rightType->getMinValue(), leftType->getMaxValue()*rightType->getMaxValue(), ASTType::INF, leftType->isMonotonic()&&rightType->isMonotonic()));
    } else if (((leftType->whichType() == TYPE_INT) && (rightType->whichType() == TYPE_REAL)) ||
               ((leftType->whichType() == TYPE_REAL) && (rightType->whichType() == TYPE_REAL)) ||
               ((leftType->whichType() == TYPE_REAL) && (rightType->whichType() == TYPE_INT))) {
      peer->setType(new ASTType(TYPE_REAL, leftType->getMinValue()*rightType->getMinValue(), leftType->getMaxValue()*rightType->getMaxValue(), ASTType::INF, leftType->isMonotonic()&&rightType->isMonotonic()));
    } else {
      panic("Cannot infer type for division with arguments other than integers (%d, %d)", leftType->whichType(), rightType->whichType());
    }
  } else if (!strcmp(peer->getOp(), ">")|| !strcmp(peer->getOp(), ">=") || !strcmp(peer->getOp(), "<")||!strcmp(peer->getOp(), "<=") || !strcmp(peer->getOp(), "==")) {
    peer->getLeftExpression()->accept(this);
    peer->getRightExpression()->accept(this);

    ASTType *leftType = peer->getLeftExpression()->getType();
    ASTType *rightType = peer->getRightExpression()->getType();
    assert(leftType && rightType);

    int sensitivity = (!leftType->getSensitivity() && rightType->getSensitivity()) ? 0 : ASTType::INF;

    if (((leftType->whichType() == TYPE_INT) || (leftType->whichType() == TYPE_REAL)) && ((rightType->whichType() == TYPE_INT) || (rightType->whichType() == TYPE_REAL))) {
      peer->setType(new ASTType(TYPE_BOOL, 0, 1, sensitivity, false));
    } else {
      panic("Cannot infer type for comparison with arguments other than integers (%d, %d)", leftType->whichType(), rightType->whichType());
    }
  } else if (!strcmp(peer->getOp(), "&&")|| !strcmp(peer->getOp(), "||")) {
    peer->getLeftExpression()->accept(this);
    peer->getRightExpression()->accept(this);

    ASTType *leftType = peer->getLeftExpression()->getType();
    ASTType *rightType = peer->getRightExpression()->getType();
    assert(leftType && rightType);

    int sensitivity = (!leftType->getSensitivity() && rightType->getSensitivity()) ? 0 : ASTType::INF;

    if ((leftType->whichType() == TYPE_BOOL) && (rightType->whichType() == TYPE_BOOL)) {
      peer->setType(new ASTType(TYPE_BOOL, 0, 1, sensitivity, false));
    } else {
      panic("Cannot infer type for logical op with arguments other than booleans (%d, %d)", leftType->whichType(), rightType->whichType());
    }
  } else {
    panic("Cannot infer type for infix operator '%s'", peer->getOp());
  }
}
