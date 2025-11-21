/*
 * Phoenix-RTOS
 *
 * Generic useful macros
 *
 * Copyright 2024 Phoenix Systems
 * Author: Daniel Sawka
 *
 * %LICENSE%
 */

#ifndef _PH_LIB_HELPERS_H_
#define _PH_LIB_HELPERS_H_

/*
These macros should only be used in `#if` and `#elif` directives, because undefined identifiers expand to 0 there.
Otherwise there will be "use of undefined identifier" errors (an exception: identifier is first checked for
existence with e.g. `#ifdef`).

Anything that expands to a numerical expression (or to an empty value) is fine. String literals don't work.
*/

/*
These macros produce a logically correct result only if X is defined. You may use `#ifdef` or `defined()`
for this purpose. Unfortunately, `defined()` cannot be conveniently put inside a macro as this is undefined
behavior (see -Wexpansion-to-defined for details), so you have to use it directly on the spot, for example:
`#if defined(PERIPH1) && !ISEMPTY(PERIPH1)`
*/

/* True if X is empty (has no value). The result in #if is valid only if defined(X) is true */
/* parasoft-begin-suppress MISRAC2012-RULE_20_7 "This macro relies on a clever trick to determine if
 * the token passed as X is defined and so parentheses cannot be used around the X parameter" */
#define ISEMPTY(X) ((0 - X - 1) == 1 && (X + 0) != -2)
/* parasoft-end-suppress MISRAC2012-RULE_20_7 */

#endif
