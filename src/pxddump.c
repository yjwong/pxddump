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

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <WinSock2.h>

#include "pxddump.h"

/**
 * Function implementation.
 */

int pxd_read_header(FILE* file, struct pxd_header* header) {
	if (fread(&header->version, sizeof(header->version), 1, file) < 1 ||
		fread(&header->width, sizeof(header->width), 1, file) < 1 ||
		fread(&header->height, sizeof(header->height), 1, file) < 1 ||
		fread(&header->num_layers, sizeof(header->num_layers), 1, file) < 1 ||
		fread(&header->file_len, sizeof(header->file_len), 1, file) < 1) {
		return PXD_EXIT_FILE_HEADER_READ_ERROR;
	} else {
		header->version = ntohl(header->version);
		header->width = ntohl(header->width);
		header->height = ntohl(header->height);
		header->num_layers = ntohs(header->num_layers);
		header->file_len = ntohl(header->file_len);
		return PXD_EXIT_OK;
	}
}

int pxd_read_layer(FILE* file, struct pxd_layer* layer) {
	// Save where we are.
	off_t layer_offset = ftell(file);
	if (layer_offset < 0) {
		return PXD_EXIT_LAYER_HEADER_READ_ERROR;
	}

	// Read the layer header.
	if (fread(&layer->header.layer_len, sizeof(layer->header.layer_len), 1, file) < 1 ||
		fread(&layer->header.name_len, sizeof(layer->header.name_len), 1, file) < 1) {
		return PXD_EXIT_LAYER_HEADER_READ_ERROR;
	}

	// Do some byte order flipping.
	layer->header.layer_len = ntohl(layer->header.layer_len);
	layer->header.name_len = ntohs(layer->header.name_len) + 1;

	// Read the full name.
	layer->header.name = (char*) malloc(layer->header.name_len);
	if (layer->header.name == NULL) {
		return PXD_EXIT_LAYER_NAME_ALLOC_ERROR;
	}

	if (fread(layer->header.name, sizeof(char), layer->header.name_len, file) < 1) {
		return PXD_EXIT_LAYER_NAME_READ_ERROR;
	}

	// Read the rest of the struct.
	if (fread(&layer->header.unknown0, sizeof(layer->header.unknown0), 1, file) < 1 ||
		fread(&layer->header.unknown1, sizeof(layer->header.unknown1), 1, file) < 1 ||
		fread(&layer->header.width, sizeof(layer->header.width), 1, file) < 1 ||
		fread(&layer->header.height, sizeof(layer->header.height), 1, file) < 1 ||
		fread(&layer->header.unknown2, sizeof(layer->header.unknown2), 1, file) < 1 ||
		fread(&layer->header.data_len, sizeof(layer->header.data_len), 1, file) < 1) {
		return PXD_EXIT_LAYER_HEADER_READ_ERROR;
	}
	
	layer->header.width = ntohl(layer->header.width);
	layer->header.height = ntohl(layer->header.height);
	layer->header.data_len = ntohl(layer->header.data_len);

	// Read the actual data.
	layer->data = (char*) malloc(layer->header.data_len);
	if (layer->data == NULL) {
		return PXD_EXIT_LAYER_DATA_ALLOC_ERROR;
	}

	fread(layer->data, sizeof(char), layer->header.data_len, file);

	// Seek to end of layer.
	layer_offset = layer_offset + sizeof(layer->header.layer_len) + layer->header.layer_len;
	if (fseek(file, layer_offset, SEEK_SET) < 0) {
		return PXD_EXIT_LAYER_HEADER_READ_ERROR;
	}

	return PXD_EXIT_OK;
}

/**
 * Main entry point.
 */

int main(int argc, char* argv[]) {
	FILE* file;
	struct pxd_header header;
	struct pxd_layer layer;
	int retval;
	int i;

	file = fopen("D:/Dropbox/Ingress/Westies Logo/Ver4.dat", "rb");
	if (file == NULL) {
		printf("Error opening file for reading!\n");
		return PXD_EXIT_FILE_OPEN_ERROR;
	}

	// Read the dimensions of the image.
	if (pxd_read_header(file, &header) > 0) {
		printf("Failed reading header of file!\n");
		return PXD_EXIT_FILE_HEADER_READ_ERROR;
	}

	printf("Image width: %d\n", header.width);
	printf("Image height: %d\n", header.height);
	printf("Number of layers: %d\n", header.num_layers);
	printf("File length: %d bytes\n", header.file_len);

	// Read each layer.
	for (i = 0; i < header.num_layers; i++) {
		retval = pxd_read_layer(file, &layer);
		if (retval > 0) {
			if (retval == PXD_EXIT_LAYER_HEADER_READ_ERROR) {
				printf("Failed reading layer header!\n");
			} else if (retval == PXD_EXIT_LAYER_NAME_ALLOC_ERROR) {
				printf("Failed allocating memory for layer name!\n");
			} else if (retval == PXD_EXIT_LAYER_NAME_READ_ERROR) {
				printf("Failed reading layer name!\n");
			} else if (retval == PXD_EXIT_LAYER_DATA_ALLOC_ERROR) {
				printf("Memory allocation failure while reading layer data!\n");
			}

			return retval;
		}

		// Print information about the layer.
		printf("Layer found: %s\n", layer.header.name);
		printf("Layer width: %d\n", layer.header.width);
		printf("Layer height: %d\n", layer.header.height);
	}

	return 0;
}