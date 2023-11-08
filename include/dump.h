#ifndef __dump_h__
#define __dump_h__

#include "ast.h"
#include "printer.h"

/* ASTDumpVisitor: Basically a pretty-printer that displays an AST. Useful to check whether
   the parser worked properly. */

class ASTDumpVisitor : public ASTVisitor
{
  Printer *printer;
  bool singleMode;
  
public:
  ASTDumpVisitor(Printer *printer);
  virtual ~ASTDumpVisitor();
  virtual void visit(ASTVariable *peer);
  virtual void visit(ASTInfixOpExpression *peer);
  virtual void visit(ASTLetBinding *peer);
  virtual void visit(ASTFunctionExpression *peer);
  virtual void visit(ASTConstInt *peer);
  virtual void visit(ASTConstReal *peer);
  virtual void visit(ASTConstBool *peer);
  virtual void visit(ASTIfThenElseStatement *peer);
  virtual void visit(ASTExpressionStatement *peer);
  virtual void visit(ASTForStatement *peer);
  virtual void visit(ASTVignette *peer);
  virtual void visit(ASTArrayExpression *peer);
  virtual void visit(ASTSplitStatement *peer);
  void visit(ASTType *peer);
};

char *prettyDouble(double d, char *buf);

#endif /* defined(__dump_h__) */
