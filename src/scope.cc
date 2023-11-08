#include <stdio.h>
#include <string.h>

#include "frontend.h"

void FEScope::add(SymbolClass cls, const char *identifier)
{
  struct symbolRecord *newRecord = (struct symbolRecord*) malloc(sizeof(struct symbolRecord));
  newRecord->next = symbols;
  newRecord->cls = cls;
  newRecord->name = (char*) malloc(strlen(identifier)+1);
  strcpy(newRecord->name, identifier);
  symbols = newRecord;
}

bool FEScope::symbolExists(const char *identifier, SymbolClass cls)
{
  if (symbolExistsLocally(identifier, cls))
    return true;
    
  if (parentScope)
    return parentScope->symbolExists(identifier, cls);
    
  return false;
}

bool FEScope::symbolExistsLocally(const char *identifier, SymbolClass cls)
{
  struct symbolRecord *iter = symbols;
  
  for (struct symbolRecord *iter = symbols; iter; iter = iter->next)
    if (!strcmp(iter->name, identifier) && (iter->cls == cls))
      return true;
    
  return false; 
}
