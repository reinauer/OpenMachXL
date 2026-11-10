#include "optimizer_bridge.h"
#include "espresso.h"
#include <string.h>

void espresso_reset_reduce_order(void);

static pcover decode_cover_rows(const char *rows, size_t count, size_t nvars)
{
    pcover cover = new_cover((int)count);
    pcube row    = new_cube();
    size_t i, v;
    for (i = 0; i < count; ++i) {
        set_clear(row, cube.size);
        for (v = 0; v < nvars; ++v) {
            char c = rows[i * nvars + v];
            if (c != '1')
                set_insert(row, 2 * v);
            if (c != '0')
                set_insert(row, 2 * v + 1);
        }
        set_insert(row, 2 * nvars);
        cover = sf_addset(cover, row);
    }
    free_cube(row);
    return cover;
}

int machxl_espresso_cover_operation(const char *left, size_t left_count, const char *right, size_t right_count,
                                    size_t nvars, int operation, char **result, size_t *result_count)
{
    pcover a, b = NULL, out;
    pcube p, last;
    size_t index          = 0, v;
    *result               = NULL;
    *result_count         = 0;
    cube.num_binary_vars  = (int)nvars;
    cube.num_vars         = (int)nvars + 1;
    cube.part_size        = ALLOC(int, cube.num_vars);
    cube.part_size[nvars] = 1;
    cube_setup();
    a = decode_cover_rows(left, left_count, nvars);
    if (operation == 1) {
        b   = decode_cover_rows(right, right_count, nvars);
        out = cv_intersect(a, b);
    } else
        out = complement(cube1list(a));
    *result_count = (size_t)out->count;
    *result       = malloc((*result_count * nvars) + 1);
    if (*result)
        foreach_set(out, last, p)
        {
            for (v = 0; v < nvars; ++v) {
                int zero           = is_in_set(p, 2 * v) != 0;
                int one            = is_in_set(p, 2 * v + 1) != 0;
                (*result)[index++] = zero ? (one ? '-' : '0') : '1';
            }
        }
    free_cover(out);
    free_cover(a);
    if (b)
        free_cover(b);
    setdown_cube();
    FREE(cube.part_size);
    sf_cleanup();
    sm_cleanup();
    return !*result;
}

int machxl_espresso_minimize(const char *on, size_t on_count, const char *dc, size_t dc_count, size_t nvars,
                             struct machxl_espresso_options options, char **result, size_t *result_count)
{
    int inverted;
    return machxl_espresso_minimize_phase(on, on_count, dc, dc_count, NULL, 0, nvars, 0, options, result, result_count,
                                          &inverted);
}

int machxl_espresso_minimize_phase(const char *on, size_t on_count, const char *dc, size_t dc_count, const char *off,
                                   size_t off_count, size_t nvars, int both, struct machxl_espresso_options options,
                                   char **result, size_t *result_count, int *inverted)
{
    pcover f, d, r, saved, saved_r;
    pcube p, last;
    size_t index = 0, v;
    int failed;
    *result       = NULL;
    *result_count = 0;
    *inverted     = 0;
    debug = verbose_debug = trace = summary = use_random_order = 0;
    force_irredundant = recompute_onset = use_super_gasp = 0;
    /* MACHMIN leaves these Berkeley globals at zero (no main.c defaults). */
    remove_essential = unwrap_onset = 0;
    single_expand                   = options.fast;
    skip_make_sparse                = options.skip_sparse;
    if (options.reset_reduce_order)
        espresso_reset_reduce_order();
    memset(total_time, 0, sizeof(total_time));
    memset(total_calls, 0, sizeof(total_calls));
    cube.num_binary_vars  = (int)nvars;
    cube.num_vars         = (int)nvars + 1;
    cube.part_size        = ALLOC(int, cube.num_vars);
    cube.part_size[nvars] = 1;
    cube_setup();
    f = decode_cover_rows(on, on_count, nvars);
    d = decode_cover_rows(dc, dc_count, nvars);
    r = off ? decode_cover_rows(off, off_count, nvars) : complement(cube2list(f, d));
    /* Avoid expensive negative-phase candidates using the compatibility size gate. */
    both = both && f->count >= 2 && (f->count < 11 ? r->count < 8 * f->count : r->count <= 4 * f->count + f->count / 2);
    saved   = sf_save(f);
    saved_r = both ? sf_save(r) : NULL;
    if (both)
        skip_make_sparse = 1;
    f      = options.exact ? minimize_exact(f, d, r, 1) : espresso(f, d, r);
    failed = verify(f, saved, d);
    if (both && !failed) {
        r      = options.exact ? minimize_exact(r, d, f, 1) : espresso(r, d, f);
        failed = verify(r, saved_r, d);
        /* An empty negative cover cannot win phase selection. */
        if (!failed && r->count > 0 && r->count < f->count) {
            pcover swap = f;
            f           = r;
            r           = swap;
            *inverted   = 1;
        }
    }
    if (!failed) {
        *result_count = (size_t)f->count;
        *result       = malloc((*result_count * nvars) + 1);
        if (!*result)
            failed = 1;
        else
            foreach_set(f, last, p)
            {
                for (v = 0; v < nvars; ++v) {
                    int zero           = is_in_set(p, 2 * v) != 0;
                    int one            = is_in_set(p, 2 * v + 1) != 0;
                    (*result)[index++] = zero ? (one ? '-' : '0') : '1';
                }
            }
    }
    free_cover(f);
    free_cover(d);
    free_cover(r);
    free_cover(saved);
    if (saved_r)
        free_cover(saved_r);
    setdown_cube();
    FREE(cube.part_size);
    sf_cleanup();
    sm_cleanup();
    return failed;
}
