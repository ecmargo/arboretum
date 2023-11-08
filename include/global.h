#ifndef __global_h__
#define __global_h__

#include "dump.h"

#define dumpAST(ast) do { ast->accept(new ASTDumpVisitor(new Printer(stderr))); fprintf(stderr, "\n"); } while (0)

extern int enabledWarnings;
extern bool disableFuzz;
extern bool disableRandom;

class ASTBase;
class ASTVignette;

char *render(ASTBase *node, bool single = false);

#define strcatf(buf, fmt, arg...) sprintf(&(buf)[strlen(buf)], fmt, arg)
#define panic(a...) do { fprintf(stderr, "PANIC: "); fprintf(stderr, a); fprintf(stderr, "\n"); abort(); } while (0)

void addCandidate(ASTVignette *candidateArg, const char *nameArg, const char *suffixArg);
long long currentTimeMicros();

//Number Users
extern long long N;

//Number of elem returned
extern int K;

//Number of categories
extern int C;

//Number of people per MPC 
extern int nPerMPC;

//Number of cores
extern int nCores;

extern double epsilon;

#endif /* defined(__global_h__) */
