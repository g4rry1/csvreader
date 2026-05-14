#pragma once

#define ERR_OK     0  /* success */
#define ERR_EOF    1  /* end of input (internal to parse_data_line) */
#define ERR_MEMORY 2  /* malloc/realloc failed */
#define ERR_PARSE  3  /* invalid CSV format or data */
#define ERR_EVAL   4  /* evaluation error (cycle, division by zero, etc.) */
