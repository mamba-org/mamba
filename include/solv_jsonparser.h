/*
 * Copyright (c) 2018, SUSE LLC
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

#ifndef SOLV_JSONPARSER_H
#define SOLV_JSONPARSER_H

#include "solv/queue.h"

struct solv_jsonparser {
  FILE *fp;
  int line;
  int depth;

  char *key;
  size_t keylen;
  char *value;
  size_t valuelen;

  int state;		/* START, END, OBJECT, ARRAY */
  Queue stateq;
  int nextc;
  int nextline;
  char *space;
  size_t nspace;
  size_t aspace;
};

#define JP_ERROR	-1
#define JP_END		0
#define JP_START	1
#define JP_STRING	2
#define JP_NUMBER	3
#define JP_BOOL		4
#define JP_NULL		5
#define JP_OBJECT	6
#define JP_OBJECT_END	7
#define JP_ARRAY	8
#define JP_ARRAY_END	9

void jsonparser_init(struct solv_jsonparser *jp, FILE *fp);
void jsonparser_free(struct solv_jsonparser *jp);
int jsonparser_parse(struct solv_jsonparser *jp);
int jsonparser_skip(struct solv_jsonparser *jp, int type);

#endif /* SOLV_JSONPARSER_H */
