/* $Id: conf.c,v 1.4.4.2 2011/12/05 20:26:53 bew Exp $ */
/* $Source: /nfs/cscbz/gdoi/gdoicvs/gdoi/src/conf.c,v $ */

/*	$OpenBSD: conf.c,v 1.30 2001/03/27 15:46:29 ho Exp $	*/
/*	$EOM: conf.c,v 1.48 2000/12/04 02:04:29 angelos Exp $	*/

/* 
 * The license applies to all software incorporated in the "Cisco GDOI reference
 * implementation" except for those portions incorporating third party software 
 * specifically identified as being licensed under separate license. 
 *  
 *  
 * The Cisco Systems Public Software License, Version 1.0 
 * Copyright (c) 2001-1011 Cisco Systems, Inc. All rights reserved.
 * Subject to the following terms and conditions, Cisco Systems, Inc., 
 * hereby grants you a worldwide, royalty-free, nonexclusive, license, 
 * subject to third party intellectual property claims, to create 
 * derivative works of the Licensed Code and to reproduce, display, 
 * perform, sublicense, distribute such Licensed Code and derivative works. 
 * All rights not expressly granted herein are reserved. 
 * 1.      Redistributions of source code must retain the above 
 * copyright notice, this list of conditions and the following 
 * disclaimer.
 * 2.      Redistributions in binary form must reproduce the above 
 * copyright notice, this list of conditions and the following 
 * disclaimer in the documentation and/or other materials 
 * provided with the distribution.
 * 3.      The names Cisco and "Cisco GDOI reference implementation" must not 
 * be used to endorse or promote products derived from this software without 
 * prior written permission. For written permission, please contact 
 * opensource@cisco.com.
 * 4.      Products derived from this software may not be called 
 * "Cisco" or "Cisco GDOI reference implementation", nor may "Cisco" or 
 * "Cisco GDOI reference implementation" appear in 
 * their name, without prior written permission of Cisco Systems, Inc.
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
 * PURPOSE, TITLE AND NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT 
 * SHALL CISCO SYSTEMS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY 
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF 
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. THIS LIMITATION OF LIABILITY SHALL NOT APPLY TO 
 * LIABILITY FOR DEATH OR PERSONAL INJURY RESULTING FROM SUCH 
 * PARTY'S NEGLIGENCE TO THE EXTENT APPLICABLE LAW PROHIBITS SUCH 
 * LIMITATION. SOME JURISDICTIONS DO NOT ALLOW THE EXCLUSION OR 
 * LIMITATION OF INCIDENTAL OR CONSEQUENTIAL DAMAGES, SO THAT 
 * EXCLUSION AND LIMITATION MAY NOT APPLY TO YOU. FURTHER, YOU 
 * AGREE THAT IN NO EVENT WILL CISCO'S LIABILITY UNDER OR RELATED TO 
 * THIS AGREEMENT EXCEED AMOUNT FIVE THOUSAND DOLLARS (US) 
 * (US$5,000). 
 *  
 * ====================================================================
 * This software consists of voluntary contributions made by Cisco Systems, 
 * Inc. and many individuals on behalf of Cisco Systems, Inc. For more 
 * information on Cisco Systems, Inc., please see <http://www.cisco.com/>.
 *
 * This product includes software developed by Ericsson Radio Systems.
 */ 


/*
 * Copyright (c) 1998, 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2000, 2001 H�kan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "sysdep.h"

#include "app.h"
#include "conf.h"
#include "log.h"
#include "util.h"

struct conf_trans {
  TAILQ_ENTRY (conf_trans) link;
  int trans;
  enum conf_op { CONF_SET, CONF_REMOVE, CONF_REMOVE_SECTION } op;
  char *section;
  char *tag;
  char *value;
  int override;
  int is_default;
};

TAILQ_HEAD (conf_trans_head, conf_trans) conf_trans_queue;

/*
 * Radix-64 Encoding.
 */
const u_int8_t bin2asc[]
  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const u_int8_t asc2bin[] =
{
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255,  62, 255, 255, 255,  63,
   52,  53,  54,  55,  56,  57,  58,  59,
   60,  61, 255, 255, 255, 255, 255, 255,
  255,   0,   1,   2,   3,   4,   5,   6,
    7,   8,   9,  10,  11,  12,  13,  14,
   15,  16,  17,  18,  19,  20,  21,  22,
   23,  24,  25, 255, 255, 255, 255, 255,
  255,  26,  27,  28,  29,  30,  31,  32,
   33,  34,  35,  36,  37,  38,  39,  40,
   41,  42,  43,  44,  45,  46,  47,  48,
   49,  50,  51, 255, 255, 255, 255, 255
};

