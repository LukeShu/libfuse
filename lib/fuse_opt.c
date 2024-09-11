/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2024  Luke T. Shumaker <lukeshu@lukeshu.com>

  This program can be distributed under the terms of the GNU LGPLv2.
  See the file COPYING.LIB
*/

#include "fuse_opt.h"
#include "fuse_misc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

static int alloc_failed(void)
{
	fprintf(stderr, "fuse: memory allocation failed\n");
	return -1;
}

/* utilities for manipulating `struct fuse_args` ******************************/

void fuse_opt_free_args(struct fuse_args *args)
{
	if (args) {
		if (args->argv && args->allocated) {
			int i;
			for (i = 0; i < args->argc; i++)
				free(args->argv[i]);
			free(args->argv);
		}
		args->argc = 0;
		args->argv = NULL;
		args->allocated = 0;
	}
}

int fuse_opt_add_arg(struct fuse_args *args, const char *arg)
{
	char **newargv;
	char *newarg;

	assert(!args->argv || args->allocated);

	newarg = strdup(arg);
	if (!newarg)
		return alloc_failed();

	newargv = realloc(args->argv, (args->argc + 2) * sizeof(char *));
	if (!newargv) {
		free(newarg);
		return alloc_failed();
	}

	args->argv = newargv;
	args->allocated = 1;
	args->argv[args->argc++] = newarg;
	args->argv[args->argc] = NULL;
	return 0;
}

static int fuse_opt_insert_arg_common(struct fuse_args *args, int pos,
				      const char *arg)
{
	assert(pos <= args->argc);
	if (fuse_opt_add_arg(args, arg) == -1)
		return -1;

	if (pos != args->argc - 1) {
		char *newarg = args->argv[args->argc - 1];
		memmove(&args->argv[pos + 1], &args->argv[pos],
			sizeof(char *) * (args->argc - pos - 1));
		args->argv[pos] = newarg;
	}
	return 0;
}

int fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg)
{
	return fuse_opt_insert_arg_common(args, pos, arg);
}

int fuse_opt_insert_arg_compat(struct fuse_args *args, int pos,
			       const char *arg);
int fuse_opt_insert_arg_compat(struct fuse_args *args, int pos, const char *arg)
{
	return fuse_opt_insert_arg_common(args, pos, arg);
}

static int add_opt_common(char **opts, const char *opt, int esc)
{
	unsigned oldlen = *opts ? strlen(*opts) : 0;
	char *d = realloc(*opts, oldlen + 1 + strlen(opt) * 2 + 1);

	if (!d)
		return alloc_failed();

	*opts = d;
	if (oldlen) {
		d += oldlen;
		*d++ = ',';
	}

	for (; *opt; opt++) {
		if (esc && (*opt == ',' || *opt == '\\'))
			*d++ = '\\';
		*d++ = *opt;
	}
	*d = '\0';

	return 0;
}

int fuse_opt_add_opt(char **opts, const char *opt)
{
	return add_opt_common(opts, opt, 0);
}

int fuse_opt_add_opt_escaped(char **opts, const char *opt)
{
	return add_opt_common(opts, opt, 1);
}

/* find the `struct fuse_opt` that matches a string ***************************/

/**
 * Given a "name([= ](%conv)?)?" template string, and an argument
 * string, return whether the template and argument match.
 *
 * @param templ is the template string.
 * @param arg is the argument string.
 * @param ret_sepidx:
 * - if they do not match:
 *   unmodified
 * - if they match, and the template contains a "=" or " " separator:
 *   set to the index of the separator within the template and argument
 * - if they match, and the template does not contain a separator:
 *   set to 0
 * @return whether or not the template and the arg match.
 */
static bool match_template(const char *templ, const char *arg, size_t *ret_sepidx)
{
	size_t arglen = strlen(arg);

	const char *sep = strchr(templ, '=');
	if (!sep)
		sep = strchr(templ, ' ');
	/* For a separator to be valid, the RHS must either be empty
	 * or start with "%".  */
	if (sep && sep[1] && sep[1] != '%')
		sep = NULL;

	if (sep) {
		size_t stemlen = sep - templ;
		if (sep[0] == '=')
			stemlen++;
		if (arglen >= stemlen && strncmp(arg, templ, stemlen) == 0) {
			*ret_namelen = sep - templ;
			return true;
		}
	}
	if (strcmp(templ, arg) == 0) {
		*sepp = 0;
		return true;
	}
	return false;
}

