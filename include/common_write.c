/*
 * Copyright (c) 2007, Novell Inc.
 *
 * This program is licensed under the BSD license, read LICENSE.BSD
 * for further information
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <solv/pool.h>
#include <solv/repo.h>
#include <solv/repo_write.h>
#include <solv/solvversion.h>

/* toolversion history
 * 1.0: initial tool version
 * 1.1: changed PRODUCT_ENDOFLIFE parsing
*/

static int
keyfilter_solv(Repo *repo, Repokey *key, void *kfdata)
{
  if (key->name == SUSETAGS_SHARE_NAME || key->name == SUSETAGS_SHARE_EVR || key->name == SUSETAGS_SHARE_ARCH)
    return KEY_STORAGE_DROPPED;
  return repo_write_stdkeyfilter(repo, key, kfdata);
}

/*
 * Write <repo> to fp
 */
inline void
tool_write(Repo *repo, FILE *fp)
{
  Repodata *info;
  Queue addedfileprovides;
  Repowriter *writer;

  info = repo_add_repodata(repo, 0);	/* add new repodata for our meta info */
  repodata_set_str(info, SOLVID_META, REPOSITORY_TOOLVERSION, LIBSOLV_TOOLVERSION);
  repodata_unset(info, SOLVID_META, REPOSITORY_EXTERNAL);	/* do not propagate this */

  queue_init(&addedfileprovides);
  pool_addfileprovides_queue(repo->pool, &addedfileprovides, 0);
  if (addedfileprovides.count)
    repodata_set_idarray(info, SOLVID_META, REPOSITORY_ADDEDFILEPROVIDES, &addedfileprovides);
  else
    repodata_unset(info, SOLVID_META, REPOSITORY_ADDEDFILEPROVIDES);
  queue_free(&addedfileprovides);

  pool_freeidhashes(repo->pool);	/* free some mem */

  repodata_internalize(info);
  writer = repowriter_create(repo);
  repowriter_set_keyfilter(writer, keyfilter_solv, 0);
  if (repowriter_write(writer, fp) != 0)
    {
      fprintf(stderr, "repo write failed: %s\n", pool_errstr(repo->pool));
      exit(1);
    }
  if (fflush(fp))
    {
      perror("fflush");
      exit(1);
    }
  repowriter_free(writer);
  repodata_free(info);		/* delete meta info repodata again */
}