struct conf_binding {
  LIST_ENTRY (conf_binding) link;
  char *section;
  char *tag;
  char *value;
  int is_default;
};

char *conf_path = CONFIG_FILE;
LIST_HEAD (conf_bindings, conf_binding) conf_bindings[256];

static char *conf_addr;

static __inline__ u_int8_t
conf_hash (char *s)
{
  u_int8_t hash = 0;

  while (*s)
    {
      hash = ((hash << 1) | (hash >> 7)) ^ tolower (*s);
      s++;
    }
  return hash;
}

/*
 * Insert a tag-value combination from LINE (the equal sign is at POS)
 */
static int
conf_remove_now (char *section, char *tag)
{
  struct conf_binding *cb, *next;

  for (cb = LIST_FIRST (&conf_bindings[conf_hash (section)]); cb; cb = next)
    {
      next = LIST_NEXT (cb, link);
      if (strcasecmp (cb->section, section) == 0
	  && strcasecmp (cb->tag, tag) == 0)
	{
	  LIST_REMOVE (cb, link);
	  LOG_DBG ((LOG_MISC, 70, "[%s]:%s->%s removed", section, tag,
		    cb->value));
	  free (cb->section);
	  free (cb->tag);
	  free (cb->value);
	  free (cb);
	  return 0;
	}
    }
  return 1;
}

static int
conf_remove_section_now (char *section)
{
  struct conf_binding *cb, *next;
  int unseen = 1;

  for (cb = LIST_FIRST (&conf_bindings[conf_hash (section)]); cb; cb = next)
    {
      next = LIST_NEXT (cb, link);
      if (strcasecmp (cb->section, section) == 0)
	{
	  unseen = 0;
	  LIST_REMOVE (cb, link);
	  LOG_DBG ((LOG_MISC, 70, "[%s]:%s->%s removed", section, cb->tag,
		    cb->value));
	  free (cb->section);
	  free (cb->tag);
	  free (cb->value);
	  free (cb);
	}
    }
  return unseen;
}

/*
 * Insert a tag-value combination from LINE (the equal sign is at POS)
 * into SECTION of our configuration database.
 */
static int
conf_set_now (char *section, char *tag, char *value, int override,
	      int is_default)
{
  struct conf_binding *node = 0;

  if (override)
    conf_remove_now (section, tag);
  else if (conf_get_str (section, tag))
    {
      if (!is_default)
	log_print ("conf_set: duplicate tag [%s]:%s, ignoring...\n", section,
		   tag);
      return 1;
    }

  node = calloc (1, sizeof *node);
  if (!node)
    {
      log_error ("conf_set: calloc (1, %d) failed", sizeof *node);
      return 1;
    }
  node->section = strdup (section);
  node->tag = strdup (tag);
  node->value = strdup (value);
  node->is_default = is_default;

  LIST_INSERT_HEAD (&conf_bindings[conf_hash (section)], node, link);
  LOG_DBG ((LOG_MISC, 70, "conf_set: [%s]:%s->%s", node->section, node->tag,
	    node->value));
  return 0;
}

/*
 * Parse the line LINE of SZ bytes.  Skip Comments, recognize section
 * headers and feed tag-value pairs into our configuration database.
 */
static void
conf_parse_line (int trans, char *line, size_t sz)
{
  char *cp = line;
  int i;
  static char *section = 0;
  static int ln = 0;

  ln++;

  /* Lines starting with '#' or ';' are comments.  */
  if (*line == '#' || *line == ';')
    return;

  /* '[section]' parsing...  */
  if (*line == '[')
    {
      for (i = 1; i < sz; i++)
	if (line[i] == ']')
	  break;
      if (i == sz)
	{
	  log_print ("conf_parse_line: %d:"
		     "non-matched ']', ignoring until next section", ln);
	  section = 0;
	  return;
	}
      if (section)
	free (section);
      section = malloc (i);
      strncpy (section, line + 1, i - 1);
      section[i - 1] = '\0';
      return;
    }

  /* Deal with assignments.  */
  for (i = 0; i < sz; i++)
    if (cp[i] == '=')
      {
	/* If no section, we are ignoring the lines.  */
	if (!section)
	  {
	    log_print ("conf_parse_line: %d: ignoring line due to no section",
		       ln);
	    return;
	  }
	line[strcspn (line, " \t=")] = '\0';
	/* XXX Perhaps should we not ignore errors?  */
	conf_set (trans, section, line,
		  line + i + 1 + strspn (line + i + 1, " \t"), 0, 0);
	return;
      }

  /* Other non-empty lines are wierd.  */
  i = strspn (line, " \t");
  if (line[i])
    log_print ("conf_parse_line: %d: syntax error", ln);

  return;
}

