/* fuse_optdoc.h - preprocessor utilities to help enforce consistency
 *                 with opt parsing within libfuse
 *
 * Copyright (C) 2024  Luke T. Shumaker <lukeshu@lukeshu.com>
 *
 * This program can be distributed under the terms of the GNU LGPLv2.
 * See the file COPYING.LIB.
*/

/* primitive utilities */

#define _EMPTY()
#define _LPAREN() (
#define _RPAREN() )

#define _FIRST(a, ...) a
#define _SECOND(a, b, ...) b
#define _REST(a, ...) __VA_ARGS__

#define _CAT(a, b) a ## b
#define _EAT(...)
#define _EXPAND(...) __VA_ARGS__

/* conditionals */

#define _T xxTxx
#define _F xxFxx

#define _SENTINEL() bogus, _T /* a magic sentinel value */
#define _IS_SENTINEL(...) _SECOND(__VA_ARGS__, _F)

#define _IS_TUPLE(x) _IS_SENTINEL(_IS_TUPLE__ x)
#define _IS_TUPLE__(...) _SENTINEL()

#define _IF(cond) _CAT(_IF__, cond) /* _IF(cond)(then)(else) */
#define _IF__xxTxx(...) __VA_ARGS__ _EAT
#define _IF__xxFxx(...) _EXPAND

/* 'optional' container */

#define _VAL(x) (x)
#define _NIL()

#define _IS_VAL(x) _IS_TUPLE(x)

#define _UNWRAP_VAL(x) _EXPAND x

/* recursion utilities */

#define _EVAL(...) _EVAL__1024(__VA_ARGS__) /* 1024 iterations aught to be enough for anybody */
#define _EVAL__1024(...) _EVAL__512(_EVAL__512(__VA_ARGS__))
#define _EVAL__512(...) _EVAL__256(_EVAL__256(__VA_ARGS__))
#define _EVAL__256(...) _EVAL__128(_EVAL__128(__VA_ARGS__))
#define _EVAL__128(...) _EVAL__64(_EVAL__64(__VA_ARGS__))
#define _EVAL__64(...) _EVAL__32(_EVAL__32(__VA_ARGS__))
#define _EVAL__32(...) _EVAL__16(_EVAL__16(__VA_ARGS__))
#define _EVAL__16(...) _EVAL__8(_EVAL__8(__VA_ARGS__))
#define _EVAL__8(...) _EVAL__4(_EVAL__4(__VA_ARGS__))
#define _EVAL__4(...) _EVAL__2(_EVAL__2(__VA_ARGS__))
#define _EVAL__2(...) _EVAL__1(_EVAL__1(__VA_ARGS__))
#define _EVAL__1(...) __VA_ARGS__

#define _DEFER2(macro) macro _EMPTY _EMPTY()()

#define _CAT1(a, b) a ## b
#define _CAT2(a, b) _CAT1(a, b)

/* the actual optdoc interface ************************************************/

/* Terminology:
 * - "optdoc" - this header library
 * - "gopt" - a "generalized option"; one of: (1) "--flag" (2) "-o option" (3) "positional-arg"
 * - "param" - the RHS in "-o option=param"
 */

/**
 * OPTDOC_GOPTS(groupname, data_type, specs...) defines 3 symbols:
 *
 *  - GROUPNAME_opt_spec (array of 'struct fuse_opt')
 *  - GROUPNAME_opt_proc (fuse_opt_proc_t)
 *  - GROUPNAME_opt_help (void function)
 *
 * There are several limitations of optdoc compared to using
 * fuse_opt.h directly:
 * - "--flags" may not take a parameter, only "-o options" can
 * - it is not possible for a gopt to have a synonym, except for the
 *   special-cased "-h"/"--help", "-V"/"--version", and "-d"/"-o
 *   debug" cases
 *
 * However, optdoc provides several things that fuse_opt.h does not:
 * - the doc-string is in-line with the `struct fuse_opt` definition,
 *   for easy verification that gopts are documented
 * - the proc-code is in-line with the `struct fuse_opt` definition,
 *   for increased readability
 * - each gopt is clearly marked as DISCARD or KEEP, for increased
 *   readability
 * - built-in handling of "[no]option" booleans
 * - options with params may have both a 'member' and a proc without
 *   having to specify multiple `struct fuse_opt`s
 *
 * @param groupname is the base name to use in the defined symbols.
 *
 * @param data_t is something like 'struct mystruct'.
 *
 * @param specs is a series of OPTDOC_GOPT_() expressions that specify
 *        each generalize-option.
 */
