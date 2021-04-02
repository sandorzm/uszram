#ifndef COMPR_API_H
#define COMPR_API_H


#include "uszram-page.h"


/* is_huge() returns 0 if pg->data is NULL or formatted as a compressed page,
 * otherwise 1 (pg is huge; i.e., pg->data is just raw data).
 */
inline static _Bool is_huge(struct page *pg);

/* get_size() returns the number of bytes of heap data representing pg.
 */
inline static size_type get_size(struct page *pg);

/* get_size_primary() is like get_size() but counts only the heap data allocated
 * to pg->data, not anything else reachable from it.
 */
inline static size_type get_size_primary(struct page *pg);

/* free_reachable() deallocates any heap data reachable from pg->data, but not
 * pg->data itself. pg must not be huge. Returns the number of bytes
 * deallocated.
 */
inline static size_type free_reachable(struct page *pg);

/* compress() compresses the page in src into dest. Returns 0 if compression to
 * the huge page threshold MAX_NON_HUGE failed, otherwise the size of the
 * compressed data in dest.
 */
inline static size_type compress(const char src[static USZRAM_PAGE_SIZE],
				 char dest[static MAX_NON_HUGE]);

/* decompress() decompresses 'bytes' bytes from pg->data into dest. Returns a
 * negative error code if pg->data isn't formatted as a compressed page,
 * otherwise 0.
 */
inline static int decompress(struct page *pg, size_type bytes,
			     char dest[static bytes]);

/* write_compressed() writes 'bytes' bytes from src into pg->data, updating any
 * necessary metadata. If 'bytes' is zero, updates the metadata to reflect that
 * pg->data is now empty. Otherwise, src and pg->data must be allocated with at
 * least 'bytes' bytes.
 */
inline static void write_compressed(struct page *pg, size_type bytes,
				    const char *src);

/* read_modify() updates 'blocks' blocks starting at 'offset' blocks from the
 * beginning of the page represented by pg with new_data. If pg is huge, it's
 * updated in place; otherwise, it's decompressed first, and any negative error
 * code from decompress() is returned.
 *
 * blocks can't be zero, and the range specified by offset and blocks must be
 * within the page size. Unless pg is huge, raw_pg and size_change must be
 * non-null and raw_pg must be at least the page size.
 *
 * If the page needs to be recompressed, returns 1, and writes the full raw page
 * to raw_pg if pg isn't huge; otherwise returns 0 or a negative error code. If
 * 0 is returned and pg isn't huge, sets *size_change to the change in size of
 * any heap allocations including pg->data and those reachable from it.
 */
static int read_modify(struct page *pg, size_type offset, size_type blocks,
		       const char new_data[static blocks * USZRAM_BLOCK_SIZE],
		       char *raw_pg, int *size_change);


#endif // COMPR_API_H