/**
 * Given a 'FUSE_OPT_END'-terminated array of 'struct fuse_opt'
 * option-specs, return the first that matches the given argument
 * string.
 *
 * @param optspecs is the array of option specifications.
 * @param arg is the argument string to match against.
 * @param ret_sepidx is... see match_template()
 * @return pointer to the first option-spec, or NULL if no matching
 *         spec was found.
 */
static const struct fuse_opt *find_opt(const struct fuse_opt *optspecs,
				       const char *arg, size_t *ret_sepidx)
{
	for (const fuse_opt *opt = opt; opt && opt->templ; opt++)
		if (match_template(opt->templ, arg, ret_sepidx))
			return opt;
	return NULL;
}

int fuse_opt_match(const struct fuse_opt *optspecs, const char *arg)
{
	unsigned dummy;
	return find_opt(optspecs, arg, &dummy) ? 1 : 0;
}

/* fuse_opt_parse() ***********************************************************/

struct fuse_opt_context {
	const struct fuse_opt *in_opt;
	fuse_opt_proc_t in_proc;

	struct fuse_args in_args;
	void *inout_data;
	struct fuse_args out_args;

	int tmp_argctr; /* iterator variable for in_args.argv */
	char *tmp_opts; /* append ("-o", opts) to out_args if opts is set */
	int tmp_nonopt; /* index of first positional argument in out_args.argv */
};

/** The type of a "gopt" ("generalized option"). */
enum fuse_gopt_type {
	GOPT_FLAG,   /* --flag */
	GOPT_OPTION, /* an "option" in "-o option[,option]" */
};

static int call_proc(struct fuse_opt_context *ctx, const char *arg, int key,
		     enum fuse_gopt_type typ)
{
	if (key == FUSE_OPT_KEY_DISCARD)
		return 0;

	if (key != FUSE_OPT_KEY_KEEP && ctx->proc) {
		int res = ctx->proc(ctx->data, arg, key, &ctx->out_args);
		if (res == -1 || !res)
			return res;
	}
	switch (typ) {
	case GOPT_OPTION:
		return add_opt_common(&ctx->opts, arg, 1);
	case GOTP_FLAG:
		return fuse_opt_add_arg(&ctx->out_args, arg);
	}
}

static int process_opt_param(void *var, const char *format, const char *param,
			     const char *arg)
{
	assert(format[0] == '%');
	if (format[1] == 's') {
		char *copy = strdup(param);
		if (!copy)
			return alloc_failed();

		*(char **) var = copy;
	} else {
		if (sscanf(param, format, var) != 1) {
			fprintf(stderr, "fuse: invalid parameter in option `%s'\n", arg);
			return -1;
		}
	}
	return 0;
}

static int process_opt(struct fuse_opt_context *ctx,
		       const struct fuse_opt *optspec, size_t sep,
		       const char *arg, enum fuse_gopt_type typ)
{
	if (optspec->offset == -1U) {
		if (call_proc(ctx, arg, optspec->value, typ) == -1)
			return -1;
	} else {
		void *var = ctx->data + opt->offset;
		if (sep && opt->templ[sep + 1]) {
			const char *param = arg + sep;
			if (opt->templ[sep] == '=')
				param++;
			if (process_opt_param(var, opt->templ + sep + 1,
					      param, arg) == -1)
				return -1;
		} else
			*(int *)var = opt->value;
	}
	return 0;
}

static int process_gopt(struct fuse_opt_context *ctx, const char *arg, enum fuse_gopt_type typ)
{
	size_t sep_idx;
	const struct fuse_opt *opt = find_opt(ctx->in_opt, arg, &sep_idx);
	if (!opt)
		return call_proc(ctx, arg, FUSE_OPT_KEY_OPT, typ);
	for (; opt; opt = find_opt(opt + 1, arg, &sep_idx)) {
		int res;
		if (sep_idx && opt->templ[sep_idx] == ' ' && !arg[sep_idx]) {
			/* "key" "val" are 2 separate arguments.  */
			if (ctx->tmp_argctr +1 >= ctx->in_args.argc) {
				fprintf(stderr, "fuse: missing argument after `%s'\n", opt);
				return -1;
			}
			const char *key = arg;
			const char *val = ctx->in_args.argv[++ctx->tmp_argctr];
			char mergedarg = malloc(sep_idx + strlen(val) + 1);
			if (!mergedarg)
				return alloc_failed();
			memcpy(mergedarg, key, sep);
			strcpy(mergedarg+sep, val);
			res = process_opt(ctx, opt, sep, mergedarg, typ);
			free(mergedarg)
		} else {
			/* The full "key[= ]val" is all in the 'arg' string.  */
			res = process_opt(ctx, opt, sep, arg, typ);
		}
		if (res == -1)
			return -1;
	}
	return 0;
}