#define OPTDOC_GOPTS(groupname, data_t, specs...) _EVAL(_OPTDOC_GOPTS(groupname, data_t, specs))

/**
 * The `OPTDOC_GOPT_*` macros construct arguments to OPTDOC_GOPTS().
 *
 * Special-purpose:
 * - OPTDOC_GOPT_HELP() : "-h" and "--help"
 * - OPTDOC_GOPT_VERSION() : "-V" and "--version"
 * - OPTDOC_GOPT_DEBUG() : "-d" and "-o debug"
 *
 * General-purpose:
 * - OPTDOC_GOPT_FLAG(dashname, ...) : "-n" or "--name"
 * - OPTDOC_GOPT_OPT(name, ...) : "-o name"
 * - OPTDOC_GOPT_OPTBOOL(no, name, ...) : "-o [no]name"
 * - OPTDOC_GOPT_OPTPARAM(name, conv, metavar, ...) : "-o name=(conv|metavar)"
 * - OPTDOC_GOPT_POSITIONAL(...) : "positional-arg", may only be specified once
 *
 * Common params:
 *
 * @param help is either 'HELP("description of gopt")' or 'NOHELP'.
 *
 * @param preserve is either 'KEEP' or 'DISCARD'.
 *
 * @param member (only for OPTBOOL and OPTPARAM) is a member of data_t
 *        that is assigned to the value, or the special value
 *        'NOMEMBER' if there is no member to assign to.  Must be a
 *        'bool' for OPTBOOL.  Must match the scanf-type of 'conv' for
 *        OPTPARAM.
 *
 * @param action is a snippet of C code that runs when this gopt is
 *        encountered on the CLI.  The following variables are in-scope:
 *          - `data_t *data`: data to be manipulated; if there is a
 *            'member' then `data->member` has already been set
 *          - `const char *arg`: the gopt from the command-line as a
 *            string, including any "=param"
 *          - `struct fuse_args *outargs`: args to manipulate
 *        May `return -1` early for errors, but otherwise should not
 *        have a `return` statement.
 */
#define OPTDOC_GOPT_HELP(help, action)                                            (_GOPT_HELP, __COUNTER__, help, action)
#define OPTDOC_GOPT_VERSION(help, action)                                         (_GOPT_VERSION, __COUNTER__, help, action)
#define OPTDOC_GOPT_DEBUG(help, action)                                           (_GOPT_DEBUG, __COUNTER__, help, action)
#define OPTDOC_GOPT_FLAG(dashname, help, preserve, action)                        (_GOPT_FLAG, __COUNTER__, dashname, help, preserve, action)
#define OPTDOC_GOPT_OPT(name, help, preserve, action)                             (_GOPT_OPT, __COUNTER__, name, help, preserve, action)
#define OPTDOC_GOPT_OPTBOOL(no, name, help, preserve, member, action)             (_GOPT_OPTBOOL, __COUNTER__, __COUNTER__, no, name, help, preserve, member, action)
#define OPTDOC_GOPT_OPTPARAM(name, conv, metavar, help, preserve, member, action) (_GOPT_OPTPARAM, __COUNTER__, name, conv, metavar, help, preserve, member, action)
#define OPTDOC_GOPT_POSITIONAL(preserve, action)                                  (_GOPT_POSITIONAL, preserve, action)

