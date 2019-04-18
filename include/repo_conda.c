/*
 * Copyright (c) 2019, SUSE LLC.
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "solv/pool.h"
#include "solv/repo.h"
#include "solv/chksum.h"
#include "solv_jsonparser.h"
#include "solv/conda.h"
#include "solv/repo_conda.h"

struct parsedata {
  Pool *pool;
  Repo *repo;
  Repodata *data;
};

static int
parse_deps(struct parsedata *pd, struct solv_jsonparser *jp, Offset *depp)
{
  int type = JP_ARRAY;
  while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_ARRAY_END)
    {
      if (type == JP_STRING)
  {
    Id id = pool_conda_matchspec(pd->pool, jp->value);
    if (id)
      *depp = repo_addid_dep(pd->repo, *depp, id, 0);
  }
      else
  type = jsonparser_skip(jp, type);
    }
  return type;
}

static int
parse_package(struct parsedata *pd, struct solv_jsonparser *jp, char *kfn)
{
  int type = JP_OBJECT;
  Pool *pool= pd->pool;
  Repodata *data = pd->data;
  Solvable *s;
  Id handle = repo_add_solvable(pd->repo);
  s = pool_id2solvable(pool, handle);
  char *fn = 0;
  char *subdir = 0;

  while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_OBJECT_END)
    {
      if (type == JP_STRING && !strcmp(jp->key, "build"))
  repodata_add_poolstr_array(data, handle, SOLVABLE_BUILDFLAVOR, jp->value);
      else if (type == JP_NUMBER && !strcmp(jp->key, "build_number"))
  repodata_set_num(data, handle, SOLVABLE_BUILDVERSION, strtoull(jp->value, 0, 10));
      else if (type == JP_ARRAY && !strcmp(jp->key, "depends"))
  type = parse_deps(pd, jp, &s->requires);
      else if (type == JP_ARRAY && !strcmp(jp->key, "requires"))
  type = parse_deps(pd, jp, &s->requires);
      else if (type == JP_STRING && !strcmp(jp->key, "license"))
  repodata_add_poolstr_array(data, handle, SOLVABLE_LICENSE, jp->value);
      else if (type == JP_STRING && !strcmp(jp->key, "md5"))
  repodata_set_checksum(data, handle, SOLVABLE_PKGID, REPOKEY_TYPE_MD5, jp->value);
      else if (type == JP_STRING && !strcmp(jp->key, "sha256"))
  repodata_set_checksum(data, handle, SOLVABLE_CHECKSUM, REPOKEY_TYPE_SHA256, jp->value);
      else if (type == JP_STRING && !strcmp(jp->key, "name"))
  s->name = pool_str2id(pool, jp->value, 1);
      else if (type == JP_STRING && !strcmp(jp->key, "version"))
  s->evr= pool_str2id(pool, jp->value, 1);
      else if (type == JP_STRING && !strcmp(jp->key, "fn") && !fn)
  fn = solv_strdup(jp->value);
      else if (type == JP_STRING && !strcmp(jp->key, "subdir") && !subdir)
  subdir = solv_strdup(jp->value);
      else if (type == JP_NUMBER && !strcmp(jp->key, "size"))
  repodata_set_num(data, handle, SOLVABLE_DOWNLOADSIZE, strtoull(jp->value, 0, 10));
      else if (type == JP_NUMBER && !strcmp(jp->key, "timestamp"))
  {
    unsigned long long ts = strtoull(jp->value, 0, 10);
    if (ts > 253402300799ULL)
      ts /= 1000;
    repodata_set_num(data, handle, SOLVABLE_BUILDTIME, ts);
  }
      else
  type = jsonparser_skip(jp, type);
    }
  if (fn || kfn)
    repodata_set_location(data, handle, 0, subdir, fn ? fn : kfn);
  solv_free(fn);
  solv_free(subdir);
  if (!s->evr)
    s->evr = 1;
  if (s->name)
    s->provides = repo_addid_dep(pd->repo, s->provides, pool_rel2id(pool, s->name, s->evr, REL_EQ, 1), 0);
  return type;
}

static int
parse_packages(struct parsedata *pd, struct solv_jsonparser *jp)
{
  int type = JP_OBJECT;
  while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_OBJECT_END)
    {
      if (type == JP_OBJECT)
  {
    char *fn = solv_strdup(jp->key);
    type = parse_package(pd, jp, fn);
    solv_free(fn);
  }
      else
  type = jsonparser_skip(jp, type);
    }
  return type;
}

static int
parse_packages2(struct parsedata *pd, struct solv_jsonparser *jp)
{
  int type = JP_ARRAY;
  while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_ARRAY_END)
    {
      if (type == JP_OBJECT)
  type = parse_package(pd, jp, 0);
      else
  type = jsonparser_skip(jp, type);
    }
  return type;
}

static int
parse_main(struct parsedata *pd, struct solv_jsonparser *jp)
{
  int type = JP_OBJECT;
  while (type > 0 && (type = jsonparser_parse(jp)) > 0 && type != JP_OBJECT_END)
    {
      if (type == JP_OBJECT && !strcmp("packages", jp->key))
  type = parse_packages(pd, jp);
      if (type == JP_ARRAY && !strcmp("packages", jp->key))
  type = parse_packages2(pd, jp);
      else
  type = jsonparser_skip(jp, type);
    }
  return type;
}

int
repo_add_conda(Repo *repo, FILE *fp, int flags)
{
  Pool *pool = repo->pool;
  struct solv_jsonparser jp;
  struct parsedata pd;
  Repodata *data;
  int type, ret = 0;

  data = repo_add_repodata(repo, flags);

  memset(&pd, 0, sizeof(pd));
  pd.pool = pool;
  pd.repo = repo;
  pd.data = data;

  jsonparser_init(&jp, fp);
  if ((type = jsonparser_parse(&jp)) != JP_OBJECT)
    ret = pool_error(pool, -1, "repository does not start with an object");
  else if ((type = parse_main(&pd, &jp)) != JP_OBJECT_END)
    ret = pool_error(pool, -1, "parse error line %d", jp.line);
  jsonparser_free(&jp);

  if (!(flags & REPO_NO_INTERNALIZE))
    repodata_internalize(data);

  return ret;
}

