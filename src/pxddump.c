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

#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <sys/types.h>

#include <WinSock2.h>
#include <Windows.h>

#include "png.h"
#include "zlib.h"
#include "pxddump.h"

/**
 * Function implementation.
 */

FILE* pxd_get_temp_file() {
	DWORD dwRetVal = 0;
	UINT uRetVal = 0;
	FILE* temp_file;
	char temp_path[MAX_PATH];
	char temp_file_name[MAX_PATH];

	// Obtain the TEMP path.
	dwRetVal = GetTempPathA(MAX_PATH, temp_path);
	if (dwRetVal > MAX_PATH || (dwRetVal == 0)) {
		return NULL;
	}

	// Get a temporary file name.
	uRetVal = GetTempFileNameA(temp_path, "pxd", 0, temp_file_name);
	if (uRetVal == 0) {
		return NULL;
	}

	// Open the file.
	temp_file = fopen(temp_file_name, "wb+");
	return temp_file;
}

int pxd_inflate(FILE* pxd_file, FILE* file) {
	int retval;
	unsigned have;
	z_stream stream;
	unsigned char in[PXD_INFLATE_CHUNK_LEN];
	unsigned char out[PXD_INFLATE_CHUNK_LEN];

	// Allocate inflate state.
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	stream.avail_in = 0;
	stream.next_in = Z_NULL;
	retval = inflateInit(&stream);
	if (retval != Z_OK) {
		Z_ERRNO;
	}

	// Decompress until deflate stream ends or EOF.
	do {
		stream.avail_in = fread(in, 1, PXD_INFLATE_CHUNK_LEN, pxd_file);
		if (ferror(pxd_file)) {
			inflateEnd(&stream);
			return Z_ERRNO;
		}

		if (stream.avail_in == 0) {
			break;
		}

		stream.next_in = in;

		// Run inflate() on input until output buffer not full.
		do {
			stream.avail_out = PXD_INFLATE_CHUNK_LEN;
			stream.next_out = out;
			retval = inflate(&stream, Z_NO_FLUSH);
			assert(retval != Z_STREAM_ERROR);

			switch (retval) {
			case Z_NEED_DICT:
				retval = Z_DATA_ERROR; // Fall through.
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				inflateEnd(&stream);
				return retval;
			}

			have = PXD_INFLATE_CHUNK_LEN - stream.avail_out;
			if (fwrite(out, 1, have, file) != have || ferror(file)) {
				inflateEnd(&stream);
				return Z_ERRNO;
			}
		} while (stream.avail_out == 0);

		// Done when inflate() says it's done.
	} while (retval != Z_STREAM_END);

	// Clean up and return.
	inflateEnd(&stream);
	return retval == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

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

	if (fread(layer->data, sizeof(char), layer->header.data_len, file) < 1) {
		return PXD_EXIT_LAYER_DATA_READ_ERROR;	
	}

	// Seek to end of layer.
	layer_offset = layer_offset + sizeof(layer->header.layer_len) + layer->header.layer_len;
	if (fseek(file, layer_offset, SEEK_SET) < 0) {
		return PXD_EXIT_LAYER_HEADER_READ_ERROR;
	}

	return PXD_EXIT_OK;
}

void pxd_export_layer(struct pxd_layer* layer) {
	 FILE* exported;
	 char* exported_name;
	 png_structp png_ptr;
	 png_infop png_info_ptr;
	 char** png_rows;
	 int i;

	 // Prepare image data into rows.
	 png_rows = (char**) malloc(layer->header.height * sizeof(char*));
	 if (png_rows == NULL) {
		printf("Error initializing PNG rows!\n");
		return;
	 }

	 for (i = 0; i < layer->header.height; i++) {
		// Assume 4 bytes per pixel.
		png_rows[i] = layer->data + (i * layer->header.width) * 4;
	 }
	 
	 // Open a file to save the layer.
	 exported_name = (char*) malloc(strlen(layer->header.name) + 4 + 1);
	 sprintf(exported_name, "%s.png", layer->header.name);
	 exported = fopen(exported_name, "wb");
	 if (exported == NULL) {
		 printf("Error exporting layer: %s\n", layer->header.name);
		 return;
	 }

	 // Initialize PNG structures.
	 png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	 if (png_ptr == NULL) {
		 printf("Error initializing PNG structure!\n");
		 return;
	 }

	 png_info_ptr = png_create_info_struct(png_ptr);
	 if (png_info_ptr == NULL) {
		 printf("Error initializing PNG info structure!\n");
		 png_destroy_write_struct(&png_ptr, NULL);
		 return;
	 }

	 // Initialize I/O.
	 png_init_io(png_ptr, exported);
	 
	 // Set PNG file parameters.
	 png_set_IHDR(png_ptr, png_info_ptr, layer->header.width, layer->header.height,
		 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	 png_set_rows(png_ptr, png_info_ptr, (png_bytepp) png_rows);

	 // Write the file.
	 png_write_png(png_ptr, png_info_ptr, PNG_TRANSFORM_SWAP_ALPHA, NULL);
	 png_write_end(png_ptr, png_info_ptr);

	 // Free up resources.
	 png_destroy_write_struct(&png_ptr, &png_info_ptr);
}

/**
 * Main entry point.
 */

int main(int argc, char* argv[]) {
	FILE* pxd_file;
	FILE* file;
	struct pxd_header header;
	struct pxd_layer layer;
	int retval;
	int i;

	if (argc != 2) {
		printf("Usage: ./pxddump pxd_file\n");
		return PXD_EXIT_WRONG_ARGS;
	}

	pxd_file = fopen(argv[1], "rb");
	if (pxd_file == NULL) {
		printf("Error opening file for reading!\n");
		return PXD_EXIT_FILE_OPEN_ERROR;
	}
	
	// Inflate the file.
	file = pxd_get_temp_file();
	if (file == NULL) {
		printf("Error opening temporary file!\n");
		return PXD_EXIT_TEMP_FILE_OPEN_ERROR;
	}

	if (pxd_inflate(pxd_file, file) != Z_OK) {
		printf("Error inflating file!\n");
		return PXD_EXIT_FILE_INFLATE_ERROR;
	} else {
		fseek(file, 0, SEEK_SET);
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
			} else if (retval == PXD_EXIT_LAYER_DATA_READ_ERROR) {
				printf("Failed reading layer data!\n");
			}

			return retval;
		}

		// Print information about the layer.
		printf("Layer found: %s\n", layer.header.name);
		printf("Layer width: %d\n", layer.header.width);
		printf("Layer height: %d\n", layer.header.height);

		// Write layer to png.
		pxd_export_layer(&layer);
	}

	return 0;
}