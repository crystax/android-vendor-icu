/*
*******************************************************************************
*
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  ucol_tok.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created 02/22/2001
*   created by: Vladimir Weinstein
*
* This module maintains a contraction table structure in expanded form
* and provides means to flatten this structure
* 
*/

#include "ucol_cnt.h"
#include "cmemory.h"
#include "unicode/uchar.h"

void uprv_growTable(ContractionTable *tbl, UErrorCode *status) {
    if(tbl->position == tbl->size) {
        uint32_t *newData = (uint32_t *)realloc(tbl->CEs, 2*tbl->size*sizeof(uint32_t));
        UChar *newCPs = (UChar *)realloc(tbl->codePoints, 2*tbl->size*sizeof(UChar));
        if(newData == NULL || newCPs == NULL) {
#ifdef UCOL_DEBUG
            fprintf(stderr, "out of memory for contractions\n");
#endif
            *status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        tbl->CEs = newData;
        tbl->codePoints = newCPs;
        tbl->size *= 2;
    }
}

CntTable *uprv_cnttab_open(CompactIntArray *mapping, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return 0;
    }
    CntTable *tbl = (CntTable *)uprv_malloc(sizeof(CntTable));
    tbl->mapping = mapping;
    tbl->elements = (ContractionTable **)uprv_malloc(INIT_EXP_TABLE_SIZE*sizeof(ContractionTable *));
    tbl->capacity = INIT_EXP_TABLE_SIZE;
    uprv_memset(tbl->elements, 0, INIT_EXP_TABLE_SIZE*sizeof(ContractionTable *));
    tbl->size = 0;
    tbl->position = 0;
    tbl->CEs = NULL;
    tbl->codePoints = NULL;
    tbl->offsets = NULL;
    return tbl;
}

ContractionTable *addATableElement(CntTable *table, uint32_t *key, UErrorCode *status) {
    ContractionTable *el = (ContractionTable *)uprv_malloc(sizeof(ContractionTable));
    el->CEs = (uint32_t *)uprv_malloc(INIT_EXP_TABLE_SIZE*sizeof(uint32_t));
    el->codePoints = (UChar *)uprv_malloc(INIT_EXP_TABLE_SIZE*sizeof(UChar));
    el->position = 0;
    el->size = INIT_EXP_TABLE_SIZE;
    uprv_memset(el->CEs, 'F', INIT_EXP_TABLE_SIZE*sizeof(uint32_t));
    uprv_memset(el->codePoints, 'F', INIT_EXP_TABLE_SIZE*sizeof(UChar));

    table->elements[table->size] = el;

    //uhash_put(table->elements, (void *)table->size, el, status);

    *key = table->size++;

    if(table->size == table->capacity) {
        // do realloc
        table->elements = (ContractionTable **)realloc(table->elements, table->capacity*2*sizeof(ContractionTable *));
        uprv_memset(table->elements+table->capacity, 0, table->capacity*sizeof(ContractionTable *));
        if(table->elements == NULL) {
#ifdef UCOL_DEBUG
          fprintf(stderr, "out of memory for contraction parts\n");
#endif
          *status = U_MEMORY_ALLOCATION_ERROR;
        } else {
          table->capacity *= 2;
        }
    }

    return el;
}

int32_t uprv_cnttab_constructTable(CntTable *table, uint32_t mainOffset, UErrorCode *status) {
    int32_t i = 0, j = 0;
    if(U_FAILURE(*status) || table->size == 0) {
        return 0;
    }

    table->position = 0;

    if(table->offsets != NULL) {
        free(table->offsets);
    }
    table->offsets = (int32_t *)uprv_malloc(table->size*sizeof(int32_t));


    /* See how much memory we need */
    for(i = 0; i<table->size; i++) {
        table->offsets[i] = table->position+mainOffset;
        table->position += table->elements[i]->position;
    }

    /* Allocate it */
    if(table->CEs != NULL) {
        free(table->CEs);
    }
    table->CEs = (uint32_t *)uprv_malloc(table->position*sizeof(uint32_t));
    uprv_memset(table->CEs, '?', table->position*sizeof(uint32_t));
    if(table->codePoints != NULL) {
        free(table->codePoints);
    }
    table->codePoints = (UChar *)uprv_malloc(table->position*sizeof(UChar));
    uprv_memset(table->codePoints, '?', table->position*sizeof(UChar));

    /* Now stuff the things in*/

    UChar *cpPointer = table->codePoints;
    uint32_t *CEPointer = table->CEs;
    for(i = 0; i<table->size; i++) {
        int32_t size = table->elements[i]->position;
        uint8_t ccMax = 0, ccMin = 255, cc = 0;
        for(j = 1; j<size; j++) {
          cc = u_getCombiningClass(table->elements[i]->codePoints[j]);
          if(cc>ccMax) {
            ccMax = cc;
          }
          if(cc<ccMin) {
            ccMin = cc;
          }
          *(cpPointer+j) = table->elements[i]->codePoints[j];
        }
        *cpPointer = ((ccMin==ccMax)?1:0 << 8) | ccMax;

        uprv_memcpy(CEPointer, table->elements[i]->CEs, size*sizeof(uint32_t));
        for(j = 0; j<size; j++) {
            if(isContraction(*(CEPointer+j))) {
                *(CEPointer+j) = constructContractCE(table->offsets[getContractOffset(*(CEPointer+j))]);
            }
        }
        cpPointer += size;
        CEPointer += size;
    }


    uint32_t CE;
    for(i = 0; i<=0xFFFF; i++) {
        CE = ucmp32_get(table->mapping, i);
        if(isContraction(CE)) {
            CE = constructContractCE(table->offsets[getContractOffset(CE)]);
            ucmp32_set(table->mapping, (UChar)i, CE);
        }
    }


    return table->position;
}

