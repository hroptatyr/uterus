#if !defined INCLUDED_sumux_h_
#define INCLUDED_sumux_h_

typedef struct mux_ctx_s *mux_ctx_t;
typedef struct sumux_opt_s *sumux_opt_t;

struct mux_ctx_s {
	int badfd;
	/* input file index */
	int infd;

	/* can be used by the muxer */
	void *rdr;
	/* will be used by ute-mux */
	void *wrr;

	/* our options */
	sumux_opt_t opts;
};

struct sumux_opt_s {
	const char **infiles;
	const char *outfile;
	const char *badfile;
	const char *sname;
	void(*muxf)(mux_ctx_t);
	const char *zone;
	/** offset for timestamps relative to something other than epoch */
	int32_t tsoff;
	/** tick type as in sl1t.h */
	uint16_t tt;
	/** multiplier for reduced, down-scaled or point-less values */
	int32_t mul;
	/** magnifier for expanded or up-scaled values */
	int32_t mag;
};

extern void ariva_slab(mux_ctx_t);
extern void ibrti_slab(mux_ctx_t);
extern void dukas_slab(mux_ctx_t);
extern void gesmes_slab(mux_ctx_t);
extern void tfraw_slab(mux_ctx_t);
extern void sl1t_mux(mux_ctx_t);

#endif	/* INCLUDED_sumux_h_ */