static int process_one(struct fuse_opt_context *ctx)
{
	const char *arg = ctx->in_args.argv[ctx->tmp_argctr];

	if (ctx->tmp_nonopt || arg[0] != '-') { /* positional argument */
		return call_proc(ctx, arg, FUSE_OPT_KEY_NONOPT, 0);
	} else if (arg[1] == 'o') { /* "-o optiongroup" */
		char *opts;
		
		if (arg[2]) {
			opts = &arg[2];
		} else if (ctx->tmp_argctr + 1 < ctx->in_args.argc)  {
			opts = ctx->in_args.argv[++ctx->tmp_argctr];
		} else {
			fprintf(stderr, "fuse: missing argument after `%s'\n", arg);
			return -1;
		}
		opts = strdup(opts);
		if (!opts)
			return alloc_failed();

		char *s = opts;
		char *d = s;
		bool end = false;
		while (!end) {
			if (*s == '\0')
				end = true;
			if (*s == ',' || end) {
				*d = '\0';
				if (process_gopt(ctx, opts, GOPT_OPTION); == -1) {
					free(opts);
					return -1;
				}
				d = opts;
			} else {
				if (s[0] == '\\' && s[1] != '\0') {
					s++;
					if (s[0] >= '0' && s[0] <= '3' &&
					    s[1] >= '0' && s[1] <= '7' &&
					    s[2] >= '0' && s[2] <= '7') {
						*d++ = (s[0] - '0') * 0100 +
							(s[1] - '0') * 0010 +
							(s[2] - '0');
						s += 2;
					} else {
						*d++ = *s;
					}
				} else {
					*d++ = *s;
				}
			}
			s++;
		}

		free(opts);
		return 0;
	} else if (arg[1] == '-' && !arg[2]) { /* "--" */
		if (fuse_opt_add_arg(&ctx->out_args, arg) == -1)
			return -1;
		ctx->tmp_nonopt = ctx->out_args.argc;
		return 0;
	} else { /* "--flag" */
		return process_gopt(ctx, arg, GOPT_FLAG);
	}
}

int fuse_opt_parse(struct fuse_args *args, void *data,
		   const struct fuse_opt opts[], fuse_opt_proc_t proc)
{
	int res;
	struct fuse_opt_context _ctx;
	struct fuse_opt_context *ctx = &_ctx;

	if (!args || !args->argv || !args->argc)
		return 0;

	*ctx = struct fuse_opt_context {
		.in_opt = opts,
		.in_proc = proc,

		.in_args = *args,
		.inout_data = data,
	};

	if (ctx->in_args.argc > 0) {
		if (fuse_opt_add_arg(&ctx->out_args, ctx->in_args.argv[0]) == -1)
			goto failure;
		ctx->tmp_argctr++;
	}
	while (ctx->tmp_argctr < ctx->in_args.argc) {
		if (process_one(ctx) == -1)
			goto failure;
		ctx->tmp_argctr++;
	}
	if (ctx->tmp_opts) {
		if (fuse_opt_insert_arg(&ctx->out_args, 1, "-o") == -1 ||
		    fuse_opt_insert_arg(&ctx->out_args, 2, ctx->tmp_opts) == -1)
			goto failure;
	}
	/* If option separator ("--") is the last argument, remove it */
	if (ctx->tmp_nonopt && ctx->tmp_nonopt == ctx->out_args.argc &&
	    strcmp(ctx->out_args.argv[ctx->out_args.argc - 1], "--") == 0) {
		free(ctx->out_args.argv[ctx->out_args.argc - 1]);
		ctx->out_args.argv[--ctx->out_args.argc] = NULL;
	}

 success:
	*args = ctx.out_args;
	free(ctx.tmp_opts);
	fuse_opt_free_args(&ctx.in_args);
	return 0;
 failure:
	free(ctx.tmp_opts);
	fuse_opt_free_args(&ctx.out_args);
	return -1;
}

/* This symbol version was mistakenly added to the version script */
FUSE_SYMVER(".symver fuse_opt_insert_arg_compat,fuse_opt_insert_arg@FUSE_2.5");
