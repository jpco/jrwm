# On contributions

Please note that JrWM falls within the part of the River ecosystem where
LLM-generated contributions are not accepted, and I will uphold that
restriction.


## Welcome contributions

This is a (non-exhaustive!) list of additions or changes to this project which
would be gratefully reviewed.  For those who just want to suggest particular
changes, rather than produce code, issues are also welcome.

-   Tweaks to existing behavior to more closely match other WMs
-   Alternate layouts
-   Alternate space-management schemes (i.e., dynamic spaces)
-   Correctness improvements and/or better protocol integration
-   (Optional) dynamic configuration via IPC or file


## Code guidelines

The following are some code guidelines.  PRs or issues to fix code which
contradicts these guidelines are welcome.

-   Functions should either be "real", pure functions, or should be
    state-modifying procedures; don't return a value from a function that
    modifies state or makes calls to River/Wayland.

-   Predicate functions should be named `condition_object`, like `idle_space`,
    rather than something like `is_space_idle` or `space_idle_p`.

-   Where they are in tension, prefer simplicity and legibility over concision.

-   Loops and conditionals should always have braces, unless they contain only
    other conditionals without braces or a single statement with no comment (and
    even then, braces are okay).  An if statement should have braces on all its
    "then" statements or on none of them.

-   Files should be ordered: types, variables, static functions, and finally
    extern functions (or main).  If this order becomes too obnoxious, that's
    evidence the file should be split up.