#define OPTDOC_PRINT1(flag, desc)         fprintf(stderr, "    %-21s  %s\n", flag, desc)
#define OPTDOC_PRINT2(flag1, flag2, desc) fprintf(stderr, "    %-3s  %-12s  %s\n", flag1, flag2, desc)

/* the actual libfuse-specific implementation *********************************/

#define _OPTDOC_FOREACH(data_t, listname, items...)                                  \
	_CAT2(listname, _EXPAND(_FIRST _FIRST(items))) _LPAREN()                     \
	  data_t,                                                                    \
	  _EXPAND(_REST _FIRST(items))                                               \
	_RPAREN()                                                                    \
	_IF(_IS_TUPLE(_SECOND(items,))) (                                            \
		_DEFER2(_OPTDOC_FOREACH__INDIRECT)()(data_t, listname, _REST(items)) \
	)()
#define _OPTDOC_FOREACH__INDIRECT() _OPTDOC_FOREACH

#define _OPTDOC_GOPTS(groupname, data_t, specs...)                   \
	static const struct fuse_opt groupname##_opt_spec[] = {      \
		_OPTDOC_FOREACH(data_t, _OPTDOC_SPEC, specs)         \
		FUSE_OPT_END                                         \
	};                                                           \
	                                                             \
	static void groupname##_opt_help(void)                       \
	{                                                            \
		_OPTDOC_FOREACH(data_t, _OPTDOC_HELP, specs)         \
	}                                                            \
	                                                             \
	static int groupname##_opt_proc(void *_data,                 \
	                                const char *arg, int _key,   \
	                                struct fuse_args *outargs)   \
	{                                                            \
		data_t *data = _data;                                \
		                                                     \
		/* suppress potential -Wunused-parameter warnings */ \
		(void)arg;                                           \
		(void)outargs;                                       \
		                                                     \
		switch (_key) {                                      \
		_OPTDOC_FOREACH(data_t, _OPTDOC_PROC, specs)         \
		default:                                             \
			return _OPTDOC_SPECIAL_KEEP;                 \
		}                                                    \
	}

/* the special 'DISCARD', 'KEEP', 'HELP()', 'NOHELP', and 'NOMEMBER values */

#define _OPTDOC_SPECIAL_DISCARD 0
#define _OPTDOC_SPECIAL_KEEP    1

#define _OPTDOC_SPECIAL_HELP(str) _VAL(str)
#define _OPTDOC_SPECIAL_NOHELP    _NIL()

#define _OPTDOC_IS_NOMEMBER(x) _IS_SENTINEL(_OPTDOC_IS_NOMEMBER__##x)
#define _OPTDOC_IS_NOMEMBER__NOMEMBER _SENTINEL()

/* what to do for each gopt type */

#define _OPTDOC_HELP_GOPT_HELP(data_t, key, help, action)                                            _IF(_IS_VAL(help))(OPTDOC_PRINT2("-h", "--help"         , _UNWRAP_VAL(_OPTDOC_SPECIAL_##help));)()
#define _OPTDOC_HELP_GOPT_VERSION(data_t, key, help, action)                                         _IF(_IS_VAL(help))(OPTDOC_PRINT2("-V", "--version"      , _UNWRAP_VAL(_OPTDOC_SPECIAL_##help));)()
#define _OPTDOC_HELP_GOPT_DEBUG(data_t, key, help, action)                                           _IF(_IS_VAL(help))(OPTDOC_PRINT2("-d", "-o debug"       , _UNWRAP_VAL(_OPTDOC_SPECIAL_##help));)()
#define _OPTDOC_HELP_GOPT_FLAG(data_t, key, dashname, help, preserve, action)                        _IF(_IS_VAL(help))(OPTDOC_PRINT1(dashname               , _UNWRAP_VAL(_OPTDOC_SPECIAL_##help));)()
#define _OPTDOC_HELP_GOPT_OPT(data_t, key, name, help, preserve, action)                             _IF(_IS_VAL(help))(OPTDOC_PRINT1("-o " name             , _UNWRAP_VAL(_OPTDOC_SPECIAL_##help));)()
#define _OPTDOC_HELP_GOPT_OPTBOOL(data_t, key_y, key_n, no, name, help, preserve, member, action)    _IF(_IS_VAL(help))(OPTDOC_PRINT1("-o [" no "]" name     , _UNWRAP_VAL(_OPTDOC_SPECIAL_##help));)()
#define _OPTDOC_HELP_GOPT_OPTPARAM(data_t, key, name, conv, metavar, help, preserve, member, action) _IF(_IS_VAL(help))(OPTDOC_PRINT1("-o " name "=" metavar , _UNWRAP_VAL(_OPTDOC_SPECIAL_##help));)()
#define _OPTDOC_HELP_GOPT_POSITIONAL(data_t, preserve, action)

