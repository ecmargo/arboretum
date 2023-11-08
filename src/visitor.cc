#include "ast.h"

void ASTBase::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTExpression::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTVariable::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTInfixOpExpression::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTUnaryExpression::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTFunctionExpression::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTConstExpression::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTConstInt::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTConstReal::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTLetBinding::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTConstBool::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTIfThenElseStatement::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTStatement::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTExpressionStatement::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTForStatement::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTVignette::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTArrayExpression::accept(ASTVisitor *worker) { worker->visit(this); };
void ASTSplitStatement::accept(ASTVisitor *worker) { worker->visit(this); };
