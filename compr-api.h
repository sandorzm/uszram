#ifndef COMPR_API_H
#define COMPR_API_H


#include "uszram-def.h"


struct page;

/* is_huge() returns 0 if pg->data is NULL or formatted as a compressed page,
 * otherwise 1 (pg is huge; i.e., pg->data is just raw data).
 */
static inline _Bool is_huge(const struct page *pg);

/* get_size() returns the number of bytes of heap data representing pg.
 */
static inline size_type get_size(const struct page *pg);

/* get_size_primary() is like get_size() but counts only the heap data allocated
 * to pg->data, not anything else reachable from it.
 */
static inline size_type get_size_primary(const struct page *pg);

/* free_reachable() deallocates any heap data reachable from pg->data, but not
 * pg->data itself. pg must not be huge. Returns the number of bytes
 * deallocated.
 */
static inline size_type free_reachable(const struct page *pg);

/* compress() compresses the page in src into dest. Returns 0 if compression to
 * the huge page threshold MAX_NON_HUGE failed, otherwise the size of the
 * compressed data in dest.
 */
static inline size_type compress(const char src[static PAGE_SIZE],
				 char dest[static MAX_NON_HUGE]);

/* decompress() decompresses 'bytes' bytes from pg->data into dest. Returns a
 * negative error code if pg->data isn't formatted as a compressed page,
 * otherwise 0.
 */
static inline int decompress(const struct page *pg, size_type bytes,
			     char dest[static PAGE_SIZE]);

/* write_compressed() writes 'bytes' bytes from src into pg->data, updating any
 * necessary metadata. If 'bytes' is zero, updates the metadata to reflect that
 * pg->data is now empty. Otherwise, src and pg->data must be allocated with at
 * least 'bytes' bytes.
 */
static inline void write_compressed(struct page *pg, size_type bytes,
				    const char *src);

/* needs_recompress() does bookkeeping to reflect 'blocks' block updates to pg.
 * pg must be huge. If pg has accumulated USZRAM_HUGE_WAIT updates (including
 * 'blocks') since compression, returns 1. Otherwise, updates pg's update
 * counter and returns 0.
 */
static inline _Bool needs_recompress(struct page *pg, size_type blocks);

/* read_modify() decompresses and updates each range in 'ranges' of pg with data
 * from new_data. 'ranges' must be disjoint and all fit within a page. If
 * decompress() fails, the error code is returned.
 *
 * pg must not be huge, 'ranges' must have at least range_count elements, and
 * new_data must have at least as many blocks of data as the sum of all
 * ranges[i].count.
 *
 * If pg needs to be recompressed, returns 1 and writes the full raw page to
 * raw_pg. Otherwise, returns 0 or a negative error code, leaving any metadata
 * in pg in a consistent state.
 */
static inline int read_modify(const struct page *pg,
			      unsigned char range_count,
			      const BlkRange *ranges,
			      char raw_pg[static PAGE_SIZE],
			      const char *new_data);

/* read_modify_hint() is like read_modify() but accepts the parameter old_data,
 * a hint that must contain the original, unmodified versions of the regions of
 * pg to be updated (i.e., the original version of new_data). It may allow
 * skipping decompression.
 */
static inline int read_modify_hint(const struct page *pg,
				   unsigned char range_count,
				   const BlkRange *ranges,
				   char raw_pg[static PAGE_SIZE],
				   const char *new_data,
				   const char *old_data);

/* read_delete() is like read_modify() except that it sets the blocks to zero
 * instead of new data, and a return value of 0 means that pg is now empty (all
 * zeros) and can be deallocated. pg always needs to be recompressed.
 */
static inline int read_delete(const struct page *pg,
			      unsigned char range_count,
			      const BlkRange *ranges,
			      char raw_pg[static PAGE_SIZE]);


#endif // COMPR_API_H
