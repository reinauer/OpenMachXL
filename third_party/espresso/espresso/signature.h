#if BPI == 16
#define ODD_MASK  0xaaaa
#define EVEN_MASK 0x5555
#else
#define ODD_MASK  0xaaaaaaaa
#define EVEN_MASK 0x55555555
#endif

#define POSITIVE 1
#define NEGATIVE 0

#define PRESENT 1
#define ABSENT  0

#define RAISED 2

/* black_white.c */ void setup_bw(pset_family R, pset c);
/* black_white.c */ void free_bw(void);
/* black_white.c */ bool black_white(void);
/* black_white.c */ void split_list(pset_family R, int v);
/* black_white.c */ void merge_list(void);
/* black_white.c */ void print_bw(int size);
/* black_white.c */ void variable_list_alloc(int size);
/* black_white.c */ void variable_list_init(int reduced_c_free_count, int * reduced_c_free_list);
/* black_white.c */ void variable_list_delete(int element);
/* black_white.c */ void variable_list_insert(int element);
/* black_white.c */ bool variable_list_empty(void);
/* black_white.c */ void get_next_variable(int * pv, int * pphase, pset_family R);
/* black_white.c */ void print_variable_list(void);
/* black_white.c */ extern void reset_black_list(void);
/* black_white.c */ extern void push_black_list(void);
/* black_white.c */ extern void pop_black_list(void);
/* canonical.c */ pcover find_canonical_cover(pset_family F1, pset_family D, pset_family R);
/* essentiality.c */ pcover etr_order(pset_family F, pset_family E, pset_family R, pset c, pset d);
/* essentiality.c */ void aux_etr_order(pset_family F, pset_family E, pset_family R, pset c, pset d);
/* essentiality.c */ pcover get_mins(pset c);
/* essentiality.c */ int ascending(const void * left, const void * right);
/* util_signature.c */ void set_time_limit(int seconds);
/* util_signature.c */ void print_cover(pset_family F, char * name);
/* util_signature.c */ int sf_equal(pset_family F1, pset_family F2);
/* util_signature.c */ int time_usage(char * name);
/* util_signature.c */ void s_totals(long time, int i);
/* util_signature.c */ void s_runtime(long total);
/* sigma.c */ pcube get_sigma(pset_family R, pset c);
/* sigma.c */ void set_not(pset c);
/* signature.c */ void cleanup(int signum);
/* signature.c */ pcover signature(pset_family F1, pset_family D1, pset_family R1);
/* signature.c */ pcover generate_primes(pset_family F, pset_family R);
/* signature_exact.c */ pcover signature_minimize_exact(pset_family ESCubes, pset_family ESSet);
/* signature_exact.c */ sm_matrix *signature_form_table(pset_family ESCubes, pset_family ESSet);
