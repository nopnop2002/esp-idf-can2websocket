#define STANDARD_FRAME 0
#define EXTENDED_FRAME 1

#define MESSAGE_FRAME  0
#define REMOTE_FRAME   1

#define FORMAT_BINARY  31
#define FORMAT_OCTAL   32
#define FORMAT_DECIMAL 33
#define FORMAT_HEXADECIMAL 34

typedef struct {
	char *name;
	int16_t name_len;
	int32_t canid;
	int16_t ext;
	int16_t rtr;
	int16_t data_len;
	int16_t data[8];
} FRAME_t;

typedef struct {
	uint16_t frame;
	uint32_t canid;
	char * name;
	int16_t name_len;
} TOPIC_t;

