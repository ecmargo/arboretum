#include <math.h> 

#include "global.h"
#include "dump.h"

bool showTags = false;
bool showExpressionTypes = false;
bool showLetBindingTypes = true;

char *prettyDouble(double d, char *buf)
{
  sprintf(buf, "%.5f", d);
  while ((strlen(buf)>0) && (buf[strlen(buf)-1] == '0'))
    buf[strlen(buf)-1] = 0;
  if ((strlen(buf)>0) && (buf[strlen(buf)-1] == '.'))
    buf[strlen(buf)-1] = 0;
  return buf;
}

ASTDumpVisitor::ASTDumpVisitor(Printer *printer)
{
  this->printer = printer;
}

ASTDumpVisitor::~ASTDumpVisitor()
{
  delete printer;
}

void ASTDumpVisitor::visit(ASTType *peer)
{
//  if (peer->isMonotonic())
//    printer->print("monononic ");

  if (peer->getMustBeEncrypted())
    printer->print("MBE ");
  if (peer->getMustBeFHE())
    printer->print("MBF ");

  switch (peer->getEncryptionType()) {
    case ENC_PLAINTEXT:
      printer->print("plaintext ");
      break;
    case ENC_AHE:
      printer->print("ahe ");
      break;
    case ENC_FHE:
      printer->print("fhe ");
      break;
    default: 
      break;
  }

  switch (peer->whichType()) {
    case TYPE_INT:
      printer->print("int");
      break;
    case TYPE_REAL:
      printer->print("real");
      break;
    case TYPE_BOOL:
      printer->print("bool");
      break;
    case TYPE_KEYPAIR:
      printer->print("keypair");
      break;
    case TYPE_PUBKEY:
      printer->print("pubkey");
      break;
    default:
      printer->print("UNKNOWN");
      break;
  }
  for (int i=peer->getArrayDepth()-1; i>=0; i--) 
    printer->print("[%d]", peer->getArrayDim(i));

  printer->print("<%lld..%lld>", peer->getMinValue(), peer->getMaxValue());
  if (peer->getSensitivity() != ASTType::INF)
    printer->print(" : %d", peer->getSensitivity());
}

void ASTDumpVisitor::visit(ASTConstInt *peer)
{
  char buf[200];
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  if (showExpressionTypes && peer->getType())
    printer->print("(");

  printer->print("%s", prettyDouble(peer->getValue(), buf));

  if (showExpressionTypes && (peer->getType())) {
    printer->print(" : ");
    visit(peer->getType());
    printer->print(")");
  }
}

void ASTDumpVisitor::visit(ASTConstReal *peer)
{
  char buf[200];
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  if (showExpressionTypes && peer->getType())
    printer->print("(");

  printer->print("%s", prettyDouble(peer->getValue(), buf));

  if (showExpressionTypes && (peer->getType())) {
    printer->print(" : ");
    visit(peer->getType());
    printer->print(")");
  }
}

void ASTDumpVisitor::visit(ASTConstBool *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  printer->print("%s", peer->getValue() ? "true" : "false");
}

void ASTDumpVisitor::visit(ASTLetBinding *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  peer->getVarToBind()->accept(this);
  if (showLetBindingTypes && (peer->getType())) {
    printer->print(" : (");
    visit(peer->getType());
    printer->print(")");
  }
  printer->print(" = ");
  peer->getRightHandExp()->accept(this);
  printer->println(";");

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTDumpVisitor::visit(ASTSplitStatement *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  printer->println("split;");

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTDumpVisitor::visit(ASTExpressionStatement *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  peer->getExpression()->accept(this);
  printer->println(";");

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTDumpVisitor::visit(ASTInfixOpExpression *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  if (!strcmp(peer->getOp(), "*"))
    printer->print("(");
  peer->getLeftExpression()->accept(this);
  if (!strcmp(peer->getOp(), "*"))
    printer->print(")");

  printer->print(" %s ", peer->getOp());

  if (!strcmp(peer->getOp(), "*"))
    printer->print("(");
  peer->getRightExpression()->accept(this);
  if (!strcmp(peer->getOp(), "*"))
    printer->print(")");
}

void ASTDumpVisitor::visit(ASTVariable *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  if (showExpressionTypes && peer->getType())
    printer->print("(");

  printer->print("%s", peer->getName());
  for (int i=0; i<peer->getNumIndexes(); i++) {
    printer->print("[");
    peer->getIndex(i)->accept(this);
    printer->print("]");
  }

  if (showExpressionTypes && (peer->getType())) {
    printer->print(" : ");
    visit(peer->getType());
    printer->print(")");
  }
}

void ASTDumpVisitor::visit(ASTArrayExpression *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  if (showExpressionTypes && peer->getType())
    printer->print("(");

  printer->print("[");
  for (int i=0; i<peer->getNumElements(); i++) {
    if (i>0) 
      printer->print(", ");

    peer->getElement(i)->accept(this);
  }
  printer->print("]");

  if (showExpressionTypes && (peer->getType())) {
    printer->print(" : ");
    visit(peer->getType());
    printer->print(")");
  }
}

void ASTDumpVisitor::visit(ASTIfThenElseStatement *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  printer->print("if ");
  peer->getExpr()->accept(this);
  printer->println(" then {");
  printer->indent(+1);
  peer->getThenBranch()->accept(this);
  printer->indent(-1);
  if (peer->getElseBranch()) {
    printer->println("} else {");
    printer->indent(+1);
    peer->getElseBranch()->accept(this);
    printer->indent(-1);
  }
  printer->println("}");

  if (peer->getNext())
    peer->getNext()->accept(this);
}

extern bool enablePrettyPrinting;

void ASTDumpVisitor::visit(ASTFunctionExpression *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  printer->print("%s(", peer->getFunctionName());
  for (int i=0; i<peer->getNumArgs(); i++) {
    if (i>0)
      printer->print(",");
    peer->getArg(i)->accept(this);
  }
  printer->print(")");
}

void ASTDumpVisitor::visit(ASTForStatement *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  printer->print("for %s=%d to %d", peer->getLoopVariable(), peer->getFromVal(), peer->getToVal());
  printer->indent(1);
  printer->println(" do");
  peer->getBody()->accept(this);
  printer->indent(-1);
  printer->println("endfor;");

  if (peer->getNext())
    peer->getNext()->accept(this);
}

void ASTDumpVisitor::visit(ASTVignette *peer)
{
  if (showTags) 
    printer->print("[%d]", peer->getTag());

  if (peer->parallel()) {
    printer->print("parallel vignette (%s from %d to %d)", peer->getLoopVariable(), peer->getFromVal(), peer->getToVal());
  } else {
    printer->print("vignette");
  }

  switch (peer->getLocation()) {
    case LOC_USER:
      printer->println(" on user");
      break;
    case LOC_COMMITTEE:
      printer->println(" on committees");
      break;
    case LOC_AGGREGATOR:
      printer->println(" on aggregator");
      break;
    case LOC_UNSPECIFIED:
      printer->println();
      break;
  }

  printer->indent(1);
  peer->getStatements()->accept(this);
  printer->indent(-1);
  printer->println("endvignette;");

  if (peer->getNext())
    peer->getNext()->accept(this);
}

