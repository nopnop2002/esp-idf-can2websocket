#define CMD_CAPTURE  100
#define CMD_VIEW	 200
#define CMD_HIDE	 220
#define CMD_BINARY	 500
#define CMD_OCTAL	 520
#define CMD_DECIMAL	 540
#define CMD_HEXA	 560
#define CMD_HALT	 400

typedef struct {
	uint16_t command;
	portTickType now;
	char topic[64];
	int16_t topic_len;
	int32_t canid;
	int16_t ext;
	int16_t rtr;
	int16_t data_len;
	char data[8];
} FRAME_t;

typedef struct {
	uint16_t frame;
	uint32_t canid;
	char * topic;
	int16_t topic_len;
} TOPIC_t;

typedef struct {
	int head; // current read pointer
	int tail; // next write pointer
	int count; // current data count
	int size; // total data count
	FRAME_t *data;
} QUEUE_t;

