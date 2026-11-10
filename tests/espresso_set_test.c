#include "espresso.h"
#include <assert.h>

/* This multiple-output cover needs a second sparse-cleanup pass to remove
 * another product and five literals. It catches stale cost comparisons and
 * loss of the force_irredundant policy independently of Espresso's driver. */
static void test_sparse_iteration(void)
{
    static const char *terms[] = {
        "-001100", "-1-0100", "0--1001", "-0-0001", "111-110",
        "1000011", "-011011", "0-11011", "-100101", "00-0101",
        "0001111"
    };
    pcover f, d, r, once, iterated;
    pset row;
    cost_t one_cost, iterated_cost;
    size_t i;
    int var;
    cube.num_binary_vars = 4;
    cube.num_vars = 5;
    cube.part_size = ALLOC(int, cube.num_vars);
    cube.part_size[4] = 3;
    cube_setup();
    assert(cube.num_mv_vars == 1 && cube.output == 4);
    f = new_cover(11);
    d = new_cover(0);
    row = new_cube();
    for (i = 0; i < sizeof terms / sizeof terms[0]; ++i) {
        set_clear(row, cube.size);
        for (var = 0; var < 4; ++var) {
            if (terms[i][var] != '1') set_insert(row, 2 * var);
            if (terms[i][var] != '0') set_insert(row, 2 * var + 1);
        }
        for (var = 0; var < 3; ++var)
            if (terms[i][4 + var] == '1') set_insert(row, 8 + var);
        SET(row, PRIME);
        f = sf_addset(f, row);
    }
    r = complement(cube2list(f, d));
    force_irredundant = 0;
    once = make_sparse(sf_save(f), d, r);
    force_irredundant = 1;
    iterated = make_sparse(sf_save(f), d, r);
    force_irredundant = 0;
    cover_cost(once, &one_cost);
    cover_cost(iterated, &iterated_cost);
    assert(one_cost.cubes < 11 ||
           (one_cost.cubes == 11 && one_cost.total <= 41));
    assert(iterated_cost.cubes < 10 ||
           (iterated_cost.cubes == 10 && iterated_cost.total <= 36));
    assert(verify(once, f, d) == 0);
    assert(verify(iterated, f, d) == 0);
    set_free(row);
    free_cover(f);
    free_cover(d);
    free_cover(r);
    free_cover(once);
    free_cover(iterated);
    setdown_cube();
    FREE(cube.part_size);

    /* The full library also supports covers without an output variable. */
    cube.num_binary_vars = cube.num_vars = 2;
    cube.part_size = ALLOC(int, cube.num_vars);
    cube_setup();
    assert(cube.num_mv_vars == 0 && cube.output == -1);
    setdown_cube();
    FREE(cube.part_size);
    sm_cleanup();
}

int main(void)
{
    const int sizes[] = {1, 15, 16, 17, 31, 32, 33, 64, 65};
    size_t i;
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        int size = sizes[i];
        pset row = set_full(size);
        int bit;
        assert(setp_full(row, size));
        assert(set_ord(row) == size);
        for (bit = 0; bit < size; ++bit) {
            set_remove(row, bit);
            assert(!setp_full(row, size));
            set_insert(row, bit);
            assert(setp_full(row, size));
        }
        set_free(row);
    }
    {
        FILE *file = tmpfile();
        pset_family family = sf_new(2, 33);
        pset_family copy;
        pset row = set_full(33);
        assert(file != NULL);
        family = sf_addset(family, row);
        set_clear(row, 33);
        set_insert(row, 0);
        set_insert(row, 16);
        set_insert(row, 32);
        family = sf_addset(family, row);
        sf_write(file, family);
        rewind(file);
        copy = sf_read(file);
        assert(copy->count == 2 && copy->sf_size == 33);
        assert(setp_equal(GETSET(copy, 0), GETSET(family, 0)));
        assert(setp_equal(GETSET(copy, 1), GETSET(family, 1)));
        assert(setp_full(GETSET(copy, 0), 33));
        set_free(row);
        sf_free(family);
        sf_free(copy);
        fclose(file);
    }
    test_sparse_iteration();
    sf_cleanup();
    return 0;
}
