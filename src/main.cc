#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <climits>
#include "global.h"
#include "frontend.h"
#include "ast.h"
#include "dump.h"
#include "scoring.h"
#include "vector"

#define MAX_CANDIDATES 100000
#define MAX_CANDIDATE_NAME_LEN 200

bool debugMode = false;
bool showFEX = false;
bool enablePrettyPrinting = true;

//Number Users
long long N = 1e9;

//Number of elem returned
int K=1;

//Number of categories
int C = 32768;

//Number of people per MPC
int nPerMPC=40;

//Number of cores
int nCores = 1000;

//Number of Categories
int CATS = 32768;

double epsilon = 0.1;

FILE *dataOutFile = NULL;

/* The user can specify they constraints below, these are just defaults */

unsigned long long limitAvgComputationTimeOnUserDevices = 900000;
unsigned long long limitMaxComputationTimeOnUserDevices = 3600000;
unsigned long long limitComputationTimeOnAggregator = 20;
unsigned long long limitAvgBytesSentByUserDevices = 100000000;
unsigned long long limitMaxBytesSentByUserDevices = 2000000000;
unsigned long long limitBytesSentByAgrgegator = 2000000000000000;
unsigned long long limitAvgBytesReceivedByUserDevices = 99999999999999999;
unsigned long long limitMaxBytesReceivedByUserDevices = 99999999999999999;
unsigned long long limitBytesReceivedByAgrgegator = 99999999999999999;

long long tLastPrefixScoreStarted;
long long tLastPrefixScoreComplete;

ASTVignette *currentBest = NULL;
char currentBestName[MAX_CANDIDATE_NAME_LEN];
long long currentBestMetric = LLONG_MAX;
long long currentBestMetric2 = 0;
int numPrefixesScored =0;

//Maximum number of prefixes to be scored
int maxPrefixes = 999999999;

long long currentTimeMicros()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec*1000000LL + tv.tv_usec);
}

char *render(ASTBase *node)
{
  int bufsize = 800;
  while (true) {
    char *buf = (char*) malloc(bufsize);
    Printer *stringPrinter = new Printer(buf, bufsize);
    ASTDumpVisitor *dumper = new ASTDumpVisitor(stringPrinter);

    stringPrinter->setLineMode(true);
    node->accept(dumper);
    stringPrinter->flush();
    bool full = stringPrinter->bufferFull();

    delete dumper;

    if (!full)
      return buf;

    strcpy(&buf[bufsize-6], " ...");
    return buf;
  }
}

ASTVignette *candidate[MAX_CANDIDATES];
char candidateName[MAX_CANDIDATES][MAX_CANDIDATE_NAME_LEN];
int numCandidates = 0;

bool hasAddedMHT = false;

/* These are the main functions arboretum's query language has built in
*/
ASTFunctionFinder *declassifyFinder = new ASTFunctionFinder("declassify");
ASTFunctionFinder *outputFinder = new ASTFunctionFinder("output");
ASTFunctionFinder *gumbelNoiseFinder = new ASTFunctionFinder("GumbelNoise");
ASTFunctionFinder *expFinder = new ASTFunctionFinder("exp");
ASTFunctionFinder *logFinder = new ASTFunctionFinder("log");
ASTFunctionFinder *absFinder = new ASTFunctionFinder("abs");
ASTFunctionFinder *clipFinder = new ASTFunctionFinder("clip");
ASTFunctionFinder *laplaceNoiseFinder = new ASTFunctionFinder("laplacelNoise");
ASTFunctionFinder *sampleUniformFinder = new ASTFunctionFinder("sampleUniform");

ASTCryptoFinder *cryptoFinder = new ASTCryptoFinder();
ASTCryptoAnalysis *cryptoAnalysis = new ASTCryptoAnalysis(NULL);

#define FLAG_HAVE_AHE_KEYS (1<<0)
#define FLAG_HAVE_FHE_KEYS (1<<1)

