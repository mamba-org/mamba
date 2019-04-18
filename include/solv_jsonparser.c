/*
 * solv_jsonparser.c
 *
 * Simple JSON stream parser
 *
 * Copyright (c) 2018, SUSE LLC
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

#include <stdio.h>
#include <stdlib.h>

#include "solv/util.h"
#include "solv_jsonparser.h"

void
jsonparser_init(struct solv_jsonparser *jp, FILE *fp)
{
  memset(jp, 0, sizeof(*jp));
  jp->fp = fp;
  jp->state = JP_START;
  jp->line = jp->nextline = 1;
  jp->nextc = ' ';
  queue_init(&jp->stateq);
}

void
jsonparser_free(struct solv_jsonparser *jp)
{
  solv_free(jp->space);
  queue_free(&jp->stateq);
}

static void
savec(struct solv_jsonparser *jp, char c)
{
  if (jp->nspace == jp->aspace)
    {
      jp->aspace += 256;
      jp->space = solv_realloc(jp->space, jp->aspace);
    }
  jp->space[jp->nspace++] = c;
}

static void
saveutf8(struct solv_jsonparser *jp, int c)
{
  int i;
  if (c < 0x80)
    {
      savec(jp, c);
      return;
    }
  i = c < 0x800 ? 1 : c < 0x10000 ? 2 : 3;
  savec(jp, (0x1f80 >> i) | (c >> (6 * i)));
  while (--i >= 0)
    savec(jp, 0x80 | ((c >> (6 * i)) & 0x3f));
}

static inline int
nextc(struct solv_jsonparser *jp)
{
  int c = getc(jp->fp);
  if (c == '\n')
    jp->nextline++;
  return c;
}

static int
skipspace(struct solv_jsonparser *jp)
{
  int c = jp->nextc;
  jp->nextc = ' ';
  while (c == ' ' || c == '\t' || c == '\r' || c == '\n')
    c = nextc(jp);
  jp->line = jp->nextline;
  return c;
}

static int
parseliteral(struct solv_jsonparser *jp, int c)
{
  size_t nspace = jp->nspace;
  savec(jp, c);
  for (;;)
    {
      c = nextc(jp);
      if (c < 'a' || c > 'z')
	break;
      savec(jp, c);
    }
  jp->nextc = c;
  savec(jp, 0);
  if (!strcmp(jp->space + nspace, "true"))
    return JP_BOOL;
  if (!strcmp(jp->space + nspace, "false"))
    return JP_BOOL;
  if (!strcmp(jp->space + nspace, "null"))
    return JP_NULL;
  return JP_ERROR;
}

static int
parsenumber(struct solv_jsonparser *jp, int c)
{
  savec(jp, c);
  for (;;)
    {
      c = nextc(jp);
      if ((c < '0' || c > '9') && c != '+' && c != '-' && c != '.' && c != 'e' && c != 'E')
	break;
      savec(jp, c);
    }
  jp->nextc = c;
  savec(jp, 0);
  return JP_NUMBER;
}

static int
parseutf8(struct solv_jsonparser *jp, int surrogate)
{
  int c, i, r = 0;
  /* parse 4-digit hex */
  for (i = 0; i < 4; i++)
    {
      c = nextc(jp);
      if (c >= '0' && c <= '9')
	c -= '0';
      else if (c >= 'a' && c <= 'f')
	c -= 'a' - 10;
      else if (c >= 'A' && c <= 'F')
	c -= 'A' - 10;
      else
	return -1;
      r = (r << 4) | c;
    }
  if (!surrogate && r >= 0xd800 && r < 0xdc00)
    {
      /* utf16 surrogate pair encodes 0x10000 - 0x10ffff */
      int r2;
      if (nextc(jp) != '\\' || nextc(jp) != 'u' || (r2 = parseutf8(jp, 1)) < 0xdc00 || r2 >= 0xe000)
	return -1;
      r = 0x10000 + ((r & 0x3ff) << 10 | (r2 & 0x3ff));
    }
  return r;
}

static int
parsestring(struct solv_jsonparser *jp)
{
  int c;
  for (;;)
    {
      if ((c = nextc(jp)) < 32)
	return JP_ERROR;
      if (c == '"')
	break;
      if (c == '\\')
	{
	  switch (c = nextc(jp))
	    {
	    case '"':
	    case '\\':
	    case '/':
	    case '\n':
	      break;
	    case 'b':
	      c = '\b';
	      break;
	    case 'f':
	      c = '\f';
	      break;
	    case 'n':
	      c = '\n';
	      break;
	    case 'r':
	      c = '\r';
	      break;
	    case 't':
	      c = '\t';
	      break;
	    case 'u':
	      if ((c = parseutf8(jp, 0)) < 0)
		return JP_ERROR;
	      saveutf8(jp, c);
	      continue;
	    default:
	      return JP_ERROR;
	    }
	}
      savec(jp, c);
    }
  savec(jp, 0);
  return JP_STRING;
}

static int
parsevalue(struct solv_jsonparser *jp)
{
  int c = skipspace(jp);
  if (c == '"')
    return parsestring(jp);
  if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')
    return parsenumber(jp, c);
  if ((c >= 'a' && c <= 'z'))
    return parseliteral(jp, c);
  if (c == '[')
    return JP_ARRAY;
  if (c == '{')
    return JP_OBJECT;
  if (c == ']')
    return JP_ARRAY_END;
  if (c == '}')
    return JP_OBJECT_END;
  return JP_ERROR;
}

int
jsonparser_parse(struct solv_jsonparser *jp)
{
  int type;
  size_t nspace;

  jp->depth = jp->stateq.count;
  jp->key = jp->value = 0;
  jp->keylen = jp->valuelen = 0;
  nspace = jp->nspace = 0;

  if (jp->state == JP_END)
    return JP_END;
  if (jp->state == JP_START)
    jp->state = JP_END;
  type = parsevalue(jp);
  if (type <= 0)
    return JP_ERROR;
  if (type == JP_OBJECT_END || type == JP_ARRAY_END)
    {
      if (jp->state != type - 1)
        return JP_ERROR;
      jp->state = queue_pop(&jp->stateq);
    }
  else if (jp->state == JP_OBJECT)
    {
      nspace = jp->nspace;
      if (type != JP_STRING)
	return JP_ERROR;
      if (skipspace(jp) != ':')
	return JP_ERROR;
      type = parsevalue(jp);
      if (type == JP_OBJECT_END || type == JP_ARRAY_END)
	return JP_ERROR;
      jp->key = jp->space;
      jp->keylen = nspace - 1;
    }
  if (type == JP_STRING || type == JP_NUMBER || type == JP_BOOL || type == JP_NULL)
    {
      jp->value = jp->space + nspace;
      jp->valuelen = jp->nspace - nspace - 1;
    }
  if (type == JP_OBJECT || type == JP_ARRAY)
    {
      queue_push(&jp->stateq, jp->state);
      jp->state = type;
    }
  else if (jp->state == JP_OBJECT || jp->state == JP_ARRAY)
    {
      int c = skipspace(jp);
      if (c == (jp->state == JP_OBJECT ? '}' : ']'))
	jp->nextc = c;
      else if (c != ',')
	return JP_ERROR;
    }
  return type;
}

int
jsonparser_skip(struct solv_jsonparser *jp, int type)
{
  if (type == JP_ARRAY || type == JP_OBJECT)
    {
      int depth = jp->depth + 1, endtype = type + 1;
      while (type > 0 && (type != endtype || jp->depth != depth))
        type = jsonparser_parse(jp);
    }
  return type;
}

