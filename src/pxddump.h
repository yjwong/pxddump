/**
 * pxddump - Dumps layers contained within PXD files
 * PXD files are produced by the Flash-based Pixlr Editor.
 *
 * Copyright (C) 2014 Wong Yong Jie
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT ANY WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PXDDUMP_H
#define PXDDUMP_H

#include <stdio.h>

/**
 * Compile-time constants.
 */

static const int PXD_EXIT_OK = 0;
static const int PXD_EXIT_WRONG_ARGS = 1;
static const int PXD_EXIT_FILE_OPEN_ERROR = 2;
static const int PXD_EXIT_FILE_INFLATE_ERROR = 3;
static const int PXD_EXIT_TEMP_FILE_OPEN_ERROR = 4;
static const int PXD_EXIT_FILE_HEADER_READ_ERROR = 5;
static const int PXD_EXIT_LAYER_HEADER_READ_ERROR = 6;
static const int PXD_EXIT_LAYER_NAME_ALLOC_ERROR = 7;
static const int PXD_EXIT_LAYER_NAME_READ_ERROR = 8;
static const int PXD_EXIT_LAYER_DATA_ALLOC_ERROR = 9;
static const int PXD_EXIT_LAYER_DATA_READ_ERROR = 10;

#define PXD_INFLATE_CHUNK_LEN 4096

/**
 * Data structures.
 */

// Header for a PXD file.
struct pxd_header {
	uint32_t version;
	uint32_t width;
	uint32_t height;
	uint16_t num_layers;
	uint32_t file_len;
};

// Layer header.
struct pxd_layer_header {
	uint32_t layer_len;
	uint16_t name_len;
	char* name;
	uint32_t unknown0;
	uint32_t unknown1;
	uint32_t width;
	uint32_t height;
	uint32_t unknown2;
	uint32_t data_len;
};

// The layer itself.
struct pxd_layer {
	struct pxd_layer_header header;
	char* data;
};

/**
 * Function prototypes.
 */

FILE* pxd_get_temp_file();
int pxd_inflate(FILE* pxd_file, FILE* file);
int pxd_read_header(FILE* file, struct pxd_header* header);
int pxd_read_layer(FILE* file, struct pxd_layer* layer);
void pxd_export_layer(struct pxd_layer* layer);

#endif /* PXDDUMP_H */