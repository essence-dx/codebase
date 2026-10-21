/*
 * printf_attr.h - the printf format attribute, described correctly.
 *
 * GCC's `format(printf, ...)` means "the TARGET's printf". On MinGW that is
 * the Microsoft one, whose format dialect has no `%z` length modifier, so a
 * call to a helper whose implementation is the C99 vsnprintf is rejected at
 * compile time even though the implementation handles `%zu` correctly.
 * `gnu_printf` always means the C99-conformant format, which is what these
 * implementations actually are.
 *
 * The attribute is a compile-time check on the FORMAT STRING, not on the
 * runtime library, so describing it as C99 is the accurate thing to do - and
 * it is the description that does not reject valid C99 code.
 *
 * The three helpers this covers (cbm_arena_sprintf, http_appendf,
 * cbm_http_replyf) all format through the C99 vsnprintf family.
 */
#ifndef CBM_PRINTF_ATTR_H
#define CBM_PRINTF_ATTR_H

#if defined(__GNUC__) || defined(__clang__)
#define CBM_PRINTF_FMT(fmt_index, first_arg) \
    __attribute__((format(gnu_printf, fmt_index, first_arg)))
#else
/* No attribute to apply: the format string is simply not checked. */
#define CBM_PRINTF_FMT(fmt_index, first_arg)
#endif

#endif /* CBM_PRINTF_ATTR_H */