/* Parse the mapped configuration file.  */
static void
conf_parse (int trans, char *buf, size_t sz)
{
  char *cp = buf;
  char *bufend = buf + sz;
  char *line;

  line = cp;
  while (cp < bufend)
    {
      if (*cp == '\n')
	{
	  /* Check for escaped newlines.  */
	  if (cp > buf && *(cp - 1) == '\\')
	    *(cp - 1) = *cp = ' ';
	  else
	    {
	      *cp = '\0';
	      conf_parse_line (trans, line, cp - line);
	      line = cp + 1;
	    }
	}
      cp++;
    }
  if (cp != line)
    log_print ("conf_parse: last line non-terminated, ignored.");
}

/*
 * Auto-generate default configuration values for the transforms and
 * suites the user wants.
 *
 * Resulting section names can be:
 *  For main mode:
 *     {DES,BLF,3DES,CAST}-{MD5,SHA}[-{DSS,RSA_SIG}]
 *  For quick mode:
 *     QM-{ESP,AH}[-TRP]-{DES,3DES,CAST,BLF,AES}[-{MD5,SHA,RIPEMD}][-PFS]-SUITE
 * DH groups; currently always MODP_768 for MD5, and MODP_1024 for SHA.
 *
 * XXX We may want to support USE_BLOWFISH, USE_TRIPLEDES, etc...
 * XXX No EC2N DH support here yet.
 */

/* Find the value for a section+tag in the transaction list */
char *
conf_get_trans_str (int trans, char *section, char *tag)
{
  struct conf_trans *node, *nf = 0;
  
  for (node = TAILQ_FIRST (&conf_trans_queue); node;
       node = TAILQ_NEXT (node, link))
    if (node->trans == trans && strcmp (section, node->section) == 0 && 
	strcmp (tag, node->tag) == 0)
      {
	if (!nf)
	  nf = node;
	else if (node->override)
	  nf = node;
      }
  return nf ? nf->value : NULL;
}

int
conf_find_trans_xf (int phase, char *xf)
{
  struct conf_trans *node;
  char *p;

  /* Find the relevant transforms and suites, if any.  */
  for (node = TAILQ_FIRST (&conf_trans_queue); node;
       node = TAILQ_NEXT (node, link))
    if ((phase == 1 && strcmp ("Transforms", node->tag) == 0) ||
	(phase == 2 && strcmp ("Suites", node->tag) == 0))
      {
	p = node->value;
	while ((p = strstr (p, xf)) != NULL)
	  if (*(p + strlen (p)) && *(p + strlen (p)) != ',')
	    p += strlen (p);
	  else
	    return 1;
      }
  return 0;
}

void
conf_init (void)
{
  int i;

  for (i = 0; i < sizeof conf_bindings / sizeof conf_bindings[0]; i++)
    LIST_INIT (&conf_bindings[i]);
  TAILQ_INIT (&conf_trans_queue);
  conf_reinit ();
}

/* Open the config file and map it into our address space, then parse it.  */
void
conf_reinit (void)
{
  struct conf_binding *cb = 0;
  int fd, i, trans;
  off_t sz;
  char *new_conf_addr = 0;
  struct stat sb;

  if ((stat (conf_path, &sb) == 0) || (errno != ENOENT))
    {
      if (check_file_secrecy (conf_path, &sz))
	return;

      fd = open (conf_path, O_RDONLY);
      if (fd == -1)
        {
	  log_error ("conf_reinit: open (\"%s\", O_RDONLY) failed", conf_path);
	  return;
	}

      new_conf_addr = malloc (sz);
      if (!new_conf_addr)
        {
	  log_error ("conf_reinit: malloc (%d) failed", sz);
	  goto fail;
	}

      /* XXX I assume short reads won't happen here.  */
      if (read (fd, new_conf_addr, sz) != sz)
        {
	    log_error ("conf_reinit: read (%d, %p, %d) failed",
		       fd, new_conf_addr, sz);
	    goto fail;
	}
      close (fd);

      trans = conf_begin ();

      /* XXX Should we not care about errors and rollback?  */
      conf_parse (trans, new_conf_addr, sz);
    }
  else
    trans = conf_begin ();

  /* Free potential existing configuration.  */
  if (conf_addr)
    {
      for (i = 0; i < sizeof conf_bindings / sizeof conf_bindings[0]; i++)
	for (cb = LIST_FIRST (&conf_bindings[i]); cb;
	     cb = LIST_FIRST (&conf_bindings[i]))
	  conf_remove_now (cb->section, cb->tag);
      free (conf_addr);
    }

  conf_end (trans, 1);
  conf_addr = new_conf_addr;
  return;

 fail:
  if (new_conf_addr)
    free (new_conf_addr);
  close (fd);
}