ContractionTable *uprv_cnttab_cloneContraction(ContractionTable *t) {
  ContractionTable *r = (ContractionTable *)uprv_malloc(sizeof(ContractionTable));

  r->position = t->position;
  r->size = t->size;

  r->codePoints = (UChar *)uprv_malloc(sizeof(UChar)*t->size);
  r->CEs = (uint32_t *)uprv_malloc(sizeof(uint32_t)*t->size);

  uprv_memcpy(r->codePoints, t->codePoints, sizeof(UChar)*t->size);
  uprv_memcpy(r->CEs, t->CEs, sizeof(uint32_t)*t->size);

  return r;

}

CntTable *uprv_cnttab_clone(CntTable *t) {
  int32_t i = 0;
  CntTable *r = (CntTable *)uprv_malloc(sizeof(CntTable));
  r->position = t->position;
  r->size = t->size;
  r->capacity = t->capacity;

  r->mapping = t->mapping;

  r->elements = (ContractionTable **)uprv_malloc(t->capacity*sizeof(ContractionTable *));
  //uprv_memcpy(r->elements, t->elements, t->capacity*sizeof(ContractionTable *));

  for(i = 0; i<t->size; i++) {
    r->elements[i] = uprv_cnttab_cloneContraction(t->elements[i]);
  }

  if(t->CEs != NULL) {
    r->CEs = (uint32_t *)uprv_malloc(t->position*sizeof(uint32_t));
    uprv_memcpy(r->CEs, t->CEs, t->position*sizeof(uint32_t));
  } else {
    r->CEs = NULL;
  }

  if(t->codePoints != NULL) {
    r->codePoints = (UChar *)uprv_malloc(t->position*sizeof(UChar));
    uprv_memcpy(r->codePoints, t->codePoints, t->position*sizeof(UChar));
  } else {
    r->codePoints = NULL;
  }

  if(t->offsets != NULL) {
    r->offsets = (int32_t *)uprv_malloc(t->size*sizeof(int32_t));
    uprv_memcpy(r->offsets, t->offsets, t->size*sizeof(int32_t));
  } else {
    r->offsets = NULL;
  }

  return r;
}

void uprv_cnttab_close(CntTable *table) {
    int32_t i = 0;
    for(i = 0; i<table->size; i++) {
        free(table->elements[i]->CEs);
        free(table->elements[i]->codePoints);
        free(table->elements[i]);
    }
    free(table->elements);
    free(table->CEs);
    free(table->offsets);
    free(table->codePoints);
    free(table);
}

/* this is for adding non contractions */
uint32_t uprv_cnttab_changeLastCE(CntTable *table, uint32_t element, uint32_t value, UErrorCode *status) {
    element &= 0xFFFFFF;

    ContractionTable *tbl = NULL;
    if(U_FAILURE(*status)) {
        return 0;
    }

    if((element == 0xFFFFFF) || (tbl = table->elements[element]) == NULL) {
        tbl = addATableElement(table, &element, status);
    }

    tbl->CEs[tbl->position-1] = value;

    return(constructContractCE(element));
}


/* inserts a part of contraction sequence in table. Sequences behind the offset are moved back. If element is non existent, it creates on. Returns element handle */
uint32_t uprv_cnttab_insertContraction(CntTable *table, uint32_t element, UChar codePoint, uint32_t value, UErrorCode *status) {

    element &= 0xFFFFFF;
    ContractionTable *tbl = NULL;

    if(U_FAILURE(*status)) {
        return 0;
    }

    if((element == 0xFFFFFF) || (tbl = table->elements[element]) == NULL) {
        tbl = addATableElement(table, &element, status);
    }

    uprv_growTable(tbl, status);

    uint32_t offset = 0;


    while(tbl->codePoints[offset] < codePoint && offset<tbl->position) {
        offset++;
    }

    uint32_t i = tbl->position;
    for(i = tbl->position; i > offset; i--) {
        tbl->CEs[i] = tbl->CEs[i-1];
        tbl->codePoints[i] = tbl->codePoints[i-1];
    }

    tbl->CEs[offset] = value;
    tbl->codePoints[offset] = codePoint;

    tbl->position++;

    return(constructContractCE(element));
}