void breakIntoVignettes(int flags, ASTVignette *soFar, ASTStatement *remainingStatements, const char *name)
{
  if (numPrefixesScored >= maxPrefixes)
    return;

  assert(soFar != NULL);
  ASTDumpVisitor *astDumper = new ASTDumpVisitor(new Printer(stdout));
  char newName[MAX_CANDIDATE_NAME_LEN+50];

  while (remainingStatements && remainingStatements->isSplitStatement())
    remainingStatements = remainingStatements->getNext();

  if (debugMode) {
    printf("--- Currently considering the following %s (%s):\n", remainingStatements ? "prefix" : "complete candidate", name);
    soFar->accept(astDumper);
    printf("\n");
  }

  /* Do the crypto analysis on the current prefix of vignettes */

  long long tCryptoAnalysisStarted = currentTimeMicros();
  cryptoAnalysis->reset(soFar);
  ASTType *dbType = new ASTType(TYPE_INT, 0, 1, 1, true);
  dbType->addArrayDim(CATS);
  dbType->addArrayDim(N);
  cryptoAnalysis->addSymbol("db", dbType);
  soFar->accept(cryptoAnalysis);

  cryptoFinder->reset();
  soFar->accept(cryptoFinder);

  if (cryptoFinder->hasFoundAHE() && !(flags&FLAG_HAVE_AHE_KEYS)) {
    ASTLetBinding *keygen = new ASTLetBinding(new ASTVariable("ahePrivkey"), new ASTFunctionExpression("aheKeygen"));
    keygen->setType(new ASTType(TYPE_KEYPAIR, 0, 0));
    ASTFunctionExpression *fe = new ASTFunctionExpression("aheExtractPubkey");
    fe->addArgument(new ASTVariable("ahePrivkey"));
    ASTLetBinding *pubkeyExt = new ASTLetBinding(new ASTVariable("ahePubkey"), fe);
    pubkeyExt->setType(new ASTType(TYPE_PUBKEY, 0, 0));
    keygen->setNext(pubkeyExt);
    ASTVignette *keygenVignette = new ASTVignette(keygen, NULL);
    keygenVignette->setNext(soFar);
    keygenVignette->setLocation(LOC_COMMITTEE);
    soFar = keygenVignette;
    flags |= FLAG_HAVE_AHE_KEYS;
  }

  if (cryptoFinder->hasFoundFHE() && !(flags&FLAG_HAVE_FHE_KEYS)) {
    ASTLetBinding *keygen = new ASTLetBinding(new ASTVariable("fhePrivkey"), new ASTFunctionExpression("fheKeygen"));
    keygen->setType(new ASTType(TYPE_KEYPAIR, 0, 0));
    ASTFunctionExpression *fe = new ASTFunctionExpression("fheExtractPubkey");
    fe->addArgument(new ASTVariable("fhePrivkey"));
    ASTLetBinding *pubkeyExt = new ASTLetBinding(new ASTVariable("fhePubkey"), fe);
    pubkeyExt->setType(new ASTType(TYPE_PUBKEY, 0, 0));
    keygen->setNext(pubkeyExt);
    ASTVignette *keygenVignette = new ASTVignette(keygen, NULL);
    keygenVignette->setNext(soFar);
    keygenVignette->setLocation(LOC_COMMITTEE);
    soFar = keygenVignette;
    flags |= FLAG_HAVE_FHE_KEYS;
  }

  long long tCryptoAnalysisComplete = currentTimeMicros();

  /* Check whether the current vignette prefix _already_ exceeds the specified limits. If so, we don't explore this further. */

  struct scoreStruct cost;
  tLastPrefixScoreStarted = currentTimeMicros();
  score(soFar, &cost, hasAddedMHT);
  numPrefixesScored++;
  long long metric = cost.avgComputationTimeOnUserDevices;

  long long tPrevPrefixScoreComplete = tLastPrefixScoreComplete;
  tLastPrefixScoreComplete = currentTimeMicros();

  bool continueWithThisPrefix = true;
  char outcome[300];
  strcpy(outcome, "Continuing to explore");

  if (debugMode && (numPrefixesScored%10==0)) {
    printf("%d prefixes scored", numPrefixesScored);
    printf("\n");
  }

  if (debugMode) {
    printf("Computation on user devices: %llu avg, %llu max\n", cost.avgComputationTimeOnUserDevices, cost.maxComputationTimeOnUserDevices);
    printf("Bytes sent by user devices: %llu avg, %llu max\n", cost.avgBytesSentByUserDevices, cost.maxBytesSentByUserDevices);
    printf("Bytes received by user devices: %llu avg, %llu max\n", cost.avgBytesReceivedByUserDevices, cost.maxBytesReceivedByUserDevices);
    printf("Computation on aggregator: %llu\n", cost.computationTimeOnAggregator);
    printf("Bytes sent by aggregator: %llu\n", cost.bytesSentByAgrgegator);
    printf("Bytes received by aggregator: %llu\n", cost.bytesReceivedByAgrgegator);
    printf("\n");
  }

  if ((cost.infeasible) || (cost.avgComputationTimeOnUserDevices > limitAvgComputationTimeOnUserDevices) ||
      (cost.maxComputationTimeOnUserDevices > limitMaxComputationTimeOnUserDevices) ||
      (cost.computationTimeOnAggregator > limitComputationTimeOnAggregator) ||
      (cost.avgBytesSentByUserDevices > limitAvgBytesSentByUserDevices) ||
      (cost.maxBytesSentByUserDevices > limitMaxBytesSentByUserDevices) ||
      (cost.bytesSentByAgrgegator > limitBytesSentByAgrgegator) ||
      (cost.avgBytesReceivedByUserDevices > limitAvgBytesReceivedByUserDevices) ||
      (cost.maxBytesReceivedByUserDevices > limitMaxBytesReceivedByUserDevices) ||
      (cost.bytesReceivedByAgrgegator > limitBytesReceivedByAgrgegator)) {
    if (cost.infeasible) {
      strcpy(outcome, "Invalid Crypto Assignment, not exploring further");
    } else if (cost.avgComputationTimeOnUserDevices > limitAvgComputationTimeOnUserDevices) {
      strcpy(outcome, "Avg Compute Time on Users exceeds limits, not exploring further");
    } else if (cost.maxComputationTimeOnUserDevices > limitMaxComputationTimeOnUserDevices) {
      strcpy(outcome, "Max Compute Time on Users exceeds limits, not exploring further");
    } else if (cost.computationTimeOnAggregator > limitComputationTimeOnAggregator) {
      strcpy(outcome, "Compute Time on Aggregators exceeds limits, not exploring further");
    } else if (cost.avgBytesSentByUserDevices > limitAvgBytesSentByUserDevices) {
      strcpy(outcome, "Avg Bytes sent by users exceeds limits, not exploring further");
    } else if (cost.maxBytesSentByUserDevices > limitMaxBytesSentByUserDevices) {
      strcpy(outcome, "Max Bytes sent by Users exceeds limits, not exploring further");
    } else if (cost.bytesSentByAgrgegator > limitBytesSentByAgrgegator){
      strcpy(outcome, "Max Bytes sent by Aggregator exceeds of limits, not exploring further");
    } else if (cost.avgBytesReceivedByUserDevices > limitAvgBytesReceivedByUserDevices) {
      strcpy(outcome, "Avg Bytes received by users exceeds limits, not exploring further");
    } else if (cost.maxBytesReceivedByUserDevices > limitMaxBytesReceivedByUserDevices) {
      strcpy(outcome, "Max Bytes received by Users exceeds limits, not exploring further");
    } else if (cost.bytesReceivedByAgrgegator > limitBytesReceivedByAgrgegator){
      strcpy(outcome, "Max Bytes received by Aggregator exceeds of limits, not exploring further");
    } else {
      strcpy(outcome, "Exceeds specified limits, not exploring further.\n");
    }
    if (debugMode)
      printf("%s\n", outcome);
    continueWithThisPrefix = false;
    hasAddedMHT = false;
  }


  /* Check whether the current vignette prefix is _already_ worse than the currently best known candidate. If so, don't explore further. */

  if (continueWithThisPrefix && (metric >= currentBestMetric)) {
    sprintf(outcome, "Worse than best known candidate (metric is %llu, current best is %llu from %s)", metric, currentBestMetric, currentBestName);
    if (debugMode)
      printf("%s\n", outcome);
    continueWithThisPrefix = false;
    hasAddedMHT = false;
  }

  /* If there are no more statements to assign to vignettes, score the entire thing */

  if (continueWithThisPrefix && !remainingStatements) {
    ASTVignette *candidate = soFar->clone();

    // Merge aggregator vignettes

    ASTVignette *ptr = candidate;
    while (ptr && ptr->getNext()) {
      if ((ptr->getLocation() == LOC_AGGREGATOR) && (ptr->getNext()->getLocation() == LOC_AGGREGATOR)) {
        ptr->appendStatement(ptr->getNext()->getStatements()->clone());
        ptr->setNext(ptr->getNext()->getNext());
      } else {
        ptr = ptr->getNext();
      }
    }

    if (debugMode) {
      printf("--- New best candidate found (%s, metric %llu):\n", name, metric);
      candidate->accept(astDumper);
      printf("Computation on user devices: %llu avg, %llu max\n", cost.avgComputationTimeOnUserDevices, cost.maxComputationTimeOnUserDevices);
      printf("Bytes sent by user devices: %llu avg, %llu max\n", cost.avgBytesSentByUserDevices, cost.maxBytesSentByUserDevices);
      printf("Computation on aggregator: %llu\n", cost.computationTimeOnAggregator);
      printf("Bytes sent by aggregator: %llu\n", cost.bytesSentByAgrgegator);
      printf("\n");
      sprintf(outcome, "New best candidate (metric %llu)", metric);
    }

    currentBest = candidate->clone();
    currentBestMetric = metric;

    hasAddedMHT = false;
    strncpy(currentBestName, name, sizeof(currentBestName)-1);
    continueWithThisPrefix = false;
  }

  if (dataOutFile) {
    fprintf(dataOutFile, "%s;%d;%s;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%llu;%s\n", remainingStatements ? "P" : "C",
      numPrefixesScored, name, metric,
      cost.avgComputationTimeOnUserDevices, cost.maxComputationTimeOnUserDevices,
      cost.avgBytesSentByUserDevices, cost.maxBytesSentByUserDevices,
      cost.avgBytesReceivedByUserDevices, cost.maxBytesReceivedByUserDevices,
      cost.computationTimeOnAggregator, cost.bytesSentByAgrgegator, cost.bytesReceivedByAgrgegator,
      currentTimeMicros()-tPrevPrefixScoreComplete, currentTimeMicros()-tLastPrefixScoreStarted,
      tCryptoAnalysisComplete-tCryptoAnalysisStarted, outcome
    );
    fflush(dataOutFile);
  }

  if (!continueWithThisPrefix)
    return;

  /* Make another vignette and put statements into it one by one */

  ASTVignette *previousVignette = soFar;
  while (previousVignette && previousVignette->getNext())
    previousVignette = previousVignette->getNext();

  ASTVignette *current = new ASTVignette(NULL, NULL);
  soFar->appendVignette(current);

  ASTStatement *ptr = remainingStatements;
  int numStatementsCopied = 0;
  int nTrivial = 0;
  while (ptr) {
    bool continueCopying = true;
    while (ptr && continueCopying) {
      bool isTrivial = ptr->isTrivial();
      nTrivial+= isTrivial;
      bool nextIsTrivial = ptr->getNext() && ptr->getNext()->isTrivial();
      continueCopying = (nTrivial < 2 && isTrivial) || (isTrivial && nextIsTrivial);
      current->appendStatement(ptr->cloneWithoutSuccessors());
      numStatementsCopied ++;
      ptr = ptr->getNext();
    }

    bool canBeOnUser = true, canBeOnCommittee = true, canBeOnAggregator = true;

    declassifyFinder->reset();
    current->accept(declassifyFinder);
    if (declassifyFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }
    outputFinder->reset();
    current->accept(outputFinder);
    if (outputFinder->isFound()) {
      canBeOnUser = false;
      canBeOnCommittee = false;
    }
    gumbelNoiseFinder->reset();
    current->accept(gumbelNoiseFinder);
    if (gumbelNoiseFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }
    expFinder->reset();
    current->accept(expFinder);
    if (expFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }
    logFinder->reset();
    current->accept(logFinder);
    if (logFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }
    clipFinder->reset();
    current->accept(clipFinder);
    if (clipFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }
    laplaceNoiseFinder->reset();
    current->accept(laplaceNoiseFinder);
    if (laplaceNoiseFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }
    absFinder->reset();
    current->accept(absFinder);
    if (absFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }
    sampleUniformFinder->reset();
    current->accept(sampleUniformFinder);
    if (sampleUniformFinder->isFound()) {
      canBeOnUser = false;
      canBeOnAggregator = false;
    }

    if (previousVignette && (previousVignette->getLocation() == LOC_USER))
      canBeOnUser = false;

    if (canBeOnUser) {
      current->setLocation(LOC_USER);
      sprintf(newName, "%sU%d", name, numStatementsCopied);
      breakIntoVignettes(flags, soFar, ptr, newName);
    } 
    if (canBeOnAggregator) {
      current->setLocation(LOC_AGGREGATOR);
      sprintf(newName, "%sA%d", name, numStatementsCopied);
      breakIntoVignettes(flags, soFar, ptr, newName);
    }

    /* If the current vignette is just a single for loop plus some trivial statements, we can also try to parallelize it */

    int nontrivialStatements = 0, forLoops = 0;
    ASTStatement *cur = current->getStatements();
    while (cur) {
      if (cur->isForLoop())
        forLoops ++;
      else if (!cur->isTrivial())
        nontrivialStatements ++;
      cur = cur->getNext();
    }

    ASTDataParallelChecker *dpc = new ASTDataParallelChecker();
    current->accept(dpc);

    if (!nontrivialStatements && (forLoops==1) && dpc->dataParallel()) {
      ASTVignette *currentParallel = new ASTVignette(NULL, current->getContext());
      ASTStatement *cur = current->getStatements();
      while (cur) {
        if (cur->isForLoop()) {
          ASTForStatement *forLoop = (ASTForStatement*)cur;
          currentParallel->makeParallel(forLoop->getLoopVariable(), forLoop->getFromVal(), forLoop->getToVal());
          currentParallel->appendStatement(forLoop->getBody()->clone());
        } else {
          currentParallel->appendStatement(cur->cloneWithoutSuccessors());
        }
        cur = cur->getNext();
      }

      soFar->removeLastVignette();
      soFar->appendVignette(currentParallel);
      if (canBeOnCommittee) {
        currentParallel->setLocation(LOC_COMMITTEE);
        sprintf(newName, "%sP%d", name, numStatementsCopied);
        breakIntoVignettes(flags, soFar, ptr, newName);
      }
      soFar->removeLastVignette();
      soFar->appendVignette(current);
    }

    if (canBeOnCommittee) {
      current->setLocation(LOC_COMMITTEE);
      sprintf(newName, "%sC%d", name, numStatementsCopied);
      breakIntoVignettes(flags, soFar, ptr, newName);
    }

    /* If the next statement is a split statement, don't continue filling the current vignette */

    if (ptr && ptr->isSplitStatement())
      break;
  }

  soFar->removeLastVignette();
}