/*
 * Return the numeric value denoted by TAG in section SECTION or DEF
 * if that tag does not exist.
 */
int
conf_get_num (char *section, char *tag, int def)
{
  char *value = conf_get_str (section, tag);

  if (value)
      return atoi (value);
  return def;
}

/* Validate X according to the range denoted by TAG in section SECTION.  */
int
conf_match_num (char *section, char *tag, int x)
{
  char *value = conf_get_str (section, tag);
  int val, min, max, n;

  if (!value)
    return 0;
  n = sscanf (value, "%d,%d:%d", &val, &min, &max);
  switch (n)
    {
    case 1:
      LOG_DBG ((LOG_MISC, 90, "conf_match_num: %s:%s %d==%d?", section, tag,
		val, x));
      return x == val;
    case 3:
      LOG_DBG ((LOG_MISC, 90, "conf_match_num: %s:%s %d<=%d<=%d?", section,
		tag, min, x, max));
      return min <= x && max >= x;
    default:
      log_error ("conf_match_num: section %s tag %s: invalid number spec %s",
		 section, tag, value);
    }
  return 0;
}

/* Return the string value denoted by TAG in section SECTION.  */
char *
conf_get_str (char *section, char *tag)
{
  struct conf_binding *cb;

  for (cb = LIST_FIRST (&conf_bindings[conf_hash (section)]); cb;
       cb = LIST_NEXT (cb, link))
    if (strcasecmp (section, cb->section) == 0
	&& strcasecmp (tag, cb->tag) == 0)
      {
	LOG_DBG ((LOG_MISC, 60, "conf_get_str: [%s]:%s->%s", section,
		  tag, cb->value));
	return cb->value;
      }
#if BEW_DO_WE_REALLY_NEED_THIS_ANNOYING_MESSAGE
  LOG_DBG ((LOG_MISC, 60,
	    "conf_get_str: configuration value not found [%s]:%s", section,
	    tag));
#endif
  return 0;
}

/*
 * Build a list of string values out of the comma separated value denoted by
 * TAG in SECTION.
 */
struct conf_list *
conf_get_list (char *section, char *tag)
{
  char *liststr = 0, *p, *field;
  struct conf_list *list = 0;
  struct conf_list_node *node;

  list = malloc (sizeof *list);
  if (!list)
    goto cleanup;
  TAILQ_INIT (&list->fields);
  list->cnt = 0;
  liststr = conf_get_str (section, tag);
  if (!liststr)
    goto cleanup;
  liststr = strdup (liststr);
  if (!liststr)
    goto cleanup;
  p = liststr;
  while ((field = strsep (&p, ", \t")) != NULL)
    {
      if (*field == '\0')
	{
	  log_print ("conf_get_list: empty field, ignoring...");
	  continue;
	}
      list->cnt++;
      node = calloc (1, sizeof *node);
      if (!node)
	goto cleanup;
      node->field = strdup (field);
      if (!node->field)
	goto cleanup;
      TAILQ_INSERT_TAIL (&list->fields, node, link);
    }
  free (liststr);
  return list;

 cleanup:
  if (list)
    conf_free_list (list);
  if (liststr)
    free (liststr);
  return 0;
}

struct conf_list *
conf_get_tag_list (char *section)
{
  struct conf_list *list = 0;
  struct conf_list_node *node;
  struct conf_binding *cb;

  list = malloc (sizeof *list);
  if (!list)
    goto cleanup;
  TAILQ_INIT (&list->fields);
  list->cnt = 0;
  for (cb = LIST_FIRST (&conf_bindings[conf_hash (section)]); cb;
       cb = LIST_NEXT (cb, link))
    if (strcasecmp (section, cb->section) == 0)
      {
	list->cnt++;
	node = calloc (1, sizeof *node);
	if (!node)
	  goto cleanup;
	node->field = strdup (cb->tag);
	if (!node->field)
	  goto cleanup;
	TAILQ_INSERT_TAIL (&list->fields, node, link);
      }
  return list;

 cleanup:
  if (list)
    conf_free_list (list);
  return 0;
}

