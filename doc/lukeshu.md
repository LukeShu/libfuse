# API structure

There are several chunks to the libfuse API, which actually supports
both FUSE (Filesystem in Userspace) and CUSE (Character device in
Userspace):

| Header            | Description                                                                |
|-------------------|----------------------------------------------------------------------------|
| `fuse.h`          | FUSE high-level API                                                        |
| `fuse_lowlevel.h` | FUSE low-level API                                                         |
|-------------------|----------------------------------------------------------------------------|
| `cuse_lowlevel.h` | CUSE low-level API (there is no high-level CUSE API)                       |
|-------------------|----------------------------------------------------------------------------|
| `fuse_opt.h`      | Option-parsing that is consistent with that done by the FUSE and CUSE APIs |

Internally, there is also `fuse_common.h` that is shared by `fuse.h`
and `fuse_lowlevel.h`.

# FUSE program flow

## main

At its most basic, a FUSE program can look something like this:

```c
int main(int argc, char *argv[]) {
    struct fuse_operations op = ...;
    user_data = ...;
    return fuse_main(arc, argv, &op, &user_data);
}
```

Now, there are a few reasons why you might not want to use the omnibus
`fuse_main()` and do some things yourself.  `fuse_main()` is pretty
simple, and consists of 3 parts, each of which have their own
functions:

 1. Before the event loop: `fuse_setup()`
 2. The event loop: `fuse_loop()` or `fuse_loop_mt()`
 3. After the event loop: `fuse_teardown()`.

```c
int fuse_main(int argc, char *argv[],
              const struct fuse_operations *op,
              void *user_data)
{
    /* Before the event loop.  */
    struct fuse *fuse;
    char *mountpoint;
    int multithreaded;
    fuse = fuse_setup(argc, argv,
                      op, sizeof *op,
                      &mountpoint,
                      &multithreaded,
                      user_data);
    if (fuse == NULL)
        return 1;

    /* The event loop.  */
    int res;
    if (multithreaded)
        res = fuse_loop_mt(fuse);
    else
        res = fuse_loop(fuse);

    /* After the event loop.  */
    fuse_teardown(fuse, mountpoint);
    if (res == -1)
        return 1;

    return 0;
}
```

## before the event loop

As we saw, `fuse_setup()` is the omnibus function for what needs to
happen before the event loop.  What all does it do?

```c
args = FUSE_ARGS_INIT(argc, argv);                                          // fuse_opt.h
       fuse_parse_cmdline(&args, &mountpoint, &multithreaded, &foreground); // fuse_common.h
chan = fuse_mount(mountpoint, &args);                                       // fuse_common.h
fuse = fuse_new(chan, &args, op, sizeof *op, user_data);                    // fuse.h
       fuse_daemonize(foreground);                                          // fuse_common.h
sess = fuse_get_session(fuse);                                              // fuse.h
       fuse_set_signal_handlers(sess);                                      // use_common.h
return fuse;
```

Despite the name, `fuse_parse_cmdline()` just does *part* of the
cmdline parsing; more is done by `fuse_mount()` and `fuse_new()`.
With this we see part of how the `fuse_opt.h` API is designed:

> call `fuse_opt_parse(args, &opts, opt_spec, opt_proc)` to consume
> some opts from `args` and populate `opts`.  Each option is either
> kept for future rounds, or discarded:
> 
> | `opt_spec.value`       | `opt_proc()` return |         |
> |------------------------|---------------------|---------|
> | `FUSE_OPT_KEY_DISCARD` | -                   | discard |
> | `FUSE_OPT_KEY_KEEP`    | -                   | keep    |
> | other                  | 0                   | discard |
> | other                  | 1                   | keep    |
>
> The `opt_proc` function can also insert new args for future rounds:
>
> - `fuse_opt_add_opt{,_escaped}()`
> - `fuse_opt_{add,insert}_arg()`

## the event loop

TODO

## after the event loop

TODO
