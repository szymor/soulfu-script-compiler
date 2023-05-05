#ifndef _H_SOULFU_SCRIPT
#define _H_SOULFU_SCRIPT

enum SSError
{
	SSE_NONE,
	SSE_EXTERNAL,
	SSE_HEADERIZE,
	SSE_COMPILERIZE,
	SSE_END
};

struct Buffer
{
	unsigned char *mem;
	unsigned int used;
	unsigned int max;
};

enum SSError src_headerize(struct Buffer *script, struct Buffer *run);
enum SSError src_compilerize(struct Buffer *script, struct Buffer *run);

#endif
