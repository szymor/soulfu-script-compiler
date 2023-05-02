#ifndef _H_SOULFU_SCRIPT
#define _H_SOULFU_SCRIPT

enum SSError
{
	SSE_NONE,
	SSE_EXTERNAL,
	SSE_HEADERIZE
};

struct Buffer
{
	unsigned char *mem;
	unsigned int used;
	unsigned int max;
};

enum SSError src_headerize(struct Buffer *script, struct Buffer *run);

#endif