/* adds more contractions in table. If element is non existant, it creates on. Returns element handle */
uint32_t uprv_cnttab_addContraction(CntTable *table, uint32_t element, UChar codePoint, uint32_t value, UErrorCode *status) {

    element &= 0xFFFFFF;

    ContractionTable *tbl = NULL;

    if(U_FAILURE(*status)) {
        return 0;
    }

    if((element == 0xFFFFFF) || (tbl = table->elements[element]) == NULL) {
        tbl = addATableElement(table, &element, status);
    } 

    uprv_growTable(tbl, status);

    tbl->CEs[tbl->position] = value;
    tbl->codePoints[tbl->position] = codePoint;

    tbl->position++;

    return(constructContractCE(element));
}

/* sets a part of contraction sequence in table. If element is non existant, it creates on. Returns element handle */
uint32_t uprv_cnttab_setContraction(CntTable *table, uint32_t element, uint32_t offset, UChar codePoint, uint32_t value, UErrorCode *status) {

    element &= 0xFFFFFF;
    ContractionTable *tbl = NULL;

    if(U_FAILURE(*status)) {
        return 0;
    }

    if((element == 0xFFFFFF) || (tbl = table->elements[element]) == NULL) {
        tbl = addATableElement(table, &element, status);
    }

    if(offset >= tbl->size) {
        *status = U_INDEX_OUTOFBOUNDS_ERROR;
        return 0;
    }
    tbl->CEs[offset] = value;
    tbl->codePoints[offset] = codePoint;

    //return(offset);
    return(constructContractCE(element));
}

ContractionTable *_cnttab_getContractionTable(CntTable *table, uint32_t element) {
    element &= 0xFFFFFF;
    ContractionTable *tbl = NULL;

    if((element == 0xFFFFFF) || (tbl = table->elements[element]) == NULL) {
      return NULL;
    } else {
      return tbl;
    }
}

int32_t _cnttab_findCP(ContractionTable *tbl, UChar codePoint) {
    uint32_t position = 0;
    if(tbl == NULL) {
      return -1;
    }

    while(codePoint > tbl->codePoints[position]) {
      position++;
      if(position > tbl->position) {
        return -1;
      }
    }
    if (codePoint == tbl->codePoints[position]) {
      return position;
    } else {
      return -1;
    }
}

uint32_t _cnttab_getCE(ContractionTable *tbl, int32_t position) {
  if(tbl == NULL) {
    return UCOL_NOT_FOUND;
  }
  if((uint32_t)position > tbl->position || position == -1) {
    return UCOL_NOT_FOUND;
  } else {
    return tbl->CEs[position];
  }
}

int32_t uprv_cnttab_findCP(CntTable *table, uint32_t element, UChar codePoint, UErrorCode *status) {

    if(U_FAILURE(*status)) {
        return 0;
    }

    return _cnttab_findCP(_cnttab_getContractionTable(table, element), codePoint);
}

uint32_t uprv_cnttab_getCE(CntTable *table, uint32_t element, uint32_t position, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return UCOL_NOT_FOUND;
    }

    return(_cnttab_getCE(_cnttab_getContractionTable(table, element), position));
}

uint32_t uprv_cnttab_findCE(CntTable *table, uint32_t element, UChar codePoint, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return UCOL_NOT_FOUND;
    }
    ContractionTable *tbl = _cnttab_getContractionTable(table, element);
    return _cnttab_getCE(tbl, _cnttab_findCP(tbl, codePoint));
}

UBool uprv_cnttab_isTailored(CntTable *table, uint32_t element, UChar *ztString, UErrorCode *status) {
    if(U_FAILURE(*status)) {
        return FALSE;
    }

    while(*(ztString)!=0) {
      element = uprv_cnttab_findCE(table, element, *(ztString), status);
      if(element == UCOL_NOT_FOUND) {
        return FALSE;
      }
      if(!isContraction(element)) {
        return TRUE;
      }
      ztString++;
    }
    if(uprv_cnttab_getCE(table, element, 0, status) != UCOL_NOT_FOUND) {
      return TRUE;
    } else {
      return FALSE; 
    }
}

uint32_t uprv_cnttab_changeContraction(CntTable *table, uint32_t element, UChar codePoint, uint32_t newCE, UErrorCode *status) {

    element &= 0xFFFFFF;
    ContractionTable *tbl = NULL;

    if(U_FAILURE(*status)) {
        return 0;
    }

    if((element == 0xFFFFFF) || (tbl = table->elements[element]) == NULL) {
      return 0;
    }

    uint32_t position = 0;

    while(codePoint > tbl->codePoints[position]) {
      position++;
      if(position > tbl->position) {
        return UCOL_NOT_FOUND;
      }
    }
    if (codePoint == tbl->codePoints[position]) {
      tbl->CEs[position] = newCE;
      return element;
    } else {
      return UCOL_NOT_FOUND;
    }
}

