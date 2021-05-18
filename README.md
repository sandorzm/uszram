# uszram
uszram is an in-memory compressed [RAM
drive](https://en.wikipedia.org/wiki/RAM_drive) suitable for use in in-memory
databases. It can be extended to use any compression library, including hardware
compression, enabling higher data capacity and more freedom in designing data
structures. It is especially designed for use with the [Z
API](https://github.com/Mjdgithuber/Z_API) compressor, which can quickly update
small parts of a compressed unit of data without recompressing the whole page.
uszram is named after the Linux kernel module
[zram](https://en.wikipedia.org/wiki/Zram) but is not part of the kernel and
thus can be more easily modified and configured.

## Documentation
uszram's API is declared and documented in `uszram.h`, along with most
configuration options. The private APIs for compressors, allocators, locks, and
caching strategies are documented in `*-api.h` for purposes of extending uszram.

## Building
`uszram.c` is the only translation unit needed to compile the uszram library. To
build performance tests, compile it along with `test/workload.c` and
`test/test-utils.c`. Follow the separate instructions for linking in your chosen
compression library.