void addCandidate(ASTVignette *candidateArg, const char *name, const char *suffix)
{
  char thisName[MAX_CANDIDATE_NAME_LEN];
  snprintf(thisName, MAX_CANDIDATE_NAME_LEN-1, "%s%s", name, suffix);

  ASTTagger *tagger = new ASTTagger();
  candidateArg->accept(tagger);

  ASTDumpVisitor *astDumper = new ASTDumpVisitor(new Printer(stdout));

  assert(numCandidates < MAX_CANDIDATES);
  strcpy(candidateName[numCandidates], thisName);
  candidate[numCandidates] = candidateArg;
  numCandidates ++;

  ASTFullyExpandedChecker *fec = new ASTFullyExpandedChecker();
  candidateArg->accept(fec);

  if (fec->isFullyExpanded()) {
    ASTVariable *vxdb = new ASTVariable("xdb");
    vxdb->addIndex(new ASTVariable("i"));
    ASTVariable *vdb = new ASTVariable("db");
    vdb->addIndex(new ASTVariable("i"));

    ASTVignette *initVignette = new ASTVignette(new ASTLetBinding(vxdb, vdb), NULL);
    initVignette->setLocation(LOC_USER);
    initVignette->makeParallel("i", 0, N);

    ASTVignette *candidate = candidateArg->clone();
    ASTRenameVariableVisitor *arvv = new ASTRenameVariableVisitor("db", "xdb");
    candidate->accept(arvv);

    ASTInitialTypeInference *typeInf = new ASTInitialTypeInference();
    ASTType *dbType = new ASTType(TYPE_INT, 0, 1, 1, true);
    dbType->addArrayDim(CATS);
    dbType->addArrayDim(N);
    typeInf->addSymbol("db", dbType);
    initVignette->accept(typeInf);
    candidate->accept(typeInf);

    if (debugMode || showFEX) {
      printf("--- Fully expanded candidate #%d (%s):\n", numCandidates-1, thisName);
      candidate->accept(astDumper);
      delete astDumper;
      printf("\n");
    }

    breakIntoVignettes(0, initVignette, candidate->getStatements(), thisName);
  } else {
    if (debugMode) {
      printf("--- Candidate #%d (%s):\n", numCandidates-1, thisName);
      candidateArg->accept(astDumper);
      delete astDumper;
      printf("\n");
    }
  }
}