/* Decode a PEM encoded buffer.  */
int
conf_decode_base64 (u_int8_t *out, u_int32_t *len, u_char *buf)
{
  u_int32_t c = 0;
  u_int8_t c1, c2, c3, c4;

  while (*buf)
    {
      if (*buf > 127 || (c1 = asc2bin[*buf]) == 255)
	return 0;
      buf++;

      if (*buf > 127 || (c2 = asc2bin[*buf]) == 255)
	return 0;
      buf++;

      if (*buf == '=')
	{
	  c3 = c4 = 0;
	  c++;

	  /* Check last four bit */
	  if (c2 & 0xF)
	    return 0;

	  if (strcmp ((char *)buf, "==") == 0)
	    buf++;
	  else
	    return 0;
	}
      else if (*buf > 127 || (c3 = asc2bin[*buf]) == 255)
	return 0;
      else
	{
	  if (*++buf == '=')
	    {
	      c4 = 0;
	      c += 2;

	      /* Check last two bit */
	      if (c3 & 3)
		return 0;

	      if (strcmp ((char *)buf, "="))
		return 0;

	    }
	  else if (*buf > 127 || (c4 = asc2bin[*buf]) == 255)
	      return 0;
	  else
	      c += 3;
	}

      buf++;
      *out++ = (c1 << 2) | (c2 >> 4);
      *out++ = (c2 << 4) | (c3 >> 2);
      *out++ = (c3 << 6) | c4;
    }

  *len = c;
  return 1;

}

/* Read a line from a stream to the buffer.  */
int
conf_get_line (FILE *stream, char *buf, u_int32_t len)
{
  int c;

  while (len-- > 1)
    {
      c = fgetc (stream);
      if (c == '\n')
	{
	  *buf = 0;
	  return 1;
	}
      else if (c == EOF)
	break;

      *buf++ = c;
    }

  *buf = 0;
  return 0;
}

void
conf_free_list (struct conf_list *list)
{
  struct conf_list_node *node = TAILQ_FIRST (&list->fields);

  while (node)
    {
      TAILQ_REMOVE (&list->fields, node, link);
      if (node->field)
	free (node->field);
      free (node);
      node = TAILQ_FIRST (&list->fields);
    }
  free (list);
}

int
conf_begin (void)
{
  static int seq = 0;

  return ++seq;
}

static struct conf_trans *
conf_trans_node (int transaction, enum conf_op op)
{
  struct conf_trans *node;

  node = calloc (1, sizeof *node);
  if (!node)
    {
      log_error ("conf_trans_node: calloc (1, %d) failed", sizeof *node);
      return 0;
    }
  node->trans = transaction;
  node->op = op;
  TAILQ_INSERT_TAIL (&conf_trans_queue, node, link);
  return node;
}

/* Queue a set operation.  */
int
conf_set (int transaction, char *section, char *tag, char *value, int override,
	  int is_default)
{
  struct conf_trans *node;

  node = conf_trans_node (transaction, CONF_SET);
  if (!node)
    return 1;
  node->section = strdup (section);
  if (!node->section)
    {
      log_error ("conf_set: strdup (\"%s\") failed", section);
      goto fail;
    }
  node->tag = strdup (tag);
  if (!node->tag)
    {
      log_error ("conf_set: strdup (\"%s\") failed", tag);
      goto fail;
    }
  node->value = strdup (value);
  if (!node->value)
    {
      log_error ("conf_set: strdup (\"%s\") failed", value);
      goto fail;
    }
  node->override = override;
  node->is_default = is_default;
  return 0;

 fail:
  if (node->tag)
    free (node->tag);
  if (node->section)
    free (node->section);
  if (node)
    free (node);
  return 1;
}

/* Queue a remove operation.  */
int
conf_remove (int transaction, char *section, char *tag)
{
  struct conf_trans *node;

  node = conf_trans_node (transaction, CONF_REMOVE);
  if (!node)
    goto fail;
  node->section = strdup (section);
  if (!node->section)
    {
      log_error ("conf_remove: strdup (\"%s\") failed", section);
      goto fail;
    }
  node->tag = strdup (tag);
  if (!node->tag)
    {
      log_error ("conf_remove: strdup (\"%s\") failed", tag);
      goto fail;
    }
  return 0;

 fail:
  if (node->section)
    free (node->section);
  if (node)
    free (node);
  return 1;
}

