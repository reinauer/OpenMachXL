#ifndef MACHXL_OPTIMIZER_BRIDGE_H
#define MACHXL_OPTIMIZER_BRIDGE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
struct machxl_espresso_options
{
    int fast;
    int exact;
    int skip_sparse;
    int reset_reduce_order;
};
/* Caller serializes the Berkeley library's global workspace. Rows contain
 * nvars characters, each 0/1/-. Result is malloc-owned, with no terminator. */
int machxl_espresso_minimize(const char *on, size_t on_count, const char *dc, size_t dc_count, size_t nvars,
                             struct machxl_espresso_options options, char **result, size_t *result_count);
/* If off is NULL, construct it as complement(on + dc). With both set,
 * apply MACHMIN's pre-minimization size gate, then minimize F followed
 * by R using the minimized F. inverted reports selection of R. */
int machxl_espresso_minimize_phase(const char *on, size_t on_count, const char *dc, size_t dc_count, const char *off,
                                   size_t off_count, size_t nvars, int both, struct machxl_espresso_options options,
                                   char **result, size_t *result_count, int *inverted);
/* Original expression lowering uses Espresso complement (operation 0)
 * and cover intersection (operation 1), then reverses the returned list. */
int machxl_espresso_cover_operation(const char *left, size_t left_count, const char *right, size_t right_count,
                                    size_t nvars, int operation, char **result, size_t *result_count);
#ifdef __cplusplus
}
#endif
#endif