#define _OPTDOC_SPEC_GOPT_HELP(data_t, key, help, action)                                            FUSE_OPT_KEY("-h", key), FUSE_OPT_KEY("--help", key),
#define _OPTDOC_SPEC_GOPT_VERSION(data_t, key, help, action)                                         FUSE_OPT_KEY("-V", key), FUSE_OPT_KEY("--version", key),
#define _OPTDOC_SPEC_GOPT_DEBUG(data_t, key, help, action)                                           FUSE_OPT_KEY("-d", key), FUSE_OPT_KEY("debug", key),
#define _OPTDOC_SPEC_GOPT_FLAG(data_t, key, dashname, help, preserve, action)                        FUSE_OPT_KEY(dashname, key),
#define _OPTDOC_SPEC_GOPT_OPT(data_t, key, name, help, preserve, action)                             FUSE_OPT_KEY(name, key),
#define _OPTDOC_SPEC_GOPT_OPTBOOL(data_t, key_y, key_n, no, name, help, preserve, member, action)    FUSE_OPT_KEY(name, key_y), FUSE_OPT_KEY(no name, key_n),
#define _OPTDOC_SPEC_GOPT_OPTPARAM(data_t, key, name, conv, metavar, help, preserve, member, action) _IF(_OPTDOC_IS_NOMEMBER(member))()({name "=" conv, offsetof(data_t, member), 0},) FUSE_OPT_KEY(name "=", key),
#define _OPTDOC_SPEC_GOPT_POSITIONAL(data_t, preserve, action)

#define _OPTDOC_PROC_GOPT_HELP(data_t, key, help, action)                                            case key: action return _OPTDOC_SPECIAL_KEEP;
#define _OPTDOC_PROC_GOPT_VERSION(data_t, key, help, action)                                         case key: action return _OPTDOC_SPECIAL_KEEP;
#define _OPTDOC_PROC_GOPT_DEBUG(data_t, key, help, action)                                           case key: action return _OPTDOC_SPECIAL_KEEP;
#define _OPTDOC_PROC_GOPT_FLAG(data_t, key, dashname, help, preserve, action)                        case key: action return _OPTDOC_SPECIAL_##preserve;
#define _OPTDOC_PROC_GOPT_OPT(data_t, key, name, help, preserve, action)                             case key: action return _OPTDOC_SPECIAL_##preserve;
#define _OPTDOC_PROC_GOPT_OPTBOOL(data_t, key_y, key_n, no, name, help, preserve, member, action)    case key_y: _IF(_OPTDOC_IS_NOMEMBER(member))()(data->member = true; ) action return _OPTDOC_SPECIAL_##preserve; \
                                                                                                     case key_n: _IF(_OPTDOC_IS_NOMEMBER(member))()(data->member = false;) action return _OPTDOC_SPECIAL_##preserve;
#define _OPTDOC_PROC_GOPT_OPTPARAM(data_t, key, name, conv, metavar, help, preserve, member, action) case key: action return _OPTDOC_SPECIAL_##preserve;
#define _OPTDOC_PROC_GOPT_POSITIONAL(data_t, preserve, action)                                       case FUSE_OPT_KEY_NONOPT: action return _OPTDOC_SPECIAL_##preserve;