/* Queue a remove section operation.  */
int
conf_remove_section (int transaction, char *section)
{
  struct conf_trans *node;

  node = conf_trans_node (transaction, CONF_REMOVE_SECTION);
  if (!node)
    goto fail;
  node->section = strdup (section);
  if (!node->section)
    {
      log_error ("conf_remove_section: strdup (\"%s\") failed", section);
      goto fail;
    }
  return 0;

 fail:
  if (node)
    free (node);
  return 1;
}

/* Execute all queued operations for this transaction.  Cleanup.  */
int
conf_end (int transaction, int commit)
{
  struct conf_trans *node, *next;

  for (node = TAILQ_FIRST (&conf_trans_queue); node; node = next)
    {
      next = TAILQ_NEXT (node, link);
      if (node->trans == transaction)
	{
	  if (commit)
	    switch (node->op)
	      {
	      case CONF_SET:
		conf_set_now (node->section, node->tag, node->value,
			      node->override, node->is_default);
		break;
	      case CONF_REMOVE:
		conf_remove_now (node->section, node->tag);
		break;
	      case CONF_REMOVE_SECTION:
		conf_remove_section_now (node->section);
		break;
	      default:
		log_print ("conf_end: unknown operation: %d", node->op);
	      }
	  TAILQ_REMOVE (&conf_trans_queue, node, link);
	  if (node->section)
	    free (node->section);
	  if (node->tag)
	    free (node->tag);
	  if (node->value)
	    free (node->value);
	  free (node);
	}
    }
  return 0;
}

/*
 * Dump running configuration upon SIGUSR1.
 * XXX Configuration is "stored in reverse order", so reverse it.
 */
struct dumper {
  char *s, *v;
  struct dumper *next;
};

static void
conf_report_dump (struct dumper *node)
{
  /* Recursive, cleanup when we're done.  */

  if (node->next)
    conf_report_dump (node->next);

  if (node->v)
    LOG_DBG ((LOG_REPORT, 0, "%s=\t%s", node->s, node->v));
  else if (node->s)
    {
      LOG_DBG ((LOG_REPORT, 0, "%s", node->s));
      if (strlen (node->s) > 0)
	free (node->s);
    }

  free (node);
}

void
conf_report (void)
{
  struct conf_binding *cb, *last = NULL;
  int i;
  char *current_section = (char *)0;
  struct dumper *dumper, *dnode;

  dumper = dnode = (struct dumper *)calloc (1, sizeof *dumper);
  if (!dumper)
    goto mem_fail;

  LOG_DBG ((LOG_REPORT, 0, "conf_report: dumping running configuration"));

  for (i = 0; i < sizeof conf_bindings / sizeof conf_bindings[0]; i++)
    for (cb = LIST_FIRST (&conf_bindings[i]); cb;
	 cb = LIST_NEXT (cb, link))
      {
	if (!cb->is_default)
	  {
	    /* Dump this entry */
	    if (!current_section || strcmp (cb->section, current_section))
	      {
		if (current_section)
		  {
		    dnode->s = malloc (strlen (current_section) + 3);
		    if (!dnode->s)
		      goto mem_fail;

		    sprintf (dnode->s, "[%s]", current_section);
		    dnode->next
		      = (struct dumper *)calloc (1, sizeof (struct dumper));
		    dnode = dnode->next;
		    if (!dnode)
		      goto mem_fail;

		    dnode->s = "";
		    dnode->next
		      = (struct dumper *)calloc (1, sizeof (struct dumper));
		    dnode = dnode->next;
		    if (!dnode)
		      goto mem_fail;
		  }
		current_section = cb->section;
	      }
	    dnode->s = cb->tag;
	    dnode->v = cb->value;
	    dnode->next = (struct dumper *)calloc (1, sizeof (struct dumper));
	    dnode = dnode->next;
	    if (!dnode)
	      goto mem_fail;
	    last = cb;
	  }
      }

  if (last)
    {
      dnode->s = malloc (strlen (last->section) + 3);
      if (!dnode->s)
	goto mem_fail;
      sprintf (dnode->s, "[%s]", last->section);
    }

  conf_report_dump (dumper);

  return;

 mem_fail:
  LOG_DBG ((LOG_REPORT, 0, "conf_report: memory allocation failure."));
  while ((dnode = dumper) != NULL)
    {
      dumper = dumper->next;
      if (dnode->s)
	free (dnode->s);
      free (dnode);
    }
  return;
}