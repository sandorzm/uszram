#ifndef COMPR_API_H
#define COMPR_API_H


#include "uszram-page.h"


/* is_huge() returns 1 if pg->data is formatted as a compressed page, otherwise
 * 0 (it's just raw data).
 */
inline static _Bool is_huge(struct page *pg);

/* get_size() returns the number of bytes of heap data representing pg.
 */
inline static size_type get_size(struct page *pg);

/* free_reachable() deallocates any heap data reachable from pg->data, but not
 * pg->data itself.
 */
inline static void free_reachable(struct page *pg);

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
 * necessary metadata. src and pg->data must be allocated with at least 'bytes'
 * bytes.
 */
inline static void write_compressed(struct page *pg, size_type bytes,
				    const char src[static bytes]);

/* read_modify() updates 'blocks' blocks starting at 'offset' bytes from the
 * beginning of the page represented by pg with new_data. If pg->data is a raw
 * page, it's updated in place; otherwise, it's decompressed first, returning
 * any negative error code from decompress().
 *
 * blocks can't be zero, and the range specified by offset and blocks must be
 * within the page size. Unless pg->data is raw, raw_pg must be non-null and at
 * least the page size.
 *
 * If the page needs to be recompressed, returns 1, and writes the full raw page
 * to raw_pg if pg->data isn't raw; otherwise returns 0 or a negative error
 * code.
 */
static int read_modify(struct page *pg, size_type offset, size_type blocks,
		       const char new_data[static blocks * USZRAM_BLOCK_SIZE],
		       char *raw_pg);


#endif // COMPR_API_H
