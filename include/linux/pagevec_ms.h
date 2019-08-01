#ifndef _LINUX_PAGEVEC_MS_H
#define _LINUX_PAGEVEC_MS_H

extern bool lru_ms_registered;
extern DEFINE_PER_CPU(struct pagevec_ms, lru_add_ms);

struct lru_ms_callback {
	void (*__lru_cache_add)(struct page *page);
	void (*lru_add_drain_cpu)(int cpu);
};

extern struct lru_ms_callback lru_ms_callback;
void register_lru_ms_callback(struct lru_ms_callback *cb);
void unregister_lru_ms_callback(void);

/*
 * HACK!
 *
 * For now, you must recompile kernel if you change this.
 * Because the percpu array is defined in kernel..
 * 
 * A better way, is to allocate during module install. Later.
 */
#define NR_PAGEVEC_MS 128

struct pagevec_ms {
	/*
	 * Head is the current available spot
	 */
	unsigned int head, tail;
	struct page *pages[NR_PAGEVEC_MS];
};

static inline bool pagevec_ms_has_page(struct pagevec_ms *p)
{
	if (likely(p->head > p->tail))
		return true;
	return false;
}

static inline unsigned int pagevec_ms_nr_pages(struct pagevec_ms *p)
{
	return p->head - p->tail;
}

static inline bool pagevec_ms_full(struct pagevec_ms *p)
{
	unsigned int nr;

	nr = pagevec_ms_nr_pages(p);
	if (likely(nr >= NR_PAGEVEC_MS)) {
		if (likely(nr == NR_PAGEVEC_MS))
			return true;
		BUG();
	}
	return false;
}

static inline void
pagevec_ms_enqueue(struct pagevec_ms *pvec, struct page *page)
{
	unsigned int idx;

	idx = pvec->head % NR_PAGEVEC_MS;
	pvec->head++;
	pvec->pages[idx] = page;
}

/*
 * Return the tail page
 * Do empty check before calling this function.
 */
static inline struct page *
pagevec_ms_dequeue(struct pagevec_ms *pvec)
{
	unsigned int idx;

	idx = pvec->tail % NR_PAGEVEC_MS;
	pvec->tail++;
	return pvec->pages[idx];
}

#endif
