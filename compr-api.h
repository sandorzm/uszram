#ifndef COMPR_API_H
#define COMPR_API_H


#include "uszram-page.h"


/* is_huge() returns 1 if pg->data is formatted as a compressed page, otherwise
 * 0 (it's just raw data).
 */
inline static _Bool is_huge(struct page *pg);

/* get_size() returns the size in bytes of all data buffers allocated to
 * pg (not including the size of struct page) or zero if none are.
 */
inline static size_type get_size(struct page *pg);

/* compress() compresses the page in src into dest. Returns 0 if compression to
 * the huge page threshold (MAX_NON_HUGE) failed, otherwise the size of the page
 * data in dest.
 */
static size_type compress(const char src[static USZRAM_PAGE_SIZE],
			  char dest[static MAX_NON_HUGE]);

/* decompress() decompresses 'bytes' bytes from pg->data into dest. dest must be
 * allocated with at least 'bytes' bytes; pg->data must be formatted as a
 * compressed page. Returns a negative error code if pg->data has bad format,
 * otherwise 0.
 */
static int decompress(struct page *pg, char *dest, size_type bytes);

/* write_compressed() writes 'bytes' bytes from src into pg->data, updating any
 * necessary metadata. Both src and pg->data must be allocated with at least
 * 'bytes' bytes.
 */
static void write_compressed(struct page *pg, const char *src, size_type bytes);

/* read_modify() updates 'blocks' blocks starting at 'offset' bytes from the
 * beginning of the page represented by pg with new_data. If pg->data is a raw
 * page, it's updated in place; otherwise, it's decompressed first. Any negative
 * error code from decompress() is returned.
 *
 * 'blocks' can't be zero, new_data must be at least 'blocks' times the block
 * size, and the range specified by 'offset' and 'blocks' must be within the
 * page size. Unless pg->data is raw, raw_pg must be allocated and at least
 * the page size.
 *
 * If the page needs to be recompressed, returns 1, and writes the full raw page
 * to raw_pg if it wasn't updated in place; otherwise returns 0 or a negative
 * error code.
 */
static int read_modify(struct page *pg,
		       const char new_data[static USZRAM_BLOCK_SIZE],
		       size_type offset, size_type blocks, char *raw_pg);


#endif // COMPR_API_H