int main(int argc, char *argv[])
{
  char *inputFileName = NULL;
  bool outputNameOnly = false;

  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i], "-N")) {
      N = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "-C")) {
      C = atoi(argv[++i]);
      CATS = C;
    } else if (!strcmp(argv[i], "--max-prefixes")) {
      maxPrefixes = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-avg-comp-user")) {
      limitAvgComputationTimeOnUserDevices = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-max-comp-user")) {
      limitMaxComputationTimeOnUserDevices = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-comp-aggr")) {
      limitComputationTimeOnAggregator = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-avg-sent-user")) {
      limitAvgBytesSentByUserDevices = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-max-sent-user")) {
      limitMaxBytesSentByUserDevices = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-sent-aggr")) {
      limitBytesSentByAgrgegator = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-avg-rcvd-user")) {
      limitAvgBytesReceivedByUserDevices = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-max-rcvd-user")) {
      limitMaxBytesReceivedByUserDevices = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "--limit-rcvd-aggr")) {
      limitBytesReceivedByAgrgegator = atoll(argv[++i]);
    } else if (!strcmp(argv[i], "-o")) {
      dataOutFile = fopen(argv[++i], "w+");
      if (!dataOutFile) {
        fprintf(stderr, "Cannot open '%s' for writing\n", argv[i]);
        return 4;
      }
    } else if (!strcmp(argv[i], "--debug")) {
      debugMode = true;
    } else if (!strcmp(argv[i], "--name-only")) {
      outputNameOnly = true;
    } else if (!strcmp(argv[i], "--showFEX")) {
      showFEX = true;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      return 1;
    } else {
      if (inputFileName) {
        fprintf(stderr, "More than one input file specified, but only one allowed!\n");
        return 3;
      }
      inputFileName = argv[i];
    }
  }

  if (!inputFileName) {
    fprintf(stderr, "Syntax: arboretum [-N <numParticipants>] <inputfile>\n");
    return 2;
  }

  FEScope *rootScope = new FEScope(NULL, new SourceContext());
  rootScope->add(SYM_LETBINDING, "db");
  rootScope->add(SYM_LETBINDING,"infty");
  rootScope->add(SYM_LETBINDING,"out");

  rootScope->add(SYM_FUNCTION, "sum");
  rootScope->add(SYM_FUNCTION, "max");
  rootScope->add(SYM_FUNCTION, "min");
  rootScope->add(SYM_FUNCTION, "argmax");
  rootScope->add(SYM_FUNCTION, "em");
  rootScope->add(SYM_FUNCTION, "emSecret");
  rootScope->add(SYM_FUNCTION, "declassify");
  rootScope->add(SYM_FUNCTION, "output");
  rootScope->add(SYM_FUNCTION,"GumbelNoise");
  rootScope->add(SYM_FUNCTION,"sampleUniform");
  rootScope->add(SYM_FUNCTION, "log");
  rootScope->add(SYM_FUNCTION, "abs");
  rootScope->add(SYM_FUNCTION, "clip");
  rootScope->add(SYM_FUNCTION, "laplaceNoise");

  InputContext *frontend = new InputContext(inputFileName);
  ASTVignette *ast = frontend->parse(rootScope);

  if (frontend->hasErrors()) {
    fprintf(stderr, "%s: %i Error(s) and %i Warning(s)\n", inputFileName,
      frontend->getErrorCount(), frontend->getWarningCount());
    return 1;
  }

  ASTTagger *tagger = new ASTTagger();
  ast->accept(tagger);

  Printer *prn = new Printer(stdout);
  ASTDumpVisitor *astDumper = new ASTDumpVisitor(prn);
  
  if (debugMode) {
    printf("--- Program as parsed:\n");
    ast->accept(astDumper);
    printf("\n");
  }

  // Initial type inference
  ASTInitialTypeInference *typeInf = new ASTInitialTypeInference();
  ASTType *dbType = new ASTType(TYPE_INT, 0, 1, 1, true);
  dbType->addArrayDim(CATS);
  dbType->addArrayDim(N);
  typeInf->addSymbol("db", dbType);
  ast->accept(typeInf);

  if (debugMode) {
    printf("--- After type inference:\n");
    ast->accept(astDumper);
    printf("\n");
  }

  // Find candidates
  initScoring();

  numCandidates = 0;
  addCandidate(ast, "", "");
  tLastPrefixScoreComplete = currentTimeMicros();

  printf("Num Candidates: %d\n", numCandidates);

  for (int i=0; i<numCandidates; i++) {
    ASTVariantGenerator *generator = new ASTVariantGenerator(candidate[i], candidateName[i]);
    candidate[i]->accept(generator);
  }

  if (currentBest) {
    if (outputNameOnly) {
      printf("%s\n", currentBestName);
    } else {
      printf("--- Best candidate found (%s):\n", currentBestName);
      currentBest->accept(astDumper);
      printf("Metric: %llu\n", currentBestMetric);
      struct scoreStruct costs;
      score(currentBest, &costs, hasAddedMHT);
      printf("Avg compute time on users (ms): %llu\n", costs.avgComputationTimeOnUserDevices);
      printf("Max compute time on users (ms): %llu\n", costs.maxComputationTimeOnUserDevices);
      printf("Avg bytes sent by users: %llu\n", costs.avgBytesSentByUserDevices);
      printf("Max bytes sent by users: %llu\n", costs.maxBytesSentByUserDevices);
      printf("Compute time on aggregator (hours with 1000 cores): %llu\n", costs.computationTimeOnAggregator);
      printf("Bytes sent by aggregator: %llu\n", costs.bytesSentByAgrgegator);
      printf("number of candidates evaluted: %d\n", numCandidates);
      printf("Commitee size: %d\n", nPerMPC);
      printf("Number of MPCs: %llu\n", costs.totalMPCs);
      std::vector<operationScoreStruct> bestOps;
      getOps(currentBest,&bestOps);
      printf("List of operations with costs\n");
      for(std::vector<operationScoreStruct>::iterator op=begin(bestOps); op!=end(bestOps);++op) {
            printf("Op name: %s\n",op->operationName.c_str());
            if(op->isParallel && op->operationName!="decrypt" && op->operationName!="declassify") {
                printf("Avg bytes sent by users (individual): %llu\n", op->bandwidthPerUser*((op->nParallel +N-1)/N));
                printf("Avg bytes sent by users (MPC): %llu\n", (op->bandwidthMPC * op->nMPCs * nPerMPC*op->nParallel +N-1)/N);

                printf("Avg compute time on users (individual): %llu\n", op->userComputeTime * ((op->nParallel+N-1)/N));
                printf("Avg compute time on users (MPC): %llu\n", (op->mpcComputeTime * op->nMPCs * nPerMPC*op->nParallel +N-1)/N);
            } else {
                printf("Avg bytes sent by users (individual): %llu\n", ((op->bandwidthPerUser+N-1)/N));
                printf("Avg bytes sent by users (MPC): %llu\n", (op->bandwidthMPC * op->nMPCs* nPerMPC +N-1)/N);

                printf("Avg compute time on users (individual): %llu\n", (op->userComputeTime +N-1)/N);
                printf("Avg compute time on users (MPC): %llu\n", (op->mpcComputeTime * op->nMPCs* nPerMPC +N-1)/N);
            }

            printf("Max compute time on users in op (ms): %llu\n", std::max(op->userComputeTime,op->mpcComputeTime));
            printf("Max bytes sent by users in op: %llu\n", std::max(op->bandwidthPerUser,op->bandwidthMPC));
            printf("Compute time on aggregator ms: %llu\n", op->aggComputeTime);
            printf("Bytes sent by aggregator: %llu\n", op->bandwidthAgg);
      }
    }
  } else {
    if (outputNameOnly)
      printf("NothingFound\n");
    else
      printf("No suitable candidates found. Maybe adjust the limits in main.cc to allow more candidates?\n");
  }

  if (!outputNameOnly)
    printf("\n%d prefixes scored\n", numPrefixesScored);

  if (dataOutFile)
    fclose(dataOutFile);

  return 0;
}
